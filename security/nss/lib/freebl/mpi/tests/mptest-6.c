/*
 *  Simple test driver for MPI library
 *
 *  Test 6: Output functions
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the MPI Arbitrary Precision Integer Arithmetic
 * library.
 *
 * The Initial Developer of the Original Code is Michael J. Fromberger.
 * Portions created by Michael J. Fromberger are 
 * Copyright (C) 1998, 1999, 2000 Michael J. Fromberger. 
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the GPL.
 *
 *  $Id: mptest-6.c,v 1.1 2000/07/14 00:44:45 nelsonb%netscape.com Exp $
 */

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
