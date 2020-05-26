/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef STUN_IFADDRS_BSD_H_
#define STUN_IFADDRS_BSD_H_

#include "local_addr.h"

int stun_getaddrs_filtered(nr_local_addr addrs[], int maxaddrs, int *count);

#endif  /* STUN_IFADDRS_BSD_H_ */

