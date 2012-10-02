/*
 *  lap.c
 *
 *  Find least annihilating power of a mod m
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: lap.c,v 1.5 2012/04/25 14:49:54 gerv%gerv.net Exp $ */

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
