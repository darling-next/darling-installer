#ifndef ARCHIVEPATH_H
#define ARCHIVEPATH_H

#include <set>
#include <utility>
#include <string>
#include <sys/types.h>

namespace installer_archive
{

std::string normalizePath(const std::string& archivePath);
using CreatedSymlinks = std::set<std::pair<dev_t, ino_t>>;

// The root is canonical and owned by the caller. Existing directory aliases may
// resolve within it; an archive-created symlink must never become a parent.
void ensureParentDirectories(const std::string& fileName, mode_t mode,
	const std::string& trustedRoot, const CreatedSymlinks& createdSymlinks);

}

#endif
