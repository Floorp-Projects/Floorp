/*
 * fact.c
 *
 * Compute factorial of input integer
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
 *  $Id: fact.c,v 1.1 2000/07/14 00:44:56 nelsonb%netscape.com Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

mp_err mp_fact(mp_int *a, mp_int *b);

int main(int argc, char *argv[])
{
  mp_int  a;
  mp_err  res;

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <number>\n", argv[0]);
    return 1;
  }

  mp_init(&a);
  mp_read_radix(&a, argv[1], 10);

  if((res = mp_fact(&a, &a)) != MP_OKAY) {
    fprintf(stderr, "%s: error: %s\n", argv[0],
	    mp_strerror(res));
    mp_clear(&a);
    return 1;
  }

  {
    char  *buf;
    int    len;

    len = mp_radix_size(&a, 10);
    buf = malloc(len);
    mp_todecimal(&a, buf);

    puts(buf);

    free(buf);
  }

  mp_clear(&a);
  return 0;
}

mp_err mp_fact(mp_int *a, mp_int *b)
{
  mp_int    ix, s;
  mp_err    res = MP_OKAY;

  if(mp_cmp_z(a) < 0)
    return MP_UNDEF;

  mp_init(&s);
  mp_add_d(&s, 1, &s);   /* s = 1  */
  mp_init(&ix);
  mp_add_d(&ix, 1, &ix); /* ix = 1 */

  for(/*  */; mp_cmp(&ix, a) <= 0; mp_add_d(&ix, 1, &ix)) {
    if((res = mp_mul(&s, &ix, &s)) != MP_OKAY)
      break;
  }

  mp_clear(&ix);

  /* Copy out results if we got them */
  if(res == MP_OKAY)
    mp_copy(&s, b);

  mp_clear(&s);

  return res;
}
