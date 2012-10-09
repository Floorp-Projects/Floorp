/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
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

#ifdef UNIT_TEST
#define cpr_malloc malloc
#define cpr_free   free
#define CCAPP_DEBUG printf
#else
#include "cpr_stdlib.h"
#endif

#include "sessionHash.h" 

#define HASHBUCKETS 67

hash_table_t *hashtable[HASHBUCKETS]={0};

void hashItrInit(hashItr_t *itr) 
{
  itr->bucket = 0;
  itr->node = NULL;
}

void * hashItrNext(hashItr_t *itr)
{
   int i;

   if ( itr->node != NULL ) {
     if ( itr->node->next != NULL ) {
       itr->node = itr->node->next;
       return itr->node->data;
     } 
     // We just iterated to the end of the list. 
     // Increment the bucket to search next
     itr->bucket++;
   }

   for(i=itr->bucket; i< HASHBUCKETS; i++) {
     if (hashtable[i] != NULL) {
       itr->bucket = i;
       itr->node = hashtable[i];
       return itr->node->data;
     }
   }
   return NULL;
}


/**
 * sessionHash 
 *      function to add generate hash given the key
 * 
 * @param key - 
 *
 * @return the hash index 
 */
unsigned int sessionHash (unsigned int key) 
{
 // since the key is session_id create the hashval to be line_id + call_id
   unsigned int hashval = key + ((key & 0xFFFF0000)>>16);

   return hashval%67;
}

/**
 * addhash 
 *      function to add data for a given key in the table
 * 
 * @param key  
 * @param data - pointer to data stored  
 *
 * @return - 0 for success
 */

int addhash (unsigned int key, void *data) 
{
   hash_table_t *newhash;
   hash_table_t *cur_hash;
   unsigned int hashval;

   newhash = (hash_table_t *)(cpr_malloc(sizeof(hash_table_t)));
   if (newhash == NULL) {
     return -1;
   }

   newhash->key = key;

   newhash->data = data;

   hashval = sessionHash(key);

   if (hashtable[hashval] == NULL) {
      hashtable[hashval] = newhash;
      hashtable[hashval]->prev = NULL;
      hashtable[hashval]->next = NULL;
   }
   else {
      cur_hash=hashtable[hashval];
      while(cur_hash->next != NULL) {
         cur_hash=cur_hash->next;
      }
      cur_hash->next = newhash;
      newhash->next = NULL;
      newhash->prev = cur_hash;
   }
   
   return 0;
}

/**
 * returns the session id given a callid
 * @param call_id
 * @return sessionID or 0
 */

unsigned int ccpro_get_sessionId_by_callid(unsigned short call_id) {
   int i;
   hash_table_t *cur_hash;

   for ( i=0; i<HASHBUCKETS ;i++){
       cur_hash = hashtable[i];
       while ( cur_hash) { 
         if ( (cur_hash->key & 0xffff) == call_id ) {
           return cur_hash->key;
         }
         cur_hash = cur_hash->next;
       }
   }
   return 0;
}


/**
 * findhash
 *      function retrieve the data for the given key
 *
 * @param key
 *
 * @return the data ptr or NULL
 */

void *findhash(unsigned int key) 
{
   unsigned int hashval;
   hash_table_t *cur_hash;
   
   hashval = 0;

   hashval = sessionHash(key);

   
   cur_hash = hashtable[hashval];
   while ( cur_hash != NULL ) {
       if ( cur_hash->key == key) {
            return cur_hash->data;
        }
        cur_hash = cur_hash->next;
   }

   return NULL;
}

/**
 * delhash
 *      function to remove the hash entry for a given key
 *
 * @param key
 *
 * @return - 0 for success
 */

int delhash(unsigned int  key) 
{
   unsigned int hashval;
   hash_table_t *cur_hash;
   
   hashval = 0;

   hashval = sessionHash(key);

   
   if (hashtable[hashval] == NULL) {
      return -1;
   }
 
   if (hashtable[hashval]->key == key) {
      cur_hash = hashtable[hashval];
      hashtable[hashval] = cur_hash->next;
      if ( hashtable[hashval] != NULL ) {
        hashtable[hashval]->prev = NULL;
      }
      cpr_free(cur_hash);
      return 0; 
   }
   else {

      cur_hash = hashtable[hashval]->next;

      while (cur_hash != NULL) {
         if (cur_hash->key == key) {
            cur_hash->prev->next = cur_hash->next;
            if (cur_hash->next != NULL) {
               cur_hash->next->prev = cur_hash->prev;
            }
            cpr_free(cur_hash);
            return 0; 
         }
         cur_hash = cur_hash->next;
      }
   }
   return -1;
}

#ifdef UNIT_TEST

void hashstats(int detail) 
{
   static const char *fname="hashstats";
   int max, total, i, nodes, used;
   double avg;
   hash_table_t *cur_hash;
  
   max = total = i = nodes = used = 0;
   avg = 0;
 
   if (detail > 0) {
      for (i = 0; i < HASHBUCKETS; i++) {
         if (hashtable[i] != NULL) {
            used++;
            nodes = 0;
            cur_hash = hashtable[i];
            while(cur_hash != NULL) {
               nodes++;
               if (detail > 3) {
                  CCAPPDEBUG(DEB_F_PREFIX"%lx -> %lx: (%lx) (%lx) -> %lx\n", 
                             DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname),
                             cur_hash->prev, cur_hash, cur_hash->key, cur_hash->data, cur_hash->next);
               }
               cur_hash = cur_hash->next; 
            }
            if (nodes != 0) total += nodes; 
            if (nodes > max) {
               max = nodes;
            }
            if (detail > 1) {
               CCAPPDEBUG(DEB_F_PREFIX"i: %d\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), i); 
            }
         }
      }
      avg = (double)(total) / (double)(used);
      CCAPPDEBUG(DEB_F_PREFIX"total: %d\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), total);
      CCAPPDEBUG(DEB_F_PREFIX"max: %d\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), max);
      CCAPPDEBUG(DEB_F_PREFIX"used: %lf\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), 100 * ((double)(used) / (double)(HASHBUCKETS)));
      CCAPPDEBUG(DEB_F_PREFIX"average: %lf\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), avg);
   }
}

int main()
{
  static const char *fname="main";
  hashItr_t itr;
  void * data;

  addhash(0x01010001,0x1234);
  addhash(0x01060001,0x4567);
  addhash(0x01060002,0x9324);
  addhash(0x01070002,0x4321);
  addhash(0x01070004,0x2134);
  addhash(0x01080005,0x1324);
  addhash(0x01030001,0x1243);
  hashstats(7);

  hashItrInit(&itr);
  while ( data = hashItrNext(&itr) ) {
    CCAPPDEBUG(DEB_F_PREFIX"Itr found %lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), data);
  }

  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01010001));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01060001));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01060002));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01070002));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01070004));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01080005));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01030001));

  delhash(0x01030001);
  delhash(0x01060001);
  hashstats(7);

  hashItrInit(&itr);
  while ( data = hashItrNext(&itr) ) {
    CCAPPDEBUG(DEB_F_PREFIX"Itr found %lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HAS, fname), data);
  }

  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01010001));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01060001));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01060002));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01070002));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01070004));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01080005));
  CCAPPDEBUG(DEB_F_PREFIX"%lx\n", DEB_F_PREFIX_ARGS(SIP_SES_HASH, fname), findhash(0x01030001));
}
#endif

