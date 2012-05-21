/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A stub library to LD_PRELOAD against an image that's been compiled
  with -finstrument-functions. It dumps the address of the function
  that's being entered upon function entry, and the address that's
  being returned to on function exit. Addresses are dumped as binary
  data to stdout.

 */

#include <unistd.h>

void
__cyg_profile_func_enter(void *this_fn, void *call_site)
{
    write(STDOUT_FILENO, &this_fn, sizeof this_fn);
}

void
__cyg_profile_func_exit(void *this_fn, void *call_site)
{
    write(STDOUT_FILENO, &call_site, sizeof call_site);
}
