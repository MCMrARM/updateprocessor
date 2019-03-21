#include "win10_versiondb_manager.h"
#include <playapi/util/config.h>
#include <fstream>
#include <sys/stat.h>


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
    if (git_blob_create_fromworkdir(&obj, repo, "versions.json.min"))
        throw GitError("git_blob_create_fromdisk");
    if (git_treebuilder_insert(nullptr, bld, "versions.json.min", &obj, GIT_FILEMODE_BLOB))
        throw GitError("git_treebuilder_insert");
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
