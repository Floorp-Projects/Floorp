/*
 *  bbs_rand.h
 *
 *  Blum, Blum & Shub PRNG using the MPI library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: bbs_rand.h,v 1.4 2012/04/25 14:49:54 gerv%gerv.net Exp $ */

#ifndef _H_BBSRAND_
#define _H_BBSRAND_

#include <limits.h>
#include "mpi.h"

#define  BBS_RAND_MAX    UINT_MAX

/* Suggested length of seed data */
extern int bbs_seed_size;

void         bbs_srand(unsigned char *data, int len);
unsigned int bbs_rand(void);

#endif /* end _H_BBSRAND_ */
