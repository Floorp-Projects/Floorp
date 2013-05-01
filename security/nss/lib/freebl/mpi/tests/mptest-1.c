/*
 * Simple test driver for MPI library
 *
 * Test 1: Simple input test (drives single-digit multiply and add,
 *         as well as I/O routines)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#ifdef MAC_CW_SIOUX
#include <console.h>
#endif

#include "mpi.h"

int main(int argc, char *argv[])
{
  int    ix;
  mp_int mp;

#ifdef MAC_CW_SIOUX
  argc = ccommand(&argv);
#endif

  mp_init(&mp);
  
  for(ix = 1; ix < argc; ix++) {
    mp_read_radix(&mp, argv[ix], 10);
    mp_print(&mp, stdout);
    fputc('\n', stdout);
  }

  mp_clear(&mp);
  return 0;
}
