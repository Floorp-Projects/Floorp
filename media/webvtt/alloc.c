/**
 * Copyright (c) 2013 Mozilla Foundation and Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  - Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <webvtt/util.h>
#include <stdlib.h>
#include <string.h>

static void *default_alloc( void *unused, webvtt_uint nb );
static void default_free( void *unused, void *ptr );

struct {
  /**
   * Number of allocated objects. Forbid changing the allocator if this is not
   * equal to 0 
   */
  webvtt_uint n_alloc;
  webvtt_alloc_fn_ptr alloc;
  webvtt_free_fn_ptr free;
  void *alloc_data;
} allocator = { 0, default_alloc, default_free, 0 };

static void *WEBVTT_CALLBACK
default_alloc( void *unused, webvtt_uint nb )
{
  (void)unused;
  return malloc( nb );
}

static void WEBVTT_CALLBACK
default_free( void *unused, void *ptr )
{
  (void)unused;
  free( ptr );
}

WEBVTT_EXPORT void
webvtt_set_allocator( webvtt_alloc_fn_ptr alloc, webvtt_free_fn_ptr free,
                      void *userdata )
{
  /**
   * TODO:
   * This really needs a lock. But then, so does all the allocation/free
   * functions...
   * that could be a problem.
   */
  if( allocator.n_alloc == 0 ) {
    if( alloc && free ) {
      allocator.alloc = alloc;
      allocator.free = free;
      allocator.alloc_data = userdata;
    } else if( !alloc && !free ) {
      allocator.alloc = &default_alloc;
      allocator.free = &default_free;
      allocator.alloc_data = 0;
    }
  }
}

/**
 * public alloc/dealloc functions
 */
WEBVTT_EXPORT void *
webvtt_alloc( webvtt_uint nb )
{
  void *ret = allocator.alloc( allocator.alloc_data, nb );
  if( ret )
  { ++allocator.n_alloc; }
  return ret;
}

WEBVTT_EXPORT void *
webvtt_alloc0( webvtt_uint nb )
{
  void *ret = allocator.alloc( allocator.alloc_data, nb );
  if( ret ) {
    ++allocator.n_alloc;
    memset( ret, 0, nb );
  }
  return ret;
}

WEBVTT_EXPORT void
webvtt_free( void *data )
{
  if( data && allocator.n_alloc ) {
    allocator.free( allocator.alloc_data, data );
    --allocator.n_alloc;
  }
}
