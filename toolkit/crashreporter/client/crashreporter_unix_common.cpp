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

bool UIRunProgram(const std::string& exename, const std::string& arg,
                  const std::string& data, bool wait)
{
  bool usePipes = !data.empty();
  int fd[2];

  if (usePipes && (pipe(fd) != 0)) {
    return false;
  }

  pid_t pid = fork();

  if (pid == -1) {
    return false;
  } else if (pid == 0) {
    // Child
    if (usePipes) {
      if (dup2(fd[0], STDIN_FILENO) == -1) {
        exit(EXIT_FAILURE);
      }

      close(fd[0]);
      close(fd[1]);
    }

    char* argv[] = {
      const_cast<char*>(exename.c_str()),
      const_cast<char*>(arg.c_str()),
      nullptr
    };

    // Run the program
    int rv = execv(exename.c_str(), argv);

    if (rv == -1) {
      exit(EXIT_FAILURE);
    }
  } else {
    // Parent
    if (usePipes) {
      ssize_t rv;
      size_t offset = 0;
      size_t size = data.size();

      do {
        rv = write(fd[1], data.c_str() + offset, size);

        if (rv > 0) {
          size -= rv;
          offset += rv;
        }
      } while (size && ((rv > 0) || ((rv == -1) && errno == EINTR)));

      close(fd[0]);
      close(fd[1]);
    }

    if (wait) {
      waitpid(pid, nullptr, 0);
    }
  }

  return true;
}
