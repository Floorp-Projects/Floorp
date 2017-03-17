/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_client_config_h__
#define tls_client_config_h__

#include <stdint.h>
#include <cstddef>

class ClientConfig {
 public:
  ClientConfig(const uint8_t* data, size_t len);

  bool FailCertificateAuthentication();
  bool EnableExtendedMasterSecret();
  bool RequireDhNamedGroups();
  bool EnableFalseStart();
  bool EnableDeflate();
  bool EnableCbcRandomIv();
  bool RequireSafeNegotiation();
  bool EnableCache();

 private:
  uint64_t config_;
};

#endif  // tls_client_config_h__
