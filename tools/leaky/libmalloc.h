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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Kipp E.B. Hickman <kipp@netscape.com> (original author)
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

#ifndef libmalloc_h___
#define libmalloc_h___

#include <sys/types.h>
#include <malloc.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"

typedef unsigned long u_long;

// Format of a malloc log entry. This is whats written out to the
// "malloc-log" file.
struct malloc_log_entry {
  u_long type;
  u_long address;
  u_long size;
  u_long oldaddress;
  u_long numpcs;
  char* pcs[MAX_STACK_CRAWL];
};

// type's
#define malloc_log_malloc  0
#define malloc_log_realloc 1
#define malloc_log_free    2
#define malloc_log_new     3
#define malloc_log_delete  4
#define malloc_log_addref  5
#define malloc_log_release 6

// Format of a malloc map entry; after this struct is nameLen+1 bytes of
// name data.
struct malloc_map_entry {
  u_long nameLen;
  u_long address;		// base address
};

// A method that can be called if you want to programmatically control
// the malloc logging. Note that you must link with the library to do
// this (or use dlsym after dynamically loading the library...)
extern u_long SetMallocFlags(u_long flags);

// The environment variable LIBMALLOC_LOG should be set to an integer
// value whose meaning is as follows:

// Enable logging
#define LIBMALLOC_LOG    0x1

// Don't free memory when set
#define LIBMALLOC_NOFREE 0x2

// Check heap for corruption after every malloc/free/realloc
#define LIBMALLOC_CHECK  0x4

// Log reference count calls (addref/release)
#define LIBMALLOC_LOG_RC 0x8

void __log_addref(void* p, int oldrc, int newrc);
void __log_release(void* p, int oldrc, int newrc);

#ifdef __cplusplus
}; /* end of extern "C" */
#endif

#endif /* libmalloc_h___ */
