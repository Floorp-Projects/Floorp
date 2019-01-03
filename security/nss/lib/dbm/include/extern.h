/*-
 * Copyright (c) 1991, 1993, 1994
 *  The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. ***REMOVED*** - see
 *    ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *  @(#)extern.h    8.4 (Berkeley) 6/16/94
 */

BUFHEAD *dbm_add_ovflpage(HTAB *, BUFHEAD *);
int dbm_addel(HTAB *, BUFHEAD *, const DBT *, const DBT *);
int dbm_big_delete(HTAB *, BUFHEAD *);
int dbm_big_insert(HTAB *, BUFHEAD *, const DBT *, const DBT *);
int dbm_big_keydata(HTAB *, BUFHEAD *, DBT *, DBT *, int);
int dbm_big_return(HTAB *, BUFHEAD *, int, DBT *, int);
int dbm_big_split(HTAB *, BUFHEAD *, BUFHEAD *, BUFHEAD *,
                  uint32, uint32, SPLIT_RETURN *);
int dbm_buf_free(HTAB *, int, int);
void dbm_buf_init(HTAB *, int);
uint32 dbm_call_hash(HTAB *, char *, size_t);
int dbm_delpair(HTAB *, BUFHEAD *, int);
int dbm_expand_table(HTAB *);
int dbm_find_bigpair(HTAB *, BUFHEAD *, int, char *, int);
uint16 dbm_find_last_page(HTAB *, BUFHEAD **);
void dbm_free_ovflpage(HTAB *, BUFHEAD *);
BUFHEAD *dbm_get_buf(HTAB *, uint32, BUFHEAD *, int);
int dbm_get_page(HTAB *, char *, uint32, int, int, int);
int dbm_ibitmap(HTAB *, int, int, int);
uint32 dbm_log2(uint32);
int dbm_put_page(HTAB *, char *, uint32, int, int);
void dbm_reclaim_buf(HTAB *, BUFHEAD *);
int dbm_split_page(HTAB *, uint32, uint32);

/* Default hash routine. */
extern uint32 (*dbm_default_hash)(const void *, size_t);

#ifdef HASH_STATISTICS
extern int hash_accesses, hash_collisions, hash_expansions, hash_overflows;
#endif
