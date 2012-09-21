/*
 * rand_source.c
 *
 * implements a random source based on /dev/random
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *	
 * Copyright(c) 2001-2006 Cisco Systems, Inc.
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

#include "config.h"

#ifdef DEV_URANDOM
# include <fcntl.h>          /* for open()  */
# include <unistd.h>         /* for close() */
#elif defined(HAVE_RAND_S)
# define _CRT_RAND_S
# include <stdlib.h>         
#else
# include <stdio.h>
#endif

#include "rand_source.h"


/* 
 * global dev_rand_fdes is file descriptor for /dev/random 
 * 
 * This variable is also used to indicate that the random source has
 * been initialized.  When this variable is set to the value of the
 * #define RAND_SOURCE_NOT_READY, it indicates that the random source
 * is not ready to be used.  The value of the #define
 * RAND_SOURCE_READY is for use whenever that variable is used as an
 * indicator of the state of the random source, but not as a file
 * descriptor.
 */

#define RAND_SOURCE_NOT_READY (-1)
#define RAND_SOURCE_READY     (17)

static int dev_random_fdes = RAND_SOURCE_NOT_READY;


err_status_t
rand_source_init(void) {
  if (dev_random_fdes >= 0) {
    /* already open */
    return err_status_ok;
  }
#ifdef DEV_URANDOM
  /* open random source for reading */
  dev_random_fdes = open(DEV_URANDOM, O_RDONLY);
  if (dev_random_fdes < 0)
    return err_status_init_fail;
#elif defined(HAVE_RAND_S)
  dev_random_fdes = RAND_SOURCE_READY;
#else
  /* no random source available; let the user know */
  fprintf(stderr, "WARNING: no real random source present!\n");
  dev_random_fdes = RAND_SOURCE_READY;
#endif
  return err_status_ok;
}

err_status_t
rand_source_get_octet_string(void *dest, uint32_t len) {

  /* 
   * read len octets from /dev/random to dest, and
   * check return value to make sure enough octets were
   * written 
   */
#ifdef DEV_URANDOM
  uint8_t *dst = (uint8_t *)dest;
  while (len)
  {
    ssize_t num_read = read(dev_random_fdes, dst, len);
    if (num_read <= 0 || num_read > len)
      return err_status_fail;
    len -= num_read;
    dst += num_read;
  }
#elif defined(HAVE_RAND_S)
  uint8_t *dst = (uint8_t *)dest;
  while (len)
  {
    unsigned int val;
    errno_t err = rand_s(&val);

    if (err != 0)
      return err_status_fail;
  
    *dst++ = val & 0xff;
    len--;
  }
#else
  /* Generic C-library (rand()) version */
  /* This is a random source of last resort */
  uint8_t *dst = (uint8_t *)dest;
  while (len)
  {
	  int val = rand();
	  /* rand() returns 0-32767 (ugh) */
	  /* Is this a good enough way to get random bytes?
	     It is if it passes FIPS-140... */
	  *dst++ = val & 0xff;
	  len--;
  }
#endif
  return err_status_ok;
}
 
err_status_t
rand_source_deinit(void) {
  if (dev_random_fdes < 0)
    return err_status_dealloc_fail;  /* well, we haven't really failed, *
				      * but there is something wrong    */
#ifdef DEV_URANDOM
  close(dev_random_fdes);  
#endif
  dev_random_fdes = RAND_SOURCE_NOT_READY;
  
  return err_status_ok;  
}
