/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int mp3_sniff(const uint8_t* buf, long length);

#ifdef __cplusplus
}
#endif
