/**
   r_assoc.c


   Copyright (C) 2002-2003, Network Resonance, Inc.
   Copyright (C) 2006, Network Resonance, Inc.
   All Rights Reserved

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. Neither the name of Network Resonance, Inc. nor the name of any
      contributors to this software may be used to endorse or promote
      products derived from this software without specific prior written
      permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.


 */

/**
   r_assoc.c

   This is an associative array implementation, using an open-chained
   hash bucket technique.

   Note that this implementation permits each data entry to have
   separate copy constructors and destructors. This currently wastes
   space, but could be implemented while saving space by using
   the high order bit of the length value or somesuch.

   The major problem with this code is it's not resizable, though it
   could be made so.


   Copyright (C) 1999-2000 RTFM, Inc.
   All Rights Reserved

   This package is a SSLv3/TLS protocol analyzer written by Eric Rescorla
   <ekr@rtfm.com> and licensed by RTFM, Inc.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. All advertising materials mentioning features or use of this software
      must display the following acknowledgement:

      This product includes software developed by Eric Rescorla for
      RTFM, Inc.

   4. Neither the name of RTFM, Inc. nor the name of Eric Rescorla may be
      used to endorse or promote products derived from this
      software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY ERIC RESCORLA AND RTFM, INC. ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY SUCH DAMAGE.

   $Id: r_assoc.c,v 1.4 2007/06/08 17:41:49 adamcain Exp $

   ekr@rtfm.com  Sun Jan 17 17:57:15 1999
 */

static char *RCSSTRING __UNUSED__ ="$Id: r_assoc.c,v 1.4 2007/06/08 17:41:49 adamcain Exp $";

#include <r_common.h>
#include <string.h>
#include "r_assoc.h"

typedef struct r_assoc_el_ {
     char *key;
     int key_len;
     void *data;
     struct r_assoc_el_ *prev;
     struct r_assoc_el_ *next;
     int (*copy)(void **n,void *old);
     int (*destroy)(void *ptr);
} r_assoc_el;

struct r_assoc_ {
     int size;
     int bits;
     int (*hash_func)(char *key,int len,int size);
     r_assoc_el **chains;
     UINT4 num_elements;
};

#define DEFAULT_TABLE_BITS 5

static int destroy_assoc_chain(r_assoc_el *chain);
static int r_assoc_fetch_bucket(r_assoc *assoc,
  char *key,int len,r_assoc_el **bucketp);
static int copy_assoc_chain(r_assoc_el **knewp, r_assoc_el *old);

int r_assoc_create(assocp,hash_func,bits)
  r_assoc **assocp;
  int (*hash_func)(char *key,int len,int size);
  int bits;
  {
    r_assoc *assoc=0;
    int _status;

    if(!(assoc=(r_assoc *)RCALLOC(sizeof(r_assoc))))
      ABORT(R_NO_MEMORY);
    assoc->size=(1<<bits);
    assoc->bits=bits;
    assoc->hash_func=hash_func;

    if(!(assoc->chains=(r_assoc_el **)RCALLOC(sizeof(r_assoc_el *)*
      assoc->size)))
      ABORT(R_NO_MEMORY);

    *assocp=assoc;

    _status=0;
  abort:
    if(_status){
      r_assoc_destroy(&assoc);
    }
    return(_status);
  }

int r_assoc_destroy(assocp)
  r_assoc **assocp;
  {
    r_assoc *assoc;
    int i;

    if(!assocp || !*assocp)
      return(0);

    assoc=*assocp;
    for(i=0;i<assoc->size;i++)
      destroy_assoc_chain(assoc->chains[i]);

    RFREE(assoc->chains);
    RFREE(*assocp);

    return(0);
  }

static int destroy_assoc_chain(chain)
  r_assoc_el *chain;
  {
    r_assoc_el *nxt;

    while(chain){
      nxt=chain->next;

      if(chain->destroy)
	chain->destroy(chain->data);

      RFREE(chain->key);

      RFREE(chain);
      chain=nxt;
    }

    return(0);
  }

