/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
/* 
 * Pcrc.h
 *
 * Implements a link between the (crc) CRenderingContext Interface
 * implementation and its C++ implementation viz (rc) RenderingContextObject.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _Pcrc_H_
#define _Pcrc_H_

#include "Mcrc.h"			/* Generated header */

struct crcImpl {
  crcImplHeader	header;
  void *object;				/* RenderingContextObject * */
};

/* The generated getInterface used the wrong object IDS. So we
 * override them with ours.
 */
#define OVERRIDE_crc_getInterface

/* The generated finalize doesn't have provision to free the
 * private data that we create inside the object. So we
 * override the finalize method and implement destruction
 * of our private data.
 */
#define OVERRIDE_crc_finalize

#endif /* _Pcrc_H_ */
