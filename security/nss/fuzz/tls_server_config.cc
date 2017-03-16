/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_server_config.h"

const uint64_t CONFIG_ENABLE_EXTENDED_MS = 0x01;
const uint64_t CONFIG_REQUEST_CERTIFICATE = 0x02;
const uint64_t CONFIG_REQUIRE_CERTIFICATE = 0x04;
const uint64_t CONFIG_ENABLE_DEFLATE = 0x08;
const uint64_t CONFIG_ENABLE_CBC_RANDOM_IV = 0x10;
const uint64_t CONFIG_REQUIRE_SAFE_NEGOTIATION = 0x20;
const uint64_t CONFIG_ENABLE_CACHE = 0x40;

// XOR 64-bit chunks of data to build a bitmap of config options derived from
// the fuzzing input. This seems the only way to fuzz various options while
// still maintaining compatibility with BoringSSL or OpenSSL fuzzers.
ServerConfig::ServerConfig(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    config_ ^= static_cast<uint64_t>(data[i]) << (8 * (i % 8));
  }
}

bool ServerConfig::EnableExtendedMasterSecret() {
  return config_ & CONFIG_ENABLE_EXTENDED_MS;
}

bool ServerConfig::RequestCertificate() {
  return config_ & CONFIG_REQUEST_CERTIFICATE;
}

bool ServerConfig::RequireCertificate() {
  return config_ & CONFIG_REQUIRE_CERTIFICATE;
}

bool ServerConfig::EnableDeflate() { return config_ & CONFIG_ENABLE_DEFLATE; }

bool ServerConfig::EnableCbcRandomIv() {
  return config_ & CONFIG_ENABLE_CBC_RANDOM_IV;
}

bool ServerConfig::RequireSafeNegotiation() {
  return config_ & CONFIG_REQUIRE_SAFE_NEGOTIATION;
}

bool ServerConfig::EnableCache() { return config_ & CONFIG_ENABLE_CACHE; }
