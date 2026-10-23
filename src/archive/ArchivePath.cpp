#include "ArchivePath.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <sstream>
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

// A symlink component is acceptable ONLY when the caller named a trusted
// extraction root and the link resolves to a directory INSIDE that root. This is
// the macOS system-alias shape (`/tmp` -> `private/tmp`, `/var` -> `private/var`):
// the alias belongs to the destination's own layout, not to the archive. Anything
// that resolves outside the root keeps the strict rejection, which is what stops
// an archive-created link from carrying an entry out of the destination.
static bool resolvesWithin(const char* path, const std::string& trustedRoot)
{
	if (trustedRoot.empty())
		return false;
	char resolvedPath[PATH_MAX];
	char resolvedRoot[PATH_MAX];
	if (::realpath(path, resolvedPath) == nullptr || ::realpath(trustedRoot.c_str(), resolvedRoot) == nullptr)
		return false;
	const std::string root(resolvedRoot);
	const std::string resolved(resolvedPath);
	if (root == "/")
		return !resolved.empty() && resolved[0] == '/';
	return resolved == root || (resolved.size() > root.size()
		&& resolved.compare(0, root.size(), root) == 0 && resolved[root.size()] == '/');
}

static void ensureDirectory(const char* path, mode_t mode, const std::string& trustedRoot)
{
	struct stat st;
	if (::lstat(path, &st) == 0)
	{
		if (S_ISLNK(st.st_mode))
		{
			struct stat target;
			if (!resolvesWithin(path, trustedRoot) || ::stat(path, &target) != 0 || !S_ISDIR(target.st_mode))
			{
				std::stringstream ss;
				ss << "Archive parent is not a directory: " << path;
				throw std::runtime_error(ss.str());
			}
			return;
		}
		if (!S_ISDIR(st.st_mode))
		{
			std::stringstream ss;
			ss << "Archive parent is not a directory: " << path;
			throw std::runtime_error(ss.str());
		}
		return;
	}

	if (errno != ENOENT || (::mkdir(path, mode) != 0 && errno != EEXIST))
	{
		std::stringstream ss;
		ss << "Cannot mkdir " << path << ": " << strerror(errno);
		throw std::runtime_error(ss.str());
	}

	if (::lstat(path, &st) != 0 || !S_ISDIR(st.st_mode))
	{
		std::stringstream ss;
		ss << "Archive parent is not a directory: " << path;
		throw std::runtime_error(ss.str());
	}
}

void ensureParentDirectories(const std::string& fileName, mode_t mode, const std::string& trustedRoot)
{
	std::string path = fileName;
	for (size_t pos = path.find('/'); pos != std::string::npos;
		pos = path.find('/', pos + 1))
	{
		if (pos == 0)
			continue;

		std::string parent = path.substr(0, pos);
		ensureDirectory(parent.c_str(), mode, trustedRoot);
	}
}

}
