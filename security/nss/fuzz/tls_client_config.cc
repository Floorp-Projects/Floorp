/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tls_client_config.h"

const uint64_t CONFIG_FAIL_CERT_AUTH = 0x01;
const uint64_t CONFIG_ENABLE_EXTENDED_MS = 0x02;
const uint64_t CONFIG_REQUIRE_DH_NAMED_GROUPS = 0x04;
const uint64_t CONFIG_ENABLE_FALSE_START = 0x08;
const uint64_t CONFIG_ENABLE_DEFLATE = 0x10;
const uint64_t CONFIG_ENABLE_CBC_RANDOM_IV = 0x20;
const uint64_t CONFIG_REQUIRE_SAFE_NEGOTIATION = 0x40;
const uint64_t CONFIG_ENABLE_CACHE = 0x80;

// XOR 64-bit chunks of data to build a bitmap of config options derived from
// the fuzzing input. This seems the only way to fuzz various options while
// still maintaining compatibility with BoringSSL or OpenSSL fuzzers.
ClientConfig::ClientConfig(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    config_ ^= static_cast<uint64_t>(data[i]) << (8 * (i % 8));
  }
}

bool ClientConfig::FailCertificateAuthentication() {
  return config_ & CONFIG_FAIL_CERT_AUTH;
}

bool ClientConfig::EnableExtendedMasterSecret() {
  return config_ & CONFIG_ENABLE_EXTENDED_MS;
}

bool ClientConfig::RequireDhNamedGroups() {
  return config_ & CONFIG_REQUIRE_DH_NAMED_GROUPS;
}

bool ClientConfig::EnableFalseStart() {
  return config_ & CONFIG_ENABLE_FALSE_START;
}

bool ClientConfig::EnableDeflate() { return config_ & CONFIG_ENABLE_DEFLATE; }

bool ClientConfig::EnableCbcRandomIv() {
  return config_ & CONFIG_ENABLE_CBC_RANDOM_IV;
}

bool ClientConfig::RequireSafeNegotiation() {
  return config_ & CONFIG_REQUIRE_SAFE_NEGOTIATION;
}

bool ClientConfig::EnableCache() { return config_ & CONFIG_ENABLE_CACHE; }
