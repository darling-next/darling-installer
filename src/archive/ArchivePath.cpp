#include "ArchivePath.h"

#include <cerrno>
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

static void ensureDirectory(const char* path, mode_t mode)
{
	struct stat st;
	if (::lstat(path, &st) == 0)
	{
		if (S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode))
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

	if (::lstat(path, &st) != 0 || S_ISLNK(st.st_mode) || !S_ISDIR(st.st_mode))
	{
		std::stringstream ss;
		ss << "Archive parent is not a directory: " << path;
		throw std::runtime_error(ss.str());
	}
}

void ensureParentDirectories(const std::string& fileName, mode_t mode)
{
	std::string path = fileName;
	for (size_t pos = path.find('/'); pos != std::string::npos;
		pos = path.find('/', pos + 1))
	{
		if (pos == 0)
			continue;

		std::string parent = path.substr(0, pos);
		ensureDirectory(parent.c_str(), mode);
	}
}

}
