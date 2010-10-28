/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef __LINKER_BA_H
#define __LINKER_BA_H

struct ba_bits {
    unsigned allocated:1;           /* 1 if allocated, 0 if free */
    unsigned order:7;               /* size of the region in ba space */
};

struct ba {
    /* start address of the ba space */
    unsigned long base;
    /* total size of the ba space */
    unsigned long size;
    /* the smaller allocation that can be made */
    unsigned long min_alloc;
    /* the order of the largest allocation that can be made */
    unsigned long max_order;
    /* number of entries in the ba space */
    int num_entries;
    /* the bitmap for the region indicating which entries are allocated
     * and which are free */
    struct ba_bits *bitmap;
};

extern void ba_init(struct ba *ba);
extern int ba_allocate(struct ba *ba, unsigned long len);
extern int ba_free(struct ba *ba, int index);
extern unsigned long ba_start_addr(struct ba *ba, int index);
extern unsigned long ba_len(struct ba *ba, int index);

#endif
