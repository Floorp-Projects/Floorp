/* -*- Mode: C -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(__i386__)
#include "xptcinvoke_gcc_x86_unix.cpp"
#elif defined(__x86_64__)
#include "xptcinvoke_x86_64_unix.cpp"
#elif defined(__ppc__)
#include "xptcinvoke_ppc_rhapsody.cpp"
#elif defined(__arm__)
#include "xptcinvoke_arm.cpp"
#else
#error unknown cpu architecture
#endif
