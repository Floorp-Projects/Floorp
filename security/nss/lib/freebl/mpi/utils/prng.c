/*
 *  prng.c
 *
 *  Command-line pseudo-random number generator
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
#include <limits.h>
#include <time.h>

#ifdef __OS2__
#include <types.h>
#include <process.h>
#else
#include <unistd.h>
#endif

#include "bbs_rand.h"

int main(int argc, char *argv[])
{
  unsigned char *seed;
  unsigned int   ix, num = 1;
  pid_t          pid;
  
  if(argc > 1) {
    num = atoi(argv[1]);
    if(num <= 0) 
      num = 1;
  }

  pid = getpid();
  srand(time(NULL) * (unsigned int)pid);

  /* Not a perfect seed, but not bad */
  seed = malloc(bbs_seed_size);
  for(ix = 0; ix < bbs_seed_size; ix++) {
    seed[ix] = rand() % UCHAR_MAX;
  }

  bbs_srand(seed, bbs_seed_size);
  memset(seed, 0, bbs_seed_size);
  free(seed);

  while(num-- > 0) {
    ix = bbs_rand();

    printf("%u\n", ix);
  }

  return 0;

}
