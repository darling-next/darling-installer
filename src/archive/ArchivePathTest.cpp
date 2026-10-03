#include "ArchivePath.h"

#include <cassert>
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

int main()
{
	assert(installer_archive::normalizePath("./Library//Developer") == "Library/Developer");
	assert(rejects("/Library/Developer"));
	assert(rejects("Library/../../outside"));

	char temporary[] = "/tmp/installer-archive-path-XXXXXX";
	const char* root = ::mkdtemp(temporary);
	assert(root != nullptr);
	std::string safe = std::string(root) + "/safe";
	std::string outside = std::string(root) + "/outside";
	assert(::mkdir(safe.c_str(), 0755) == 0);
	assert(::mkdir(outside.c_str(), 0755) == 0);

	std::string nested = safe + "/created/file";
	installer_archive::ensureParentDirectories(nested, 0755);
	struct stat st;
	assert(::stat((safe + "/created").c_str(), &st) == 0);
	assert(S_ISDIR(st.st_mode));

	std::string link = safe + "/link";
	assert(::symlink(outside.c_str(), link.c_str()) == 0);
	try
	{
		installer_archive::ensureParentDirectories(link + "/escaped/file", 0755);
		assert(false && "archive parent symlink was accepted");
	}
	catch (const std::runtime_error&)
	{
	}
	assert(::access((outside + "/escaped").c_str(), F_OK) != 0);

	std::printf("ARCHIVE_PATH_TEST_OK\n");
	return 0;
}
