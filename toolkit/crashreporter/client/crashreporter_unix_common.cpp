/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "crashreporter.h"

#include <algorithm>

#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace CrashReporter;
using std::string;
using std::vector;
using std::sort;

struct FileData
{
  time_t timestamp;
  string path;
};

static bool CompareFDTime(const FileData& fd1, const FileData& fd2)
{
  return fd1.timestamp > fd2.timestamp;
}

void UIPruneSavedDumps(const std::string& directory)
{
  DIR *dirfd = opendir(directory.c_str());
  if (!dirfd)
    return;

  vector<FileData> dumpfiles;

  while (dirent *dir = readdir(dirfd)) {
    FileData fd;
    fd.path = directory + '/' + dir->d_name;
    if (fd.path.size() < 5)
      continue;

    if (fd.path.compare(fd.path.size() - 4, 4, ".dmp") != 0)
      continue;

    struct stat st;
    if (stat(fd.path.c_str(), &st)) {
      closedir(dirfd);
      return;
    }

    fd.timestamp = st.st_mtime;

    dumpfiles.push_back(fd);
  }

  closedir(dirfd);

  sort(dumpfiles.begin(), dumpfiles.end(), CompareFDTime);

  while (dumpfiles.size() > kSaveCount) {
    // get the path of the oldest file
    string path = dumpfiles[dumpfiles.size() - 1].path;
    UIDeleteFile(path.c_str());

    // s/.dmp/.extra/
    path.replace(path.size() - 4, 4, ".extra");
    UIDeleteFile(path.c_str());

    dumpfiles.pop_back();
  }
}

bool UIRunProgram(const std::string& exename,
                  const std::vector<std::string>& args,
                  bool wait)
{
  pid_t pid = fork();

  if (pid == -1) {
    return false;
  } else if (pid == 0) {
    // Child
    size_t argvLen = args.size() + 2;
    char** argv = new char*[argvLen];

    argv[0] = const_cast<char*>(exename.c_str());

    for (size_t i = 0; i < args.size(); i++) {
      argv[i + 1] = const_cast<char*>(args[i].c_str());
    }

    argv[argvLen - 1] = nullptr;

    // Run the program
    int rv = execv(exename.c_str(), argv);
    delete[] argv;

    if (rv == -1) {
      exit(EXIT_FAILURE);
    }
  } else {
    // Parent
    if (wait) {
      waitpid(pid, nullptr, 0);
    }
  }

  return true;
}

bool UIEnsurePathExists(const string& path)
{
  int ret = mkdir(path.c_str(), S_IRWXU);
  int e = errno;
  if (ret == -1 && e != EEXIST)
    return false;

  return true;
}

bool UIFileExists(const string& path)
{
  struct stat sb;
  int ret = stat(path.c_str(), &sb);
  if (ret == -1 || !(sb.st_mode & S_IFREG))
    return false;

  return true;
}

bool UIDeleteFile(const string& file)
{
  return (unlink(file.c_str()) != -1);
}

std::ifstream* UIOpenRead(const string& filename, bool binary)
{
  std::ios_base::openmode mode = std::ios::in;

  if (binary) {
    mode = mode | std::ios::binary;
  }

  return new std::ifstream(filename.c_str(), mode);
}

std::ofstream* UIOpenWrite(const string& filename,
                           bool append, // append=false
                           bool binary) // binary=false
{
  std::ios_base::openmode mode = std::ios::out;

  if (append) {
    mode = mode | std::ios::app;
  }

  if (binary) {
    mode = mode | std::ios::binary;
  }

  return new std::ofstream(filename.c_str(), mode);
}

string UIGetEnv(const string& name)
{
  const char *var = getenv(name.c_str());
  if (var && *var) {
    return var;
  }

  return "";
}
