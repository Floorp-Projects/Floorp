/*
 *  lap.c
 *
 *  Find least annihilating power of a mod m
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
 *  $Id: lap.c,v 1.2 2001/10/17 20:35:37 jpierre%netscape.com Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "mpi.h"

void sig_catch(int ign);

int g_quit = 0;

int main(int argc, char *argv[])
{
  mp_int   a, m, p, k;

  if(argc < 3) {
    fprintf(stderr, "Usage: %s <a> <m>\n", argv[0]);
    return 1;
  }

  mp_init(&a);
  mp_init(&m);
  mp_init(&p);
  mp_add_d(&p, 1, &p);

  mp_read_radix(&a, argv[1], 10);
  mp_read_radix(&m, argv[2], 10);

  mp_init_copy(&k, &a);

  signal(SIGINT, sig_catch);
#ifndef __OS2__
  signal(SIGHUP, sig_catch);
#endif
  signal(SIGTERM, sig_catch);

  while(mp_cmp(&p, &m) < 0) {
    if(g_quit) {
	int  len;
	char *buf;

	len = mp_radix_size(&p, 10);
	buf = malloc(len);
	mp_toradix(&p, buf, 10);
	
	fprintf(stderr, "Terminated at: %s\n", buf);
	free(buf);
	return 1;
    }
    if(mp_cmp_d(&k, 1) == 0) {
      int    len;
      char  *buf;

      len = mp_radix_size(&p, 10);
      buf = malloc(len);
      mp_toradix(&p, buf, 10);

      printf("%s\n", buf);

      free(buf);
      break;
    }

    mp_mulmod(&k, &a, &m, &k);
    mp_add_d(&p, 1, &p);
  }

  if(mp_cmp(&p, &m) >= 0) 
    printf("No annihilating power.\n");

  mp_clear(&p);
  mp_clear(&m);
  mp_clear(&a);
  return 0;
}

void sig_catch(int ign)
{
  g_quit = 1;
}
