#include "win10_versiondb_manager.h"
#include <playapi/util/config.h>
#include <fstream>
#include <sys/stat.h>
#include <nlohmann/json.hpp>
#include <regex>

Win10VersionDBManager::Win10VersionDBManager() {
    playapi::config conf;
    std::ifstream ifs("priv/win10_git.conf");
    conf.load(ifs);

    sshPubkeyPath = conf.get("ssh.pubkey");
    sshPrivkeyPath = conf.get("ssh.privkey");
    sshPassphrase = conf.get("ssh.passphrase");

    userName = conf.get("user.name");
    userEmail = conf.get("user.email");

    struct stat st;
    if (stat(dir.c_str(), &st) != 0 || ((st.st_mode) & S_IFMT) != S_IFDIR) {
        // clone
        git_clone_options opts = GIT_CLONE_OPTIONS_INIT;
        opts.fetch_opts.callbacks.credentials = createCredentials;
        opts.fetch_opts.callbacks.payload = this;
        printf("Cloning win10 versiondb into %s\n", conf.get("url").c_str());
        if (git_clone(repo, conf.get("url").c_str(), dir.c_str(), &opts) != 0)
            throw GitError("git_clone");
        printf("Cloning done\n");
    } else {
        if (git_repository_init(repo, dir.c_str(), 0) != 0)
            throw GitError("git_repository_init");
    }

    Win10VersionTextDb textDb;
    textDb.read(dir + "versions.txt");
    textDb.write(dir + "versions.txt");
    textDb.writeJson(dir + "versions.json.min");
}

void Win10VersionDBManager::commitDb(std::string const& commitName) {
    GitTreeBuilder bld;
    GitTree parentTree, builtTree;
    git_oid obj; // reused for multiple purposes

    GitCommit parent;
    if (git_reference_name_to_id(&obj, repo, "HEAD") || git_commit_lookup(parent, repo, &obj))
        throw GitError("git_reference_name_to_id/git_commit_lookup");
    if (git_commit_tree(parentTree, parent))
        throw GitError("git_commit_tree");

    if (git_treebuilder_new(bld, repo, parentTree))
        throw GitError("git_treebuilder_new");
    for (std::string const& file : {"versions.txt", "versions.json.min"}) {
        if (git_blob_create_fromworkdir(&obj, repo, file.c_str()))
            throw GitError("git_blob_create_fromdisk");
        if (git_treebuilder_insert(nullptr, bld, file.c_str(), &obj, GIT_FILEMODE_BLOB))
            throw GitError("git_treebuilder_insert");
    }
    if (git_treebuilder_write(&obj, bld))
        throw GitError("git_treebuilder_write");
    bld.reset();
    if (git_tree_lookup(builtTree, repo, &obj))
        throw GitError("git_tree_lookup");

    GitSignature sig;
    if (git_signature_now(sig, userName.c_str(), userEmail.c_str()))
        throw GitError("git_signature_new");
    if (git_commit_create_v(&obj, repo, "HEAD", sig, sig, nullptr, commitName.c_str(), builtTree, 1, parent.ptr))
        throw GitError("git_commit_create_v");
}

void Win10VersionDBManager::pushDb() {
    GitRemote  remote;
    if (git_remote_lookup(remote, repo, "origin"))
        throw GitError("git_remote_lookup");
    git_remote_callbacks cbs = GIT_REMOTE_CALLBACKS_INIT;
    cbs.credentials = createCredentials;
    cbs.payload = this;
    if (git_remote_connect(remote, GIT_DIRECTION_PUSH, &cbs, nullptr, nullptr))
        throw GitError("git_remote_connect");
    git_push_options pushOpt = GIT_PUSH_OPTIONS_INIT;
    pushOpt.callbacks = cbs;
    const char* refs_text[] = { "refs/heads/master:refs/heads/master" };
    git_strarray refs = {(char**) refs_text, 1};
    if (git_remote_push(remote, &refs, &pushOpt))
        throw GitError("git_remote_push");
}

int Win10VersionDBManager::createCredentials(git_cred **cred, const char *url, const char *username_from_url,
                                             unsigned int allowed_types, void *payload) {
    if (!(allowed_types & GIT_CREDTYPE_SSH_KEY)) {
        return 1;
    }
    Win10VersionDBManager* th = (Win10VersionDBManager*) payload;
    int ret = git_cred_ssh_key_new(cred, username_from_url, th->sshPubkeyPath.c_str(), th->sshPrivkeyPath.c_str(),
            th->sshPassphrase.c_str());
    if (ret != 0)
        printf("git_cred_ssh_key_new failed: %s\n", giterr_last()->message);
    return ret;
}

