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

#ifndef __STRINGLIB_INTERNAL__
#define strlib_malloc(x,y) strlib_malloc(x,y,__FILE__,__LINE__)
#define strlib_update(x,y) strlib_update(x,y,__FILE__,__LINE__)
#define strlib_append(x,y) strlib_append(x,y,__FILE__,__LINE__)
#define strlib_prepend(x,y) strlib_prepend(x,y,__FILE__,__LINE__)
#define strlib_open(x,y) strlib_open(x,y,__FILE__,__LINE__)
#endif


#endif
