/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NETWORK_LINK_SERVICE_DEFINES_H_
#define NETWORK_LINK_SERVICE_DEFINES_H_

namespace mozilla {
namespace net {

// IP addresses that are used to check the route for public traffic. They are
// used just to check routing rules, no packets are sent to those hosts.
// Initially, addresses of host detectportal.firefox.com were used but they
// don't necessarily need to be updated when addresses of this host change.
#define ROUTE_CHECK_IPV4 "23.219.91.27"
#define ROUTE_CHECK_IPV6 "2a02:26f0:40::17db:5b1b"

}  // namespace net
}  // namespace mozilla

#endif  // NETWORK_LINK_SERVICE_DEFINES_H_
