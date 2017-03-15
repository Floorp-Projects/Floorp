/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_server_config_h__
#define tls_server_config_h__

#include <stdint.h>
#include <cstddef>

class ServerConfig {
 public:
  ServerConfig(const uint8_t* data, size_t len);

  bool EnableExtendedMasterSecret();
  bool RequestCertificate();
  bool RequireCertificate();
  bool EnableDeflate();
  bool EnableCbcRandomIv();
  bool RequireSafeNegotiation();
  bool EnableCache();

 private:
  uint64_t config_;
};

#endif  // tls_server_config_h__
