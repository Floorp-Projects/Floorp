/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Implements a link between the (cfb) CFontBroker Interface implementation
 * and its C++ implementation viz (fb) FontBrokerObject.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _Pcfb_H_
#define _Pcfb_H_

#include "libfont.h"
#include "nf.h"				/* For nfFontObserverCallback. This has to be
							 * included before Mcfb.h
							 */

#include "Mnffmi.h"
#include "Mnfrc.h"
#include "Mnfdoer.h"
#include "Mnfrf.h"
#include "Mnffbc.h"
#include "Mcfb.h"			/* Generated header */


struct cfbImpl {
  cfbImplHeader	header;
  void *object;				/* FontBrokerObject * */
};

/* The generated getInterface used the wrong object IDS. So we
 * override them with ours.
 */
#define OVERRIDE_cfb_getInterface

/* The generated finalize doesn't have provision to free the
 * private data that we create inside the object. So we
 * override the finalize method and implement destruction
 * of our private data.
 */
#define OVERRIDE_cfb_finalize
#endif /* _Pcfb_H_ */
