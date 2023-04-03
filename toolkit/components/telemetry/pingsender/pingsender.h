/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_telemetry_pingsender_h
#define mozilla_telemetry_pingsender_h

#include <string>

#ifdef DEBUG
#  define PINGSENDER_LOG(s, ...) printf(s, ##__VA_ARGS__)
#else
#  define PINGSENDER_LOG(s, ...)
#endif  // DEBUG

namespace PingSender {

// The maximum time, in milliseconds, we allow for the connection phase
// to the server.
constexpr uint32_t kConnectionTimeoutMs = 30 * 1000;
constexpr char kUserAgent[] = "pingsender/1.0";
constexpr char kCustomVersionHeader[] = "X-PingSender-Version: 1.0";
constexpr char kContentEncodingHeader[] = "Content-Encoding: gzip";

// System-specific function that changes the current working directory to be
// the same as the one containing the ping file. This is currently required on
// Windows to release the Firefox installation folder (see bug 1597803 for more
// details) and is a no-op on other platforms.
void ChangeCurrentWorkingDirectory(const std::string& pingPath);

// System-specific function to make an HTTP POST operation
bool Post(const std::string& url, const std::string& payload);

bool IsValidDestination(char* aUriEndingInHost);
bool IsValidDestination(std::string aUriEndingInHost);
std::string GenerateDateHeader();

}  // namespace PingSender

#endif
