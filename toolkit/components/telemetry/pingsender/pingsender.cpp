/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <string>
#include <cstdio>
#include <zlib.h>

#include "pingsender.h"

using std::string;

namespace PingSender {

const char* kUserAgent = "pingsender/1.0";
const char* kCustomVersionHeader = "X-PingSender-Version: 1.0";
const char* kContentEncodingHeader = "Content-Encoding: gzip";

/**
 * This shared function returns a Date header string for use in HTTP requests.
 * See "RFC 7231, section 7.1.1.2: Date" for its specifications.
 */
std::string
GenerateDateHeader()
{
  char buffer[128];
  std::time_t t = std::time(nullptr);
  strftime(buffer, sizeof(buffer), "Date: %a, %d %b %Y %H:%M:%S GMT", std::gmtime(&t));
  return string(buffer);
}

/**
 * Read the ping contents from stdin as a string
 */
static std::string
ReadPingFromStdin()
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

std::string
GzipCompress(const std::string& rawData)
{
  z_stream deflater = {};

  // Use the maximum window size when compressing: this also tells zlib to
  // generate a gzip header.
  const int32_t kWindowSize = MAX_WBITS + 16;
  if (deflateInit2(&deflater, Z_DEFAULT_COMPRESSION, Z_DEFLATED, kWindowSize, 8,
                   Z_DEFAULT_STRATEGY) != Z_OK) {
    PINGSENDER_LOG("ERROR: Could not initialize zlib deflating\n");
    return "";
  }

  // Initialize the output buffer. The size of the buffer is the same
  // as defined by the ZIP_BUFLEN macro in Gecko.
  const uint32_t kBufferSize = 4 * 1024 - 1;
  unsigned char outputBuffer[kBufferSize];
  deflater.next_out = outputBuffer;
  deflater.avail_out = kBufferSize;

  // Let zlib know about the input data.
  deflater.avail_in = rawData.size();
  deflater.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(rawData.c_str()));

  // Compress and append chunk by chunk.
  std::string gzipData;
  int err = Z_OK;

  while (deflater.avail_in > 0 && err == Z_OK) {
    err = deflate(&deflater, Z_NO_FLUSH);

    // Since we're using the Z_NO_FLUSH policy, zlib can decide how
    // much data to compress. When the buffer is full, we repeadetly
    // flush out.
    while (deflater.avail_out == 0) {
      size_t bytesToWrite = kBufferSize - deflater.avail_out;
      if (bytesToWrite == 0) {
        break;
      }
      gzipData.append(reinterpret_cast<const char*>(outputBuffer), bytesToWrite);

      // Update the state and let the deflater know about it.
      deflater.next_out = outputBuffer;
      deflater.avail_out = kBufferSize;
      err = deflate(&deflater, Z_NO_FLUSH);
    }
  }

  // Flush the deflater buffers.
  while (err == Z_OK) {
    err = deflate(&deflater, Z_FINISH);
    size_t bytesToWrite = kBufferSize - deflater.avail_out;
    if (bytesToWrite == 0) {
      break;
    }
    gzipData.append(reinterpret_cast<const char*>(outputBuffer), bytesToWrite);
    deflater.next_out = outputBuffer;
    deflater.avail_out = kBufferSize;
  }

  // Clean up.
  deflateEnd(&deflater);

  if (err != Z_STREAM_END) {
    PINGSENDER_LOG("ERROR: There was a problem while compressing the ping\n");
    return "";
  }

  return gzipData;
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

  // Compress the ping using gzip.
  string gzipPing(GzipCompress(ping));

  // In the unlikely event of failure to gzip-compress the ping, don't
  // attempt to send it uncompressed: Telemetry will pick it up and send
  // it compressed.
  if (gzipPing.empty()) {
    PINGSENDER_LOG("ERROR: Ping compression failed\n");
    exit(EXIT_FAILURE);
  }

  if (!Post(url, gzipPing)) {
    exit(EXIT_FAILURE);
  }

  exit(EXIT_SUCCESS);
}
