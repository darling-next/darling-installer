#ifndef ARCHIVEPATH_H
#define ARCHIVEPATH_H

#include <string>
#include <sys/types.h>

namespace installer_archive
{

std::string normalizePath(const std::string& archivePath);
void ensureParentDirectories(const std::string& fileName, mode_t mode);

}

#endif
