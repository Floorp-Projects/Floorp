/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSBUtils.h"

#include <unistd.h>
#include "base/process_util.h"

namespace mozilla {
namespace widget {
namespace lsb {

static const char* gLsbReleasePath = "/usr/bin/lsb_release";

bool
GetLSBRelease(nsACString& aDistributor,
              nsACString& aDescription,
              nsACString& aRelease,
              nsACString& aCodename)
{
  if (access(gLsbReleasePath, R_OK) != 0)
    return false;

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    NS_WARNING("pipe() failed!");
    return false;
  }

  std::vector<std::string> argv = {
    gLsbReleasePath, "-idrc"
  };

  std::vector<std::pair<int, int>> fdMap = {
    { pipefd[1], STDOUT_FILENO }
  };

  base::ProcessHandle process;
  base::LaunchApp(argv, fdMap, true, &process);
  close(pipefd[1]);
  if (!process) {
    NS_WARNING("Failed to spawn lsb_release!");
    close(pipefd[0]);
    return false;
  }

  FILE* stream = fdopen(pipefd[0], "r");
  if (!stream) {
    NS_WARNING("Could not wrap fd!");
    close(pipefd[0]);
    return false;
  }

  char dist[256], desc[256], release[256], codename[256];
  if (fscanf(stream, "Distributor ID:\t%255[^\n]\n"
                     "Description:\t%255[^\n]\n"
                     "Release:\t%255[^\n]\n"
                     "Codename:\t%255[^\n]\n",
             dist, desc, release, codename) != 4)
  {
    NS_WARNING("Failed to parse lsb_release!");
    fclose(stream);
    close(pipefd[0]);
    return false;
  }
  fclose(stream);
  close(pipefd[0]);

  aDistributor.Assign(dist);
  aDescription.Assign(desc);
  aRelease.Assign(release);
  aCodename.Assign(codename);
  return true;
}

}  // namespace lsb
}  // namespace widget
}  // namespace mozilla
