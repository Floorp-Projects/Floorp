/*
 *    mptest-9.c
 *
 *   Test logical functions
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
#include "mplogic.h"

int main(int argc, char *argv[])
{
  mp_int    a, b, c;
  int       pco;
  mp_err    res;

  printf("Test 9: Logical functions\n\n");

  if(argc < 3) {
    fprintf(stderr, "Usage: %s <a> <b>\n", argv[0]);
    return 1;
  }

  mp_init(&a); mp_init(&b); mp_init(&c);
  mp_read_radix(&a, argv[1], 16);
  mp_read_radix(&b, argv[2], 16);

  printf("a       = "); mp_print(&a, stdout); fputc('\n', stdout);
  printf("b       = "); mp_print(&b, stdout); fputc('\n', stdout);

  mpl_not(&a, &c);
  printf("~a      = "); mp_print(&c, stdout); fputc('\n', stdout);

  mpl_and(&a, &b, &c);
  printf("a & b   = "); mp_print(&c, stdout); fputc('\n', stdout);

  mpl_or(&a, &b, &c);
  printf("a | b   = "); mp_print(&c, stdout); fputc('\n', stdout);

  mpl_xor(&a, &b, &c);
  printf("a ^ b   = "); mp_print(&c, stdout); fputc('\n', stdout);

  mpl_rsh(&a, &c, 1);
  printf("a >>  1 = "); mp_print(&c, stdout); fputc('\n', stdout);
  mpl_rsh(&a, &c, 5);
  printf("a >>  5 = "); mp_print(&c, stdout); fputc('\n', stdout);
  mpl_rsh(&a, &c, 16);
  printf("a >> 16 = "); mp_print(&c, stdout); fputc('\n', stdout);

  mpl_lsh(&a, &c, 1);
  printf("a <<  1 = "); mp_print(&c, stdout); fputc('\n', stdout);
  mpl_lsh(&a, &c, 5);
  printf("a <<  5 = "); mp_print(&c, stdout); fputc('\n', stdout);
  mpl_lsh(&a, &c, 16);
  printf("a << 16 = "); mp_print(&c, stdout); fputc('\n', stdout);

  mpl_num_set(&a, &pco);
  printf("population(a) = %d\n", pco);
  mpl_num_set(&b, &pco);
  printf("population(b) = %d\n", pco);

  res = mpl_parity(&a);
  if(res == MP_EVEN)
    printf("a has even parity\n");
  else
    printf("a has odd parity\n");
  
  mp_clear(&c);
  mp_clear(&b);
  mp_clear(&a);

  return 0;
}

