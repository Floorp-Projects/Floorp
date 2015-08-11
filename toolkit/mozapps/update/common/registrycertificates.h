/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _REGISTRYCERTIFICATES_H_
#define _REGISTRYCERTIFICATES_H_

#include "certificatecheck.h"

BOOL DoesBinaryMatchAllowedCertificates(LPCWSTR basePathForUpdate,
                                        LPCWSTR filePath,
                                        BOOL allowFallbackKeySkip = TRUE);

#endif
