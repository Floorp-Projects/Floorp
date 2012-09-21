/*
 * kernel_compat.h
 * 
 * Compatibility stuff for building in kernel context where standard
 * C headers and library are not available.
 *
 * Marcus Sundberg
 * Ingate Systems AB
 */
/*
 *	
 * Copyright(c) 2005 Ingate Systems AB
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
 *   Neither the name of the author(s) nor the names of its
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

#ifndef KERNEL_COMPAT_H
#define KERNEL_COMPAT_H

#ifdef SRTP_KERNEL_LINUX

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/random.h>
#include <linux/byteorder/generic.h>


#define err_report(priority, ...) \
  do {\
    if (priority <= err_level) {\
       printk(__VA_ARGS__);\
    }\
  }while(0)

#define clock()	(jiffies)
#define time(x)	(jiffies)

/* rand() implementation. */
#define RAND_MAX	32767

static inline int rand(void)
{
	uint32_t temp;
	get_random_bytes(&temp, sizeof(temp));
	return temp % (RAND_MAX+1);
}

/* stdio/stdlib implementation. */
#define printf(...)	printk(__VA_ARGS__)
#define exit(n)	panic("%s:%d: exit(%d)\n", __FILE__, __LINE__, (n))

#endif /* SRTP_KERNEL_LINUX */

#endif /* KERNEL_COMPAT_H */
