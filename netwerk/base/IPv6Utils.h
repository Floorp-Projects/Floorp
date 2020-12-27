/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NETWORK_IPV6_UTILS_H_
#define NETWORK_IPV6_UTILS_H_

namespace mozilla {
namespace net {
namespace utils {

// IPv6 address scopes.
#define IPV6_SCOPE_GLOBAL 0       // Global scope.
#define IPV6_SCOPE_LINKLOCAL 1    // Link-local scope.
#define IPV6_SCOPE_SITELOCAL 2    // Site-local scope (deprecated).
#define IPV6_SCOPE_UNIQUELOCAL 3  // Unique local
#define IPV6_SCOPE_NODELOCAL 4    // Loopback

// Return the scope of the given address.
static int ipv6_scope(const unsigned char addr[16]) {
  const unsigned char* b = addr;
  unsigned short w = (unsigned short)((b[0] << 8) | b[1]);

  if ((b[0] & 0xFE) == 0xFC) {
    return IPV6_SCOPE_UNIQUELOCAL;
  }
  switch (w & 0xFFC0) {
    case 0xFE80:
      return IPV6_SCOPE_LINKLOCAL;
    case 0xFEC0:
      return IPV6_SCOPE_SITELOCAL;
    case 0x0000:
      w = b[1] | b[2] | b[3] | b[4] | b[5] | b[6] | b[7] | b[8] | b[9] | b[10] |
          b[11] | b[12] | b[13] | b[14];
      if (w || b[15] != 0x01) {
        break;
      }
      return IPV6_SCOPE_NODELOCAL;
    default:
      break;
  }

  return IPV6_SCOPE_GLOBAL;
}

}  // namespace utils
}  // namespace net
}  // namespace mozilla

#endif  // NETWORK_IPV6_UTILS_H_
