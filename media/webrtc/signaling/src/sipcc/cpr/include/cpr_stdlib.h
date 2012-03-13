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

#ifndef _CPR_STDLIB_H_
#define _CPR_STDLIB_H_

#include "cpr_types.h"

__BEGIN_DECLS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_stdlib.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_stdlib.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_stdlib.h"
#endif

#ifdef CPR_USE_DIRECT_OS_CALL
/*
 * The phone expects that malloced memory is zeroed out in some cases.
 * Depending on memory detection techniques and OS used, malloc may
 * be directly mapped to calloc.
 */
#ifdef CPR_USE_CALLOC_FOR_MALLOC
#define cpr_malloc(x)  calloc(1, x)
#else
#define cpr_malloc  malloc
#endif
#define cpr_calloc  calloc
#define cpr_realloc realloc
#define cpr_strdup  _strdup
#define cpr_free    free

#define CPR_REACH_MEMORY_HIGH_WATER_MARK FALSE

#else
/**
 * @brief Allocate memory
 *
 * The cpr_malloc function attempts to malloc a memory block of at least size
 * bytes. 
 *
 * @param[in] size  size in bytes to allocate
 *
 * @return      pointer to allocated memory or #NULL
 *
 * @note        memory is NOT initialized
 */
void *cpr_malloc(size_t size);

/**
 * @brief Allocate and initialize memory
 *
 * The cpr_calloc function attempts to calloc "num" number of memory block of at
 * least size bytes. The code zeros out the memory block before passing 
 * the pointer back to the calling thread.
 *
 * @param[in] nelem   number of objects to allocate
 * @param[in] size  size in bytes of an object
 *
 * @return      pointer to allocated memory or #NULL
 *
 * @note        allocated memory is initialized to zero(0)
 */
void *cpr_calloc(size_t nelem, size_t size);

/**
 * @brief Reallocate memory
 *
 * @param[in] object  memory to reallocate
 * @param[in] size new memory size
 *
 * @return     pointer to reallocated memory or #NULL
 *
 * @note       if either mem is NULL or size is zero
 *             the return value will be NULL
 */
void *cpr_realloc(void *object, size_t size);

/**
 * cpr_strdup
 *
 * @brief The CPR wrapper for strdup

 * The cpr_strdup shall return a pointer to a new string, which is a duplicate
 * of the string pointed to by "str" argument. A null pointer is returned if the
 * new string cannot be created.
 *
 * @param[in] string  - The string that needs to be duplicated
 *
 * @return The duplicated string or NULL in case of no memory
 *
 */
char *cpr_strdup(const char *string);

/**
 * @brief Free memory
 * The cpr_free function attempts to free the memory block passed in.
 *
 * @param[in] mem  memory to free
 *
 * @return     none
 */
void  cpr_free(void *mem);
#endif

__END_DECLS

#endif


