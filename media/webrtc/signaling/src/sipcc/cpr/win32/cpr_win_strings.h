/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CPR_WIN32_STRINGS_H_
#define CPR_WIN32_STRINGS_H_

#include <string.h>

#define cpr_strcasecmp _stricmp
#define cpr_strncasecmp _strnicmp

#define CPR_USE_OS_STRCASECMP

#endif /* CPR_WIN32_STRINGS_H_ */
