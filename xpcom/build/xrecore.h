/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef xrecore_h__
#define xrecore_h__

#include "nscore.h"

/**
 * Import/export macros for libXUL APIs.
 */
#ifdef XPCOM_GLUE
#define XRE_API(type, name, params) \
  typedef type (NS_FROZENCALL * name##Type) params; \
  extern name##Type name NS_HIDDEN;
#elif defined(IMPL_XREAPI)
#define XRE_API(type, name, params) EXPORT_XPCOM_API(type) name params;
#else
#define XRE_API(type, name, params) IMPORT_XPCOM_API(type) name params;
#endif

#endif // xrecore_h__
