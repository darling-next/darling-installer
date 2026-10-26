#include "ArchivePath.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace installer_archive
{

std::string normalizePath(const std::string& archivePath)
{
	if (!archivePath.empty() && archivePath[0] == '/')
		throw std::runtime_error("Invalid absolute archive path");

	std::string normalized;
	size_t componentStart = 0;
	while (componentStart <= archivePath.length())
	{
		size_t slash = archivePath.find('/', componentStart);
		std::string component = archivePath.substr(
			componentStart,
			slash == std::string::npos ? std::string::npos : slash - componentStart);

		if (component == "..")
			throw std::runtime_error("Invalid parent archive path");
		if (!component.empty() && component != ".")
		{
			if (!normalized.empty())
				normalized += '/';
			normalized += component;
		}

		if (slash == std::string::npos)
			break;
		componentStart = slash + 1;
	}

	return normalized;
}

static void rejectDirectory(const char* path, const char* reason, int error)
{
	char message[PATH_MAX + 256];
	std::snprintf(message, sizeof(message), "Archive parent rejected (%s): %s%s%s",
		reason, path, error ? ": " : "", error ? strerror(error) : "");
	throw std::runtime_error(message);
}

static bool withinRoot(const char* path, const std::string& root)
{
	if (root == "/")
		return path[0] == '/';
	return std::strncmp(path, root.c_str(), root.size()) == 0 &&
		(path[root.size()] == '\0' || path[root.size()] == '/');
}

// Upstream mkdir-p follows framework aliases but supplies no escape defense.
// Validate the entire existing link chain: an alias can itself point through a
// symlink created by an earlier archive entry, including under target "/".
static void checkExistingDirectory(const char* path,
	const CreatedSymlinks& createdSymlinks, unsigned depth)
{
	if (depth >= 40)
		rejectDirectory(path, "symlink depth limit", 0);
	size_t length = std::strlen(path);
	char probe[PATH_MAX];
	if (length >= sizeof(probe))
		rejectDirectory(path, "path too long", 0);
	std::memcpy(probe, path, length + 1);
	for (size_t pos = 1; pos <= length; ++pos)
	{
		if (probe[pos] != '/' && probe[pos] != '\0')
			continue;
		char separator = probe[pos];
		probe[pos] = '\0';
		struct stat st;
		if (::lstat(probe, &st) != 0)
			rejectDirectory(probe, "lstat failed", errno);
		if (S_ISLNK(st.st_mode))
		{
			if (createdSymlinks.count(std::make_pair(st.st_dev, st.st_ino)))
				rejectDirectory(probe, "archive-created symlink", 0);
			char target[PATH_MAX];
			ssize_t count = ::readlink(probe, target, sizeof(target));
			if (count <= 0 || count >= static_cast<ssize_t>(sizeof(target)))
				rejectDirectory(probe, "invalid symlink target", count < 0 ? errno : 0);
			std::string next;
			if (target[0] != '/')
			{
				const char* slash = std::strrchr(probe, '/');
				next.assign(probe, static_cast<size_t>(slash - probe + 1));
			}
			next.append(target, static_cast<size_t>(count));
			checkExistingDirectory(next.c_str(), createdSymlinks, depth + 1);
		}
		else if (!S_ISDIR(st.st_mode))
			rejectDirectory(probe, "non-directory object", 0);
		probe[pos] = separator;
	}
}

static void ensureDirectory(const char* path, mode_t mode,
	const std::string& trustedRoot, const CreatedSymlinks& createdSymlinks)
{
	struct stat st;
	if (::lstat(path, &st) != 0)
	{
		if (errno != ENOENT || (::mkdir(path, mode) != 0 && errno != EEXIST))
			throw std::runtime_error(std::string("Cannot mkdir ") + path + ": " + strerror(errno));
		if (::lstat(path, &st) != 0)
			rejectDirectory(path, "lstat after mkdir failed", errno);
	}
	if (S_ISDIR(st.st_mode))
		return;
	if (!S_ISLNK(st.st_mode))
		rejectDirectory(path, "non-directory object", 0);
	checkExistingDirectory(path, createdSymlinks, 0);
	char canonical[PATH_MAX];
	if (::realpath(path, canonical) == nullptr)
		rejectDirectory(path, "realpath failed", errno);
	if (!withinRoot(canonical, trustedRoot))
		rejectDirectory(canonical, "resolved outside trusted target", 0);
}

void ensureParentDirectories(const std::string& fileName, mode_t mode,
	const std::string& trustedRoot, const CreatedSymlinks& createdSymlinks)
{
	if (trustedRoot.empty() || trustedRoot[0] != '/' ||
		!withinRoot(fileName.c_str(), trustedRoot))
		throw std::runtime_error("Archive destination is outside the trusted root");
	char path[PATH_MAX];
	if (fileName.size() >= sizeof(path))
		throw std::runtime_error("Archive destination is too long");
	std::memcpy(path, fileName.c_str(), fileName.size() + 1);
	size_t component = trustedRoot.size();
	if (path[component] == '/')
		++component;
	for (size_t pos = component; pos <= fileName.size(); ++pos)
	{
		if (path[pos] != '/' && path[pos] != '\0')
			continue;
		if (pos - component == 2 && path[component] == '.' && path[component + 1] == '.')
			throw std::runtime_error("Invalid parent archive path");
		if (pos < fileName.size())
		{
			path[pos] = '\0';
			ensureDirectory(path, mode, trustedRoot, createdSymlinks);
			path[pos] = '/';
		}
		component = pos + 1;
	}
}

}
