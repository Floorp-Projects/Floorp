/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * wfFCache.h (wfFontCache.h)
 *
 * C++ implementation of a Font Cache. This does Font to Fmi
 * mapping. Font <--> Fmi mapping is done by this.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _wfFCache_H_
#define _wfFCache_H_

#include "libfont.h"

// Uses :	FontMatchInfo
//			Font
#include "Mnffmi.h"
#include "Mnff.h"

#include "wfList.h"

struct font_store {
  struct nffmi *fmi;
  struct nff *f;
};

// For now we are implementing the Cache using a List. Nspr has a
// cache that we could use. We will wait for nspr to be Jmc'ized.
//
// Also, this will increment and decrement the reference count of each
// object as it is being serviced.
class wfFontObjectCache : private wfList {
public:
  wfFontObjectCache();
  ~wfFontObjectCache();

  struct nff *RcFmi2Font(struct nfrc *rc, struct nffmi *fmi);

  struct nffmi *Font2Fmi(struct nff *f);

  struct nff *Rf2Font(struct nfrf *rf);


  int add(struct nffmi *fmi, struct nff *f);
  int remove(struct nffmi *fmi, struct nff *f);

  // releaseFont release all references to the Font in the fontObjectCache.
  // It returns negative if there was any problem in releasing references
  // to this Font. Else returns 0.
  int releaseFont(struct nff *f);

  // Iterate through all the font objects and remove the reference
  // to this rf in any of them. Once an rf is removed from any
  // font, the font object cannot be shared any longer.
  //
  // returns the number of rfs that were released
  //
  int releaseRf(struct nfrf *rf);
};
#endif /* _wfFCache_H_ */
