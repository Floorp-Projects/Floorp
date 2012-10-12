/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

typedef struct hash_table {
   struct hash_table *next;
   struct hash_table *prev;
   unsigned int key;
   void *data;
} hash_table_t;

typedef struct {
  unsigned int bucket;
  hash_table_t *node;
} hashItr_t;


extern void hashItrInit(hashItr_t *itr) ;
extern void * hashItrNext(hashItr_t *itr);
extern int addhash (unsigned int key, void *data) ;
extern int delhash(unsigned int  key);
extern void *findhash(unsigned int key);
extern unsigned int ccpro_get_sessionId_by_callid(unsigned short call_id);
