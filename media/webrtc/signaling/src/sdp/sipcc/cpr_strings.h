/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_STRINGS_H_
#define _CPR_STRINGS_H_

#include "cpr_types.h"

__BEGIN_DECLS


#include <string.h>

#ifdef _MSC_VER

#define cpr_strcasecmp _stricmp
#define cpr_strncasecmp _strnicmp
#define snprintf _snprintf

#else // _MSC_VER

#define cpr_strcasecmp  strcasecmp
#define cpr_strncasecmp strncasecmp

#endif // _MSC_VER

__END_DECLS

#endif
