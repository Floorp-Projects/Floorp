/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_LINUX_STRING_H_
#define _CPR_LINUX_STRING_H_

#include <string.h>
#include <ctype.h>

/**
 * cpr_strdup
 *
 * @brief The CPR wrapper for strdup

 * The cpr_strdup shall return a pointer to a new string, which is a duplicate
 * of the string pointed to by "str" argument. A null pointer is returned if the
 * new string cannot be created.
 *
 * @param[in] str  - The string that needs to be duplicated
 *
 * @return The duplicated string or NULL in case of no memory
 *
 */
char *
cpr_strdup(const char *str);



#endif
