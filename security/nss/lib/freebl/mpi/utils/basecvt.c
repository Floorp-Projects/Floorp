/*
 *  basecvt.c
 *
 *  Convert integer values specified on the command line from one input
 *  base to another.  Accepts input and output bases between 2 and 36
 *  inclusive.
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
 *  $Id: basecvt.c,v 1.1 2000/07/14 00:44:53 nelsonb%netscape.com Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mpi.h"

#define IBASE     10
#define OBASE     16
#define USAGE     "Usage: %s ibase obase [value]\n"
#define MAXBASE   64
#define MINBASE   2

int main(int argc, char *argv[])
{
  int    ix, ibase = IBASE, obase = OBASE;
  mp_int val;

  ix = 1;
  if(ix < argc) {
    ibase = atoi(argv[ix++]);
    
    if(ibase < MINBASE || ibase > MAXBASE) {
      fprintf(stderr, "%s: input radix must be between %d and %d inclusive\n",
	      argv[0], MINBASE, MAXBASE);
      return 1;
    }
  }
  if(ix < argc) {
    obase = atoi(argv[ix++]);

    if(obase < MINBASE || obase > MAXBASE) {
      fprintf(stderr, "%s: output radix must be between %d and %d inclusive\n",
	      argv[0], MINBASE, MAXBASE);
      return 1;
    }
  }

  mp_init(&val);
  while(ix < argc) {
    char  *out;
    int    outlen;

    mp_read_radix(&val, argv[ix++], ibase);

    outlen = mp_radix_size(&val, obase);
    out = calloc(outlen, sizeof(char));
    mp_toradix(&val, out, obase);

    printf("%s\n", out);
    free(out);
  }

  mp_clear(&val);

  return 0;
}
