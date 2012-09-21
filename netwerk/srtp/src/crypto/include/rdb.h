/*
 * replay-database.h
 *
 * interface for a replay database for packet security
 *
 * David A. McGrew
 * Cisco Systems, Inc.
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
