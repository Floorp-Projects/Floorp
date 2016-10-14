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

void UIRunMinidumpAnalyzer(const string& exename, const string& filename)
{
  // Run the minidump analyzer and wait for it to finish
  pid_t pid = fork();

  if (pid == -1) {
    return; // Nothing to do upon failure
  } else if (pid == 0) {
    execl(exename.c_str(), exename.c_str(), filename.c_str(), nullptr);
  } else {
    waitpid(pid, nullptr, 0);
  }
}
