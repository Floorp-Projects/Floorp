/* -*- Mode: C -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(__i386__)
#include "xptcstubs_gcc_x86_unix.cpp"
#elif defined(__x86_64__)
#include "xptcstubs_x86_64_darwin.cpp"
#elif defined(__ppc__)
#include "xptcstubs_ppc_rhapsody.cpp"
#else
#error unknown cpu architecture
#endif
