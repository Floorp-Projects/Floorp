/*
 * Simple test driver for MPI library
 *
 * Test 3: Multiplication, division, and exponentiation test
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the MPI Arbitrary Precision Integer Arithmetic library.
 *
 * The Initial Developer of the Original Code is
 * Michael J. Fromberger.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <time.h>

#include "mpi.h"

#define  SQRT 1  /* define nonzero to get square-root test  */
#define  EXPT 0  /* define nonzero to get exponentiate test */

int main(int argc, char *argv[])
{
  int      ix;
  mp_int   a, b, c, d;
  mp_digit r;
  mp_err   res;

  if(argc < 3) {
    fprintf(stderr, "Usage: %s <a> <b>\n", argv[0]);
    return 1;
  }

  printf("Test 3: Multiplication and division\n\n");
  srand(time(NULL));

  mp_init(&a);
  mp_init(&b);

  mp_read_variable_radix(&a, argv[1], 10);
  mp_read_variable_radix(&b, argv[2], 10);
  printf("a = "); mp_print(&a, stdout); fputc('\n', stdout);
  printf("b = "); mp_print(&b, stdout); fputc('\n', stdout);
  
  mp_init(&c);
  printf("\nc = a * b\n");

  mp_mul(&a, &b, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

  printf("\nc = b * 32523\n");

  mp_mul_d(&b, 32523, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
  
  mp_init(&d);
  printf("\nc = a / b, d = a mod b\n");
  
  mp_div(&a, &b, &c, &d);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);  
  printf("d = "); mp_print(&d, stdout); fputc('\n', stdout);  

  ix = rand() % 256;
  printf("\nc = a / %d, r = a mod %d\n", ix, ix);
  mp_div_d(&a, (mp_digit)ix, &c, &r);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);  
  printf("r = %04X\n", r);

#if EXPT
  printf("\nc = a ** b\n");
  mp_expt(&a, &b, &c);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);  
#endif

  ix = rand() % 256;
  printf("\nc = 2^%d\n", ix);
  mp_2expt(&c, ix);
  printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);

#if SQRT
  printf("\nc = sqrt(a)\n");
  if((res = mp_sqrt(&a, &c)) != MP_OKAY) {
    printf("mp_sqrt: %s\n", mp_strerror(res));
  } else {
    printf("c = "); mp_print(&c, stdout); fputc('\n', stdout);
    mp_sqr(&c, &c);
    printf("c^2 = "); mp_print(&c, stdout); fputc('\n', stdout);
  }
#endif

  mp_clear(&d);
  mp_clear(&c);
  mp_clear(&b);
  mp_clear(&a);

  return 0;
}