static int copy_assoc_chain(knewp,old)
  r_assoc_el **knewp;
  r_assoc_el *old;
  {
    r_assoc_el *knew=0,*ptr,*tmp;
    int r,_status;

    ptr=0; /* Pacify GCC's uninitialized warning.
              It's not correct */
    if(!old) {
      *knewp=0;
      return(0);
    }
    for(;old;old=old->next){
      if(!(tmp=(r_assoc_el *)RCALLOC(sizeof(r_assoc_el))))
	ABORT(R_NO_MEMORY);

      if(!knew){
	knew=tmp;
	ptr=knew;
      }
      else{
	ptr->next=tmp;
	tmp->prev=ptr;
        ptr=tmp;
      }

      ptr->destroy=old->destroy;
      ptr->copy=old->copy;

      if(old->copy){
	if(r=old->copy(&ptr->data,old->data))
	  ABORT(r);
      }
      else
	ptr->data=old->data;

      if(!(ptr->key=(char *)RMALLOC(old->key_len)))
	ABORT(R_NO_MEMORY);
      memcpy(ptr->key,old->key,ptr->key_len=old->key_len);
    }

    *knewp=knew;

    _status=0;
  abort:
    if(_status){
      destroy_assoc_chain(knew);
    }
    return(_status);
  }

static int r_assoc_fetch_bucket(assoc,key,len,bucketp)
  r_assoc *assoc;
  char *key;
  int len;
  r_assoc_el **bucketp;
  {
    UINT4 hash_value;
    r_assoc_el *bucket;

    hash_value=assoc->hash_func(key,len,assoc->bits);

    for(bucket=assoc->chains[hash_value];bucket;bucket=bucket->next){
      if(bucket->key_len == len && !memcmp(bucket->key,key,len)){
	*bucketp=bucket;
	return(0);
      }
    }

    return(R_NOT_FOUND);
  }

int r_assoc_fetch(assoc,key,len,datap)
  r_assoc *assoc;
  char *key;
  int len;
  void **datap;
  {
    r_assoc_el *bucket;
    int r;

    if(r=r_assoc_fetch_bucket(assoc,key,len,&bucket)){
      if(r!=R_NOT_FOUND)
	ERETURN(r);
      return(r);
    }

    *datap=bucket->data;
    return(0);
  }

int r_assoc_insert(assoc,key,len,data,copy,destroy,how)
  r_assoc *assoc;
  char *key;
  int len;
  void *data;
  int (*copy)(void **knew,void *old);
  int (*destroy)(void *ptr);
  int how;
  {
    r_assoc_el *bucket,*new_bucket=0;
    int r,_status;

    if(r=r_assoc_fetch_bucket(assoc,key,len,&bucket)){
      /*Note that we compute the hash value twice*/
      UINT4 hash_value;

      if(r!=R_NOT_FOUND)
	ABORT(r);
      hash_value=assoc->hash_func(key,len,assoc->bits);

      if(!(new_bucket=(r_assoc_el *)RCALLOC(sizeof(r_assoc_el))))
	ABORT(R_NO_MEMORY);
      if(!(new_bucket->key=(char *)RMALLOC(len)))
	ABORT(R_NO_MEMORY);
      memcpy(new_bucket->key,key,len);
      new_bucket->key_len=len;

      /*Insert at the list head. Is FIFO a good algorithm?*/
      if(assoc->chains[hash_value])
        assoc->chains[hash_value]->prev=new_bucket;
      new_bucket->next=assoc->chains[hash_value];
      assoc->chains[hash_value]=new_bucket;
      bucket=new_bucket;
    }
    else{
      if(!(how&R_ASSOC_REPLACE))
	ABORT(R_ALREADY);

      if(bucket->destroy)
	bucket->destroy(bucket->data);
    }

    bucket->data=data;
    bucket->copy=copy;
    bucket->destroy=destroy;
    assoc->num_elements++;

    _status=0;
  abort:
    if(_status && new_bucket){
      RFREE(new_bucket->key);
      RFREE(new_bucket);
    }
    return(_status);
  }

int r_assoc_delete(assoc,key,len)
  r_assoc *assoc;
  char *key;
  int len;
  {
    int r;
    r_assoc_el *bucket;
    UINT4 hash_value;

    if(r=r_assoc_fetch_bucket(assoc,key,len,&bucket)){
      if(r!=R_NOT_FOUND)
	ERETURN(r);
      return(r);
    }

    /* Now remove the element from the hash chain */
    if(bucket->prev){
      bucket->prev->next=bucket->next;
    }
    else {
      hash_value=assoc->hash_func(key,len,assoc->bits);
      assoc->chains[hash_value]=bucket->next;
    }

    if(bucket->next)
      bucket->next->prev=bucket->prev;

    /* Remove the data */
    if(bucket->destroy)
      bucket->destroy(bucket->data);

    RFREE(bucket->key);
    RFREE(bucket);
    assoc->num_elements--;

    return(0);
  }

