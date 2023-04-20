/*
    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <errno.h>

int close_range(unsigned int first, unsigned int last, unsigned int flags) {
  errno = ENOSYS;
  return -1;
}
