#pragma once

#include <git2.h>
#include <string>
#include <stdexcept>
#include <mutex>
#include "win10_store_manager.h"

template <typename T, void FreeFunc(T*)>
struct GitPtr {
    T* ptr;
    GitPtr() : ptr(nullptr) {}
    GitPtr(T* ptr) : ptr(ptr) {}
    ~GitPtr() { if (ptr) FreeFunc(ptr); }
    void reset() { if (ptr) FreeFunc(ptr); ptr = nullptr; }

    operator T*() { return ptr; }
    operator T**() { return &ptr; }
    T* operator->() { return ptr; }
    T* get() { return ptr; }
    T** ref() { return &ptr; }
};
struct GitError : public std::runtime_error {
    GitError(const char* fun_name) : std::runtime_error(std::string("Git Error: ") + fun_name + ": " + giterr_last()->message) {
    }
};
using GitRepository = GitPtr<git_repository, git_repository_free>;
using GitSignature = GitPtr<git_signature, git_signature_free>;
using GitCommit = GitPtr<git_commit, git_commit_free>;
using GitTree = GitPtr<git_tree, git_tree_free>;
using GitTreeBuilder = GitPtr<git_treebuilder, git_treebuilder_free>;
using GitRemote = GitPtr<git_remote, git_remote_free>;


struct Win10VersionTextDb {
    struct Version {
        int major, minor, patch, revision;

        friend bool operator<(Version const& a, Version const& b) {
            return std::tie(a.major, a.minor, a.patch, a.revision) < std::tie(b.major, b.minor, b.patch, b.revision);
        }
    };
    struct VersionInfo {
        std::string uuid;
        std::string fileName;
        std::string serverId;
    };
    std::vector<VersionInfo> releaseList, betaList;

    static Version convertVersion(std::string const& ver);

    void read(std::string const& filePath);

    void write(std::string const& filePath);

    void writeJson(std::string const& filePath);
};

class Win10VersionDBManager {

private:
    const std::string dir = "priv/win10_verdb/";
    GitRepository repo;
    std::string sshPrivkeyPath;
    std::string sshPubkeyPath;
    std::string sshPassphrase;
    std::string userName;
    std::string userEmail;
    std::mutex fileLock;

    static int createCredentials(git_cred **cred, const char *url, const char *username_from_url,
            unsigned int allowed_types, void *payload);

    void commitDb(std::string const& commitName);

    void pushDb();

public:
    Win10VersionDBManager();

    void addWin10StoreMgr(Win10StoreManager& mgr);

    void onNewWin10Version(std::vector<Win10StoreNetwork::UpdateInfo> const& u, bool isBeta);

};