/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A library that can be LD_PRELOAD-ed to dump a function's address on
  function entry.

 */

#include <unistd.h>

/**
 * When compiled with `-pg' (or even just `-p'), gcc inserts a call to
 * |mcount| in each function prologue. This implementation of |mcount|
 * will grab the return address from the stack, and write it to stdout
 * as a binary stream, creating a gross execution trace the
 * instrumented program.
 *
 * For more info on gcc inline assembly, see:
 *
 *   <http://www.ibm.com/developerworks/linux/library/l-ia.html?dwzone=linux>
 *
 */
void
mcount()
{
    register unsigned int caller;
    unsigned int buf[1];

    // Grab the return address. 
    asm("movl 4(%%esp),%0"
        : "=r"(caller));

    buf[0] = caller;
    write(STDOUT_FILENO, buf, sizeof buf[0]);
}
