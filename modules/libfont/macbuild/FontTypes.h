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

#ifndef FONTTYPES_H_
#define FONTTYPES_H_

#include "ntypes.h"

#include "Mnfrc.h"     /* before Mnffbc to stop warnings about nfrc */
#include "Mnffmi.h"    /* before Mnffbc to stop warnings about nffmi */
#include "Mnffp.h"
#include "Mnff.h"      /* before Mnfdoer to stop warnings about nff */
#include "Mnfdoer.h"   /* before Mnffbc to stop warnings about nfdoer */
#include "Mnfrf.h"     /* before Mnffbc to stop warnings about nfrf */
#include "Mnffbc.h"    /* font consumer interface definitions */
#include "nf.h"
#include "Mnffbu.h"
#include "Mnffbp.h"

typedef struct nffbc *FontBrokerHandle;
typedef struct nffbp *FontBrokerDisplayerHandle;
typedef struct nffbu *FontBrokerUtilityHandle;
typedef struct nff *WebFontHandle;
typedef struct nfdoer *DoneObservableHandle;
typedef struct nfrf *RenderableFontHandle;
typedef struct nfrc *RenderingContextHandle;
typedef struct nffmi *FontMatchInfoHandle;
typedef struct nffp *FontDisplayerHandle;

#endif	/*	FONTTYPES_H_ */
