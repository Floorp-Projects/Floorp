/*
 * replay-database.h
 *
 * interface for a replay database for packet security
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *	
 * Copyright (c) 2001-2006, Cisco Systems, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * 
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 * 
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef REPLAY_DB_H
#define REPLAY_DB_H

#include "integers.h"         /* for uint32_t     */
#include "datatypes.h"        /* for v128_t       */
#include "err.h"              /* for err_status_t */

/*
 * if the ith least significant bit is one, then the packet index
 * window_end-i is in the database
 */

typedef struct {
  uint32_t window_start;   /* packet index of the first bit in bitmask */
  v128_t bitmask;  
} rdb_t;

#define rdb_bits_in_bitmask (8*sizeof(v128_t))   

/*
 * rdb init
 *
 * initalizes rdb
 *
 * returns err_status_ok on success, err_status_t_fail otherwise
 */

err_status_t
rdb_init(rdb_t *rdb);


/*
 * rdb_check
 *
 * checks to see if index appears in rdb
 *
 * returns err_status_fail if the index already appears in rdb,
 * returns err_status_ok otherwise
 */

err_status_t
rdb_check(const rdb_t *rdb, uint32_t rdb_index);  

/*
 * rdb_add_index
 *
 * adds index to rdb_t (and does *not* check if index appears in db)
 *
 * returns err_status_ok on success, err_status_fail otherwise
 *
 */

err_status_t
rdb_add_index(rdb_t *rdb, uint32_t rdb_index);

/*
 * the functions rdb_increment() and rdb_get_value() are for use by 
 * senders, not receivers - DO NOT use these functions on the same
 * rdb_t upon which rdb_add_index is used!
 */


/*
 * rdb_increment(db) increments the sequence number in db, if it is 
 * not too high
 *
 * return values:
 * 
 *    err_status_ok            no problem
 *    err_status_key_expired   sequence number too high
 *
 */
err_status_t
rdb_increment(rdb_t *rdb);

/*
 * rdb_get_value(db) returns the current sequence number of db
 */

uint32_t
rdb_get_value(const rdb_t *rdb);


#endif /* REPLAY_DB_H */ 
