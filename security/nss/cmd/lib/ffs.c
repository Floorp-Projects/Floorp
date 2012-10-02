/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#if !defined(XP_UNIX) && !defined(XP_OS2)

int ffs( unsigned int i)
{
    int rv	= 1;

    if (!i) return 0;

    while (!(i & 1)) {
    	i >>= 1;
	++rv;
    }

    return rv;
}
#endif
