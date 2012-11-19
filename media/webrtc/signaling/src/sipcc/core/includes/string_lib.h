/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef  _STRING_LIB_INCLUDED_H /* allows multiple inclusion */
#define  _STRING_LIB_INCLUDED_H

#include "cpr_types.h"

#define LEN_UNKNOWN -1

typedef struct string_block_t_
{
    struct string_block_t_ *next;
    uint16_t    refcount;
    uint16_t    length;
    const char *fname;
    int         line;
    short       signature;
    char        data[1];
} string_block_t;


/*
 * Prototypes for functions
 */

string_t strlib_malloc(const char *str, int length, const char *fname,
                       int line);
string_t strlib_copy(string_t str);
string_t strlib_update(string_t destination, const char *source,
                       const char *fname, int line);
string_t strlib_append(string_t str, const char *toappend_str,
                       const char *fname, int line);
string_t strlib_prepend(string_t str, const char *toprepend_str,
                        const char *fname, int line);
void strlib_free(string_t str);
char *strlib_open(string_t str, int length, const char *fname, int line);
string_t strlib_close(char *str);
string_t strlib_printf(const char *format, ...);
string_t strlib_empty(void);
void strlib_debug_init(void);
long strlib_mem_used(void);
int strlib_test_memory_is_string(void *mem);
void strlib_init (void);

#ifndef __STRINGLIB_INTERNAL__
#define strlib_malloc(x,y) strlib_malloc(x,y,__FILE__,__LINE__)
#define strlib_update(x,y) strlib_update(x,y,__FILE__,__LINE__)
#define strlib_append(x,y) strlib_append(x,y,__FILE__,__LINE__)
#define strlib_prepend(x,y) strlib_prepend(x,y,__FILE__,__LINE__)
#define strlib_open(x,y) strlib_open(x,y,__FILE__,__LINE__)
#endif


#endif
