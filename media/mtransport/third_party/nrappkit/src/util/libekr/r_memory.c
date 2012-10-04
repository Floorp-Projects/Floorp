/**
   r_memory.c

   
   Copyright (C) 2004, Network Resonance, Inc.
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
   

   ekr@rtfm.com  Thu Apr 22 20:40:45 2004
 */


static char *RCSSTRING __UNUSED__="$Id: r_memory.c,v 1.2 2006/08/16 19:39:17 adamcain Exp $";

#include <string.h>
#include <stddef.h>
#include <assert.h>
#include "r_common.h"
#include "r_memory.h"

typedef struct r_malloc_chunk_ {
#ifdef SANITY_CHECKS     
     UINT4 hdr;
#endif     
     UCHAR type;
     UINT4 size;
     UCHAR memory[1];
} r_malloc_chunk;

#define CHUNK_MEMORY_OFFSET                    offsetof(struct r_malloc_chunk_, memory)
#define GET_CHUNK_ADDR_FROM_MEM_ADDR(memp) \
        ((struct r_malloc_chunk *)(((unsigned char*)(memp))-CHUNK_MEMORY_OFFSET))
#define CHUNK_SIZE(size) (size+sizeof(r_malloc_chunk))

#define HDR_FLAG 0x464c4147

static UINT4 mem_usage;      /* Includes our header */
static UINT4 mem_stats[256]; /* Does not include our header */

void *r_malloc(type,size)
  int type;
  size_t size;
  {
    size_t total;
    r_malloc_chunk *chunk;
    
    total=size+sizeof(r_malloc_chunk);

    if(!(chunk=malloc(total)))
      return(0);

#ifdef SANITY_CHECKS    
    chunk->hdr=HDR_FLAG;
#endif    
    chunk->type=type;
    chunk->size=size;

    mem_usage+=CHUNK_SIZE(size);
    mem_stats[type]+=size;
    
    return(chunk->memory);
  }

void *r_calloc(type,number,size)
  int type;
  size_t number;
  size_t size;
  {
    void *ret;
    size_t total;

    total=number*size;

    if(!(ret=r_malloc(type,total)))
      return(0);

    memset(ret,0,size);

    return(ret);
  }
     
void r_free(ptr)
  void *ptr;
  {
    r_malloc_chunk *chunk;

    if(!ptr) return;

    chunk=(r_malloc_chunk *)GET_CHUNK_ADDR_FROM_MEM_ADDR(ptr);
#ifdef SANITY_CHECKS
    assert(chunk->hdr==HDR_FLAG);
#endif    

    mem_usage-=CHUNK_SIZE(chunk->size);
    mem_stats[chunk->type]-=chunk->size;
    
    free(chunk);
  }

void *r_realloc(ptr,size)
  void *ptr;
  size_t size;
  {
    r_malloc_chunk *chunk,*nchunk;
    size_t total;
    
    if(!ptr) return(r_malloc(255,size));

    chunk=(r_malloc_chunk *)GET_CHUNK_ADDR_FROM_MEM_ADDR(ptr);
#ifdef SANITY_CHECKS
    assert(chunk->hdr==HDR_FLAG);
#endif    

    total=size + sizeof(r_malloc_chunk);

    if(!(nchunk=realloc(chunk,total)))
      return(0);

    mem_usage-=CHUNK_SIZE(nchunk->size);
    mem_stats[nchunk->type]-=nchunk->size;
    
    nchunk->size=size;
    mem_usage+=CHUNK_SIZE(nchunk->size);
    mem_stats[nchunk->type]+=nchunk->size;
    
    return(nchunk->memory);
  }
      
char *r_strdup(str)
  char *str;
  {
    int len;
    char *nstr;
    
    if(!str)
      return(0);

    len=strlen(str)+1;

    if(!(nstr=r_malloc(0,len)))
      return(0);

    memcpy(nstr,str,len);

    return(nstr);
  }
    
int r_mem_get_usage(usagep)
  UINT4 *usagep;
  {
    *usagep=mem_usage;

    return(0);
  }

int r_memory_dump_stats()
  {
    int i;

    printf("Total memory usage: %d\n",mem_usage);
    printf("Memory usage by bucket\n");
    for(i=0;i<256;i++){
      if(mem_stats[i]){
        printf("%d\t%d\n",i,mem_stats[i]);
      }
    }
    return(0);
  }

void *r_malloc_compat(size)
  size_t size;
  {
    return(r_malloc(255,size));
  }
