/*
 * Simple test driver for MPI library
 *
 * Test 4: Modular arithmetic tests
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
 *  $Id: mptest-4.c,v 1.1 2000/07/14 00:44:43 nelsonb%netscape.com Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "mpi.h"

int main(int argc, char *argv[])
{
  int      ix;
  mp_int   a, b, c, m;
  mp_digit r;

  if(argc < 4) {
    fprintf(stderr, "Usage: %s <a> <b> <m>\n", argv[0]);
    return 1;
  }

  printf("Test 4: Modular arithmetic\n\n");

  mp_init(&a);
  mp_init(&b);
  mp_init(&m);

  mp_read_radix(&a, argv[1], 10);
  mp_read_radix(&b, argv[2], 10);
  mp_read_radix(&m, argv[3], 10);
  printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);
  printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
  printf("m = "); mp_print(&m, stdout); fputc('\n', stdout);
  
  mp_init(&c);
  printf("\nc = a (mod m)\n");

  mp_mod(&a, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = b (mod m)\n");

  mp_mod(&b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = b (mod 1853)\n");

  mp_mod_d(&b, 1853, &r);
  printf("c = %04X\n", r);

  printf("\nc = (a + b) mod m\n");

  mp_addmod(&a, &b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = (a - b) mod m\n");

  mp_submod(&a, &b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = (a * b) mod m\n");

  mp_mulmod(&a, &b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = (a ** b) mod m\n");

  mp_exptmod(&a, &b, &m, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nIn-place modular squaring test:\n");
  for(ix = 0; ix < 5; ix++) {
    printf("a = (a * a) mod m   a = ");
    mp_sqrmod(&a, &m, &a);
    mp_print(&a, stdout);
    fputc('\n', stdout);
  }
  

  mp_clear(&c);
  mp_clear(&m);
  mp_clear(&b);
  mp_clear(&a);

  return 0;
}
