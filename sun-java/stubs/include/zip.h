/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef _ZIP_H_
#define _ZIP_H_

#include "bool.h"
#include "typedefs.h"
#include "jri.h"

typedef void zip_t;

struct stat;

JRI_PUBLIC_API(void)
zip_close(zip_t *zip);

JRI_PUBLIC_API(bool_t)
zip_get(zip_t *zip, const char *fn, void HUGEP *buf, int32_t len);

JRI_PUBLIC_API(zip_t *)
zip_open(const char *fn);

JRI_PUBLIC_API(bool_t)
zip_stat(zip_t *zip, const char *fn, struct stat *sbuf);

#endif /* !_ZIP_H_ */ 
