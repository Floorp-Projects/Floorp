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
