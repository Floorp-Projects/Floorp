/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <string>
#include <cstdio>

#include "pingsender.h"

using std::string;

namespace PingSender {

const char* kUserAgent = "pingsender/1.0";

/**
 * Read the ping contents from stdin as a string
 */
static std::string ReadPingFromStdin()
{
  const size_t kBufferSize = 32768;
  char buff[kBufferSize];
  string ping;
  size_t readBytes = 0;

  do {
    readBytes = fread(buff, 1, kBufferSize, stdin);
    ping.append(buff, readBytes);
  } while (!feof(stdin) && !ferror(stdin));

  if (ferror(stdin)) {
    PINGSENDER_LOG("ERROR: Could not read from stdin\n");
    return "";
  }

  return ping;
}

} // namespace PingSender

using namespace PingSender;

int main(int argc, char* argv[])
{
  string url;

  if (argc == 2) {
    url = argv[1];
  } else {
    PINGSENDER_LOG("Usage: pingsender URL\n"
                   "Send the payload passed via stdin to the specified URL "
                   "using an HTTP POST message\n");
    exit(EXIT_FAILURE);
  }

  if (url.empty()) {
    PINGSENDER_LOG("ERROR: No URL was provided\n");
    exit(EXIT_FAILURE);
  }

  string ping(ReadPingFromStdin());

  if (ping.empty()) {
    PINGSENDER_LOG("ERROR: Ping payload is empty\n");
    exit(EXIT_FAILURE);
  }

  if (!Post(url, ping)) {
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