void Win10VersionDBManager::addWin10StoreMgr(Win10StoreManager &mgr) {
    using namespace std::placeholders;
    mgr.addNewVersionCallback(std::bind(&Win10VersionDBManager::onNewWin10Version, this, _1, _2));
}

void Win10VersionDBManager::onNewWin10Version(std::vector<Win10StoreNetwork::UpdateInfo> const &u, bool isBeta) {
    if (u.empty())
        return;
    Win10VersionTextDb textDb;
    textDb.read(dir + "versions.txt");
    auto& textList = isBeta ? textDb.betaList : textDb.releaseList;
    for (auto const& v : u)
        textList.push_back({v.updateId, v.packageMoniker, v.serverId});
    textDb.write(dir + "versions.txt");
    textDb.writeJson(dir + "versions.json.min");
    std::stringstream commitName;
    commitName << "Minecraft ";
    auto ver = Win10VersionTextDb::convertVersion(u[0].packageMoniker);
    commitName << ver.major << "." << ver.minor << "." << ver.patch << "." << ver.revision;
    if (isBeta)
        commitName << " (Beta)";
    commitDb(commitName.str());
    pushDb();
}

void Win10VersionTextDb::read(std::string const &filePath) {
    std::ifstream ifs(filePath);
    std::string line;
    bool isBeta = false;
    while (std::getline(ifs, line)) {
        if (line == "Beta")
            isBeta = true;
        auto iof = line.find(' ');
        if (iof == std::string::npos)
            continue;
        auto iof2 = line.find(' ', iof + 1);
        VersionInfo vi = {line.substr(0, iof), line.substr(iof + 1, iof2 != std::string::npos ? iof2 - iof - 1 : iof2)};
        if (iof2 != std::string::npos)
            vi.serverId = line.substr(iof2 + 1);
        (isBeta ? betaList : releaseList).push_back(std::move(vi));
    }
}

void Win10VersionTextDb::write(std::string const &filePath) {
    std::ofstream ofs(filePath);
    ofs << "Releases\n";
    for (auto const& v : releaseList)
        ofs << v.uuid << " " << v.fileName << (v.serverId.empty() ? "" : " ") << v.serverId << "\n";
    ofs << "\n";
    ofs << "Beta\n";
    for (auto const& v : betaList)
        ofs << v.uuid << " " << v.fileName << (v.serverId.empty() ? "" : " ") << v.serverId << "\n";
    ofs << "\n";
}

void Win10VersionTextDb::writeJson(std::string const &filePath) {
    nlohmann::json j = nlohmann::json::array();
    struct Element {
        Version version;
        std::string uuid;
        bool isBeta;
    };
    std::vector<Element> elements;
    std::set<Version> addedVersions;
    auto addVersions = [&elements, &addedVersions](std::vector<VersionInfo> const& list, bool isBeta) {
        for (auto const& v : list) {
            auto cvVersion = convertVersion(v.fileName);
            if (v.fileName.find(".0_x64") != std::string::npos && addedVersions.count(cvVersion) == 0) {
                elements.push_back({cvVersion, v.uuid, isBeta});
                addedVersions.insert(cvVersion);
            }
        }
    };
    addVersions(releaseList, false);
    addVersions(betaList, true);
    std::sort(elements.begin(), elements.end(), [](Element const& a, Element const& b) {
        return a.version < b.version;
    });
    for (auto const& el : elements) {
        std::stringstream ver;
        ver << el.version.major << "." << el.version.minor << "." << el.version.patch << "." << el.version.revision;
        j.push_back({ver.str(), el.uuid, el.isBeta ? 1 : 0});
    }

    std::ofstream ofs(filePath);
    ofs << j;
}

Win10VersionTextDb::Version Win10VersionTextDb::convertVersion(std::string const &ver) {
    static std::regex regex (R"(Microsoft\.MinecraftUWP_([0-9]+)\.([0-9]+)\.([0-9]+)\..*__8wekyb3d8bbwe.*)");
    std::smatch match;
    if (!std::regex_match(ver, match, regex))
        throw std::runtime_error("regex_match failed");
    int major = std::stoi(match[1]);
    int minor = std::stoi(match[2]);
    int patch = std::stoi(match[3]);
    if (major == 0 && minor < 1000)
        return {major, minor / 10, minor % 10, patch};
    if (major == 0)
        return {major, minor / 100, minor % 100, patch};
    return {major, minor, patch / 100, patch % 100};
}