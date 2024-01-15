/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSBUtils.h"

#include <fstream>
#include <string>
#include <string_view>
#include <unistd.h>
#include "base/process_util.h"
#include "mozilla/FileUtils.h"
#include "mozilla/Result.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/ipc/LaunchError.h"

namespace mozilla::widget::lsb {

static const char gLsbReleasePath[] = "/usr/bin/lsb_release";
static const char gEtcOsReleasePath[] = "/etc/os-release";
static const char gUsrOsReleasePath[] = "/usr/lib/os-release";

// See https://www.freedesktop.org/software/systemd/man/latest/os-release.html
bool ExtractAndSetValue(nsACString& aContainer, std::string_view& aValue) {
  // We assume the value is well formed and doesn't contain escape characters.
  if (aValue.size() > 1 && (aValue.front() == '"' || aValue.front() == '\'')) {
    // We assume the quote is properly balanced.
    aValue = aValue.substr(1, aValue.size() - 2);
  }
  aContainer.Assign(aValue.data(), aValue.size());
  return !aValue.empty();
}

bool GetOSRelease(nsACString& aDistributor, nsACString& aDescription,
                  nsACString& aRelease, nsACString& aCodename) {
  std::ifstream stream(gEtcOsReleasePath);
  if (stream.fail()) {
    stream.open(gUsrOsReleasePath);
    if (stream.fail()) {
      return false;
    }
  }
  bool seen_id = false, seen_pretty_name = false, seen_version_id = false;
  std::string rawline;
  nsAutoCString name;
  while (std::getline(stream, rawline)) {
    std::string_view line(rawline);
    size_t pos = line.find('=');
    if (pos != std::string_view::npos) {
      auto key = line.substr(0, pos);
      auto value = line.substr(pos + 1);
      if (key == "ID") {
        if (ExtractAndSetValue(aDistributor, value)) {
          // Capitalize the first letter of the id. This mimics what
          // lsb_release does on Debian and derivatives. On RH derivatives,
          // ID tends to be capitalized already.
          char* c = aDistributor.BeginWriting();
          if (*c >= 'a' && *c <= 'z') {
            *c -= ('a' - 'A');
          }
          seen_id = true;
        }
      } else if (key == "NAME") {
        ExtractAndSetValue(name, value);
      } else if (key == "PRETTY_NAME") {
        if (ExtractAndSetValue(aDescription, value)) seen_pretty_name = true;
      } else if (key == "VERSION_ID") {
        if (ExtractAndSetValue(aRelease, value)) seen_version_id = true;
      } else if (key == "VERSION_CODENAME") {
        ExtractAndSetValue(aCodename, value);
      }
    }
  }
  // If NAME is set and only differs from ID in case, use NAME.
  if (seen_id && !name.IsEmpty() && name.EqualsIgnoreCase(aDistributor)) {
    aDistributor = name;
  }
  // Only consider our work done if we've seen at least ID, PRETTY_NAME and
  // VERSION_ID.
  return seen_id && seen_pretty_name && seen_version_id;
}

bool GetLSBRelease(nsACString& aDistributor, nsACString& aDescription,
                   nsACString& aRelease, nsACString& aCodename) {
  // Nowadays, /etc/os-release is more likely to be available than
  // /usr/bin/lsb_release. Relying on the former also avoids forking.
  if (GetOSRelease(aDistributor, aDescription, aRelease, aCodename)) {
    return true;
  }

  if (access(gLsbReleasePath, R_OK) != 0) return false;

  int pipefd[2];
  if (pipe(pipefd) == -1) {
    NS_WARNING("pipe() failed!");
    return false;
  }

  std::vector<std::string> argv = {gLsbReleasePath, "-idrc"};

  base::LaunchOptions options;
  options.fds_to_remap.push_back({pipefd[1], STDOUT_FILENO});
  options.wait = true;

  base::ProcessHandle process;
  Result<Ok, ipc::LaunchError> err =
      base::LaunchApp(argv, std::move(options), &process);
  close(pipefd[1]);
  if (err.isErr()) {
    NS_WARNING("Failed to spawn lsb_release!");
    close(pipefd[0]);
    return false;
  }

  ScopedCloseFile stream(fdopen(pipefd[0], "r"));
  if (!stream) {
    NS_WARNING("Could not wrap fd!");
    close(pipefd[0]);
    return false;
  }

  char dist[256], desc[256], release[256], codename[256];
  if (fscanf(stream,
             "Distributor ID:\t%255[^\n]\n"
             "Description:\t%255[^\n]\n"
             "Release:\t%255[^\n]\n"
             "Codename:\t%255[^\n]\n",
             dist, desc, release, codename) != 4) {
    NS_WARNING("Failed to parse lsb_release!");
    return false;
  }

  aDistributor.Assign(dist);
  aDescription.Assign(desc);
  aRelease.Assign(release);
  aCodename.Assign(codename);
  return true;
}

}  // namespace mozilla::widget::lsb
