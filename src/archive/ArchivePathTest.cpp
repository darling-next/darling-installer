#include "ArchivePath.h"

#include <cassert>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static bool rejects(const std::string& path)
{
	try
	{
		installer_archive::normalizePath(path);
	}
	catch (const std::runtime_error&)
	{
		return true;
	}
	return false;
}

static bool rejectsParent(const std::string& path, const std::string& root,
	const installer_archive::CreatedSymlinks& created)
{
	try
	{
		installer_archive::ensureParentDirectories(path, 0755, root, created);
	}
	catch (const std::runtime_error&)
	{
		return true;
	}
	return false;
}

int main()
{
	assert(installer_archive::normalizePath("./Library//Developer") == "Library/Developer");
	assert(rejects("/Library/Developer"));
	assert(rejects("Library/../../outside"));

	const char* temporaryRoot = std::getenv("TMPDIR");
	std::string temporary = std::string(temporaryRoot ? temporaryRoot : "/tmp") +
		"/installer-archive-path-XXXXXX";
	assert(::mkdtemp(&temporary[0]) != nullptr);
	char canonical[PATH_MAX];
	assert(::realpath(temporary.c_str(), canonical) != nullptr);
	std::string root = canonical;
	std::string safe = root + "/safe";
	std::string outside = root + "/outside";
	assert(::mkdir(safe.c_str(), 0755) == 0);
	assert(::mkdir(outside.c_str(), 0755) == 0);
	installer_archive::CreatedSymlinks created;

	installer_archive::ensureParentDirectories(safe + "/new/file", 0755, safe, created);
	struct stat st;
	assert(::stat((safe + "/new").c_str(), &st) == 0 && S_ISDIR(st.st_mode));

	assert(::mkdir((safe + "/target").c_str(), 0755) == 0);
	assert(::symlink("target", (safe + "/inside").c_str()) == 0);
	std::string header = safe + "/inside/headers/witness";
	installer_archive::ensureParentDirectories(header, 0755, safe, created);
	FILE* file = std::fopen(header.c_str(), "w");
	assert(file != nullptr && std::fputs("header\n", file) >= 0);
	assert(std::fclose(file) == 0);
	file = std::fopen((safe + "/target/headers/witness").c_str(), "r");
	char bytes[16] = {};
	assert(file != nullptr && std::fread(bytes, 1, sizeof(bytes), file) == 7);
	assert(std::fclose(file) == 0 && std::strcmp(bytes, "header\n") == 0);

	assert(::symlink(outside.c_str(), (safe + "/outside-link").c_str()) == 0);
	assert(rejectsParent(safe + "/outside-link/escaped/file", safe, created));
	assert(::access((outside + "/escaped").c_str(), F_OK) != 0);
	assert(rejectsParent(safe + "/../outside/lexical/file", safe, created));
	assert(::access((outside + "/lexical").c_str(), F_OK) != 0);

	assert(::symlink("absent", (safe + "/dangling").c_str()) == 0);
	assert(rejectsParent(safe + "/dangling/file", safe, created));
	assert(::access((safe + "/absent").c_str(), F_OK) != 0);
	file = std::fopen((safe + "/regular").c_str(), "w");
	assert(file != nullptr && std::fclose(file) == 0);
	assert(::symlink("regular", (safe + "/file-link").c_str()) == 0);
	assert(rejectsParent(safe + "/file-link/child/file", safe, created));

	assert(::symlink("target", (safe + "/created-link").c_str()) == 0);
	assert(::lstat((safe + "/created-link").c_str(), &st) == 0);
	created.insert(std::make_pair(st.st_dev, st.st_ino));
	assert(::symlink("created-link", (safe + "/indirect").c_str()) == 0);
	assert(rejectsParent(safe + "/created-link/blocked/file", safe, created));
	assert(rejectsParent(safe + "/indirect/blocked/file", safe, created));
	assert(::access((safe + "/target/blocked").c_str(), F_OK) != 0);

	installer_archive::ensureParentDirectories(safe + "/inside/volume/file", 0755, "/", created);
	assert(::stat((safe + "/target/volume").c_str(), &st) == 0 && S_ISDIR(st.st_mode));
	assert(rejectsParent(safe + "/indirect/volume-blocked/file", "/", created));
	assert(::access((safe + "/target/volume-blocked").c_str(), F_OK) != 0);

	assert(::symlink("cycle-b", (safe + "/cycle-a").c_str()) == 0);
	assert(::symlink("cycle-a", (safe + "/cycle-b").c_str()) == 0);
	assert(rejectsParent(safe + "/cycle-a/file", safe, created));

	assert(::unlink((safe + "/target/headers/witness").c_str()) == 0);
	assert(::rmdir((safe + "/target/headers").c_str()) == 0);
	assert(::rmdir((safe + "/target/volume").c_str()) == 0);
	assert(::rmdir((safe + "/target").c_str()) == 0);
	const char* links[] = {"inside", "outside-link", "dangling", "file-link",
		"created-link", "indirect", "cycle-a", "cycle-b", "regular"};
	for (const char* name : links)
		assert(::unlink((safe + "/" + name).c_str()) == 0);
	assert(::rmdir((safe + "/new").c_str()) == 0);
	assert(::rmdir(safe.c_str()) == 0);
	assert(::rmdir(outside.c_str()) == 0);
	assert(::rmdir(root.c_str()) == 0);
	std::printf("ARCHIVE_PATH_TEST_OK\n");
	return 0;
}
