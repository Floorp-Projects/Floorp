/*
 * rand_source.h
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


#ifndef RAND_SOURCE
#define RAND_SOURCE

#include "err.h"
#include "datatypes.h"

err_status_t
rand_source_init(void);

/*
 * rand_source_get_octet_string() writes a random octet string.
 *
 * The function call rand_source_get_octet_string(dest, len) writes
 * len octets of random data to the location to which dest points,
 * and returns an error code.  This error code should be checked,
 * and if a failure is reported, the data in the buffer MUST NOT
 * be used.
 * 
 * warning: If the return code is not checked, then non-random
 *          data may inadvertently be used.
 *
 * returns:
 *     - err_status_ok    if no problems occured.
 *     - [other]          a problem occured, and no assumptions should
 *                        be made about the contents of the destination
 *                        buffer.
 */

err_status_t
rand_source_get_octet_string(void *dest, uint32_t length);

err_status_t
rand_source_deinit(void);

/* 
 * function prototype for a random source function
 *
 * A rand_source_func_t writes num_octets at the location indicated by
 * dest and returns err_status_ok.  Any other return value indicates
 * failure.
 */

typedef err_status_t (*rand_source_func_t)
     (void *dest, uint32_t num_octets);

#endif /* RAND_SOURCE */
