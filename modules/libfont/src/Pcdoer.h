/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
/* 
 *
 * Private implementation of the Observer.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _Pcdoer_H_
#define _Pcdoer_H_

#include "string.h"			/* Mcf.c uses memcpy, memcmp etc. And it includes
							   only this one header. */
#include "nf.h"				/* For nfFontObserverCallback */
#include "Mcdoer.h"			/* Generated header */

struct cdoerImpl {
  cdoerImplHeader	header;

  nfFontObserverCallback update_callback;
  void *client_data;
};

/* The generated getInterface used the wrong object IDS. So we
 * override them with ours.
 */
#define OVERRIDE_cdoer_getInterface

/* The generated finalize doesn't have provision to free the
 * private data that we create inside the object. So we
 * override the finalize method and implement destruction
 * of our private data.
 */
#define OVERRIDE_cdoer_finalize

#endif /* _Pcdoer_H_ */

