/*
 *  Simple test driver for MPI library
 *
 *  Test 6: Output functions
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

#include "mpi.h"

void print_buf(FILE *ofp, char *buf, int len)
{
  int ix, brk = 0;

  for(ix = 0; ix < len; ix++) {
    fprintf(ofp, "%02X ", buf[ix]);

    brk = (brk + 1) & 0xF;
    if(!brk) 
      fputc('\n', ofp);
  }

  if(brk)
    fputc('\n', ofp);

}

int main(int argc, char *argv[])
{
  int       ix, size;
  mp_int    a;
  char     *buf;

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <a>\n", argv[0]);
    return 1;
  }

  printf("Test 6: Output functions\n\n");

  mp_init(&a);

  mp_read_radix(&a, argv[1], 10);

  printf("\nConverting to a string:\n");

  printf("Rx Size Representation\n");
  for(ix = 2; ix <= MAX_RADIX; ix++) {
    size = mp_radix_size(&a, ix);

    buf = calloc(size, sizeof(char));
    mp_toradix(&a, buf, ix);
    printf("%2d: %3d: %s\n", ix, size, buf);
    free(buf);

  }

  printf("\nRaw output:\n");
  size = mp_raw_size(&a);
  buf = calloc(size, sizeof(char));

  printf("Size:  %d bytes\n", size);

  mp_toraw(&a, buf);
  print_buf(stdout, buf, size);
  free(buf);
  
  mp_clear(&a);

  return 0;
}
