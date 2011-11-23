/* -*- Mode: C; tab-width: 8; c-basic-offset: 8 -*- */
/* vim:set softtabstop=8 shiftwidth=8: */
/*-
 * Copyright (C) 2006-2008 Jason Evans <jasone@FreeBSD.org>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified other than the possible
 *    addition of one or more copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The purpose of these structs is described in jemalloc.c, in the comment
 * marked MALLOC_ZONE_T_NOTE.
 *
 * We need access to some structs that come with a specific version of OSX 
 * but can't copy them here because of licensing restrictions (see bug
 * 603655). The structs below are equivalent in that they'll always be
 * compiled to the same representation on all platforms.
 *
 * `void*` and `void (*)()` may not be the same size on weird platforms, but
 * the size of a function pointer shouldn't vary according to its parameters
 * or return type.
 *
 * Apple's version of these structures, complete with member names and
 * comments, is available online at
 *
 * http://www.opensource.apple.com/source/Libc/Libc-763.12/include/malloc/malloc.h
 *
 */

/*
 * OSX 10.5 - Leopard
 */
typedef struct _leopard_malloc_zone {
 	void *m1;
	void *m2;
	void (*m3)();
	void (*m4)();
	void (*m5)();
	void (*m6)();
	void (*m7)();
	void (*m8)();
	void (*m9)();
	void *m10;
	void (*m11)();
	void (*m12)();
	void *m13;
	unsigned m14;
} leopard_malloc_zone;

/*
 * OSX 10.6 - Snow Leopard
 */
typedef struct _snow_leopard_malloc_zone {
	void *m1;
	void *m2;
	void (*m3)();
	void (*m4)();
	void (*m5)();
	void (*m6)();
	void (*m7)();
	void (*m8)();
	void (*m9)();
	void *m10;
	void (*m11)();
	void (*m12)();
	void *m13;
	unsigned m14;
	void (*m15)(); // this member added in 10.6
	void (*m16)(); // this member added in 10.6
} snow_leopard_malloc_zone;

typedef struct _snow_leopard_malloc_introspection {
    void (*m1)();
    void (*m2)();
    void (*m3)();
    void (*m4)();
    void (*m5)();
    void (*m6)();
    void (*m7)();
    void (*m8)();
    void (*m9)(); // this member added in 10.6
} snow_leopard_malloc_introspection;

/*
 * OSX 10.7 - Lion
 */
typedef struct _lion_malloc_zone {
	void *m1;
	void *m2;
	void (*m3)();
	void (*m4)();
	void (*m5)();
	void (*m6)();
	void (*m7)();
	void (*m8)();
	void (*m9)();
	void *m10;
	void (*m11)();
	void (*m12)();
	void *m13;
	unsigned m14;
	void (*m15)();
	void (*m16)();
	void (*m17)(); // this member added in 10.7
} lion_malloc_zone;

typedef struct _lion_malloc_introspection {
    void (*m1)();
    void (*m2)();
    void (*m3)();
    void (*m4)();
    void (*m5)();
    void (*m6)();
    void (*m7)();
    void (*m8)();
    void (*m9)();
    void (*m10)(); // this member added in 10.7
    void (*m11)(); // this member added in 10.7
    void (*m12)(); // this member added in 10.7
#ifdef __BLOCKS__
    void (*m13)(); // this member added in 10.7
#else
    void *m13; // this member added in 10.7
#endif
} lion_malloc_introspection;
