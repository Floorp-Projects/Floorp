/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This test verifies that NSS public headers don't conflict with common
 * identifier names.
 */

#include "nssilckt.h"

/*
 * Bug 455424: nssilckt.h used to define the enumeration constant 'Lock',
 * which conflicts with C++ code that defines a Lock class.  This is a
 * reduced test case in C for that name conflict.
 */
typedef struct {
    int dummy;
} Lock;

Lock lock;

int
main()
{
    return 0;
}
