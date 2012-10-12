/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_STRINGS_H_
#define _CPR_STRINGS_H_

#include "cpr_types.h"

__BEGIN_DECLS

#if defined SIP_OS_LINUX
#include "../linux/cpr_linux_strings.h"
#elif defined SIP_OS_WINDOWS
#include "../win32/cpr_win_strings.h"
#elif defined SIP_OS_OSX
#include "../darwin/cpr_darwin_strings.h"
#endif

#ifdef CPR_USE_OS_STRCASECMP
/* Use standard library types */
#define cpr_strcasecmp  strcasecmp
#define cpr_strncasecmp strncasecmp
#else
/* Prototypes */
/**
 * cpr_strcasecmp
 *
 * @brief The CPR wrapper for strcasecmp
 *
 * The cpr_strcasecmp performs case insensitive string comparison of the "s1"
 * and the "s2" strings.
 *
 * @param[in] s1  - The first string
 * @param[in] s2  - The second string
 *
 * @return integer <,=,> 0 depending on whether s1 is <,=,> s2
 */
int cpr_strcasecmp(const char *s1, const char *s2);

/**
 * cpr_strncasecmp
 *
 * @brief The CPR wrapper for strncasecmp
 *
 * The cpr_strncasecmp performs case insensitive string comparison for specific
 * length "len".
 *
 * @param[in] s1  - The first string
 * @param[in] s2  - The second string
 * @param[in] len  - The length to be compared
 *
 * @return integer <,=,> 0 depending on whether s1 is <,=,> s2
 */
int cpr_strncasecmp(const char *s1, const char *s2, size_t len);
#endif

__END_DECLS

#endif