int r_assoc_copy(knewp,old)
  r_assoc **knewp;
  r_assoc *old;
  {
    int r,_status,i;
    r_assoc *knew;

    if(!(knew=(r_assoc *)RCALLOC(sizeof(r_assoc))))
      ABORT(R_NO_MEMORY);
    knew->size=old->size;
    knew->bits=old->bits;
    knew->hash_func=old->hash_func;

    if(!(knew->chains=(r_assoc_el **)RCALLOC(sizeof(r_assoc_el)*old->size)))
      ABORT(R_NO_MEMORY);
    for(i=0;i<knew->size;i++){
      if(r=copy_assoc_chain(knew->chains+i,old->chains[i]))
	ABORT(r);
    }
    knew->num_elements=old->num_elements;

    *knewp=knew;

    _status=0;
  abort:
    if(_status){
      r_assoc_destroy(&knew);
    }
    return(_status);
  }

int r_assoc_num_elements(r_assoc *assoc,int *sizep)
  {
    *sizep=assoc->num_elements;

    return(0);
  }

int r_assoc_init_iter(assoc,iter)
  r_assoc *assoc;
  r_assoc_iterator *iter;
  {
    int i;

    iter->assoc=assoc;
    iter->prev_chain=-1;
    iter->prev=0;

    iter->next_chain=assoc->size;
    iter->next=0;

    for(i=0;i<assoc->size;i++){
      if(assoc->chains[i]!=0){
	iter->next_chain=i;
	iter->next=assoc->chains[i];
	break;
      }
    }

    return(0);
  }

int r_assoc_iter(iter,key,keyl,val)
  r_assoc_iterator *iter;
  void **key;
  int *keyl;
  void **val;
  {
    int i;
    r_assoc_el *ret;

    if(!iter->next)
      return(R_EOD);
    ret=iter->next;

    *key=ret->key;
    *keyl=ret->key_len;
    *val=ret->data;

    /* Now increment */
    iter->prev_chain=iter->next_chain;
    iter->prev=iter->next;

    /* More on this chain */
    if(iter->next->next){
      iter->next=iter->next->next;
    }
    else{
      iter->next=0;

      /* FInd the next occupied chain*/
      for(i=iter->next_chain+1;i<iter->assoc->size;i++){
	if(iter->assoc->chains[i]){
	  iter->next_chain=i;
	  iter->next=iter->assoc->chains[i];
	  break;
	}
      }
    }

    return(0);
  }

/* Delete the last returned value*/
int r_assoc_iter_delete(iter)
  r_assoc_iterator *iter;
  {
    /* First unhook it from the list*/
    if(!iter->prev->prev){
      /* First element*/
      iter->assoc->chains[iter->prev_chain]=iter->prev->next;
    }
    else{
      iter->prev->prev->next=iter->prev->next;
    }

    if(iter->prev->next){
      iter->prev->next->prev=iter->prev->prev;
    }

    if (iter->prev->destroy)
      iter->prev->destroy(iter->prev->data);

    iter->assoc->num_elements--;
    RFREE(iter->prev->key);
    RFREE(iter->prev);
    return(0);
  }


/*This is a hack from AMS. Supposedly, it's pretty good for strings, even
 though it doesn't take into account all the data*/
int r_assoc_simple_hash_compute(key,len,bits)
  char *key;
  int len;
  int bits;
  {
    UINT4 h=0;

    h=key[0] +(key[len-1] * len);

    h &= (1<<bits) - 1;

    return(h);
  }


int r_crc32(char *data,int len,UINT4 *crcval);

int r_assoc_crc32_hash_compute(data,len,bits)
  char *data;
  int len;
  int bits;
  {
    UINT4 res;
    UINT4 mask;

    /* First compute the CRC value */
    if(r_crc32(data,len,&res))
      ERETURN(R_INTERNAL);

    mask=~(0xffffffff<<bits);

    return(res & mask);
  }
