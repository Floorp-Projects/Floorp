/*
 *  gcd.c
 *
 *  Greatest common divisor
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

#include "mpi.h"

char     *g_prog = NULL;

void print_mp_int(mp_int *mp, FILE *ofp);

int main(int argc, char *argv[]) 
{
  mp_int   a, b, x, y;
  mp_err   res;
  int      ext = 0;

  g_prog = argv[0];

  if(argc < 3) {
    fprintf(stderr, "Usage: %s <a> <b>\n", g_prog);
    return 1;
  }

  mp_init(&a); mp_read_radix(&a, argv[1], 10);
  mp_init(&b); mp_read_radix(&b, argv[2], 10);

  /* If we were called 'xgcd', compute x, y so that g = ax + by */
  if(strcmp(g_prog, "xgcd") == 0) {
    ext = 1;
    mp_init(&x); mp_init(&y);
  }

  if(ext) {
    if((res = mp_xgcd(&a, &b, &a, &x, &y)) != MP_OKAY) {
      fprintf(stderr, "%s: error: %s\n", g_prog, mp_strerror(res));
      mp_clear(&a); mp_clear(&b);
      mp_clear(&x); mp_clear(&y);
      return 1;
    }
  } else {
    if((res = mp_gcd(&a, &b, &a)) != MP_OKAY) {
      fprintf(stderr, "%s: error: %s\n", g_prog,
	      mp_strerror(res));
      mp_clear(&a); mp_clear(&b);
      return 1;
    }
  }

  print_mp_int(&a, stdout); 
  if(ext) {
    fputs("x = ", stdout); print_mp_int(&x, stdout); 
    fputs("y = ", stdout); print_mp_int(&y, stdout); 
  }

  mp_clear(&a); mp_clear(&b);

  if(ext) {
    mp_clear(&x);
    mp_clear(&y);
  }

  return 0;

}

void print_mp_int(mp_int *mp, FILE *ofp)
{
  char  *buf;
  int    len;

  len = mp_radix_size(mp, 10);
  buf = calloc(len, sizeof(char));
  mp_todecimal(mp, buf);
  fprintf(ofp, "%s\n", buf);
  free(buf);

}
