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
 * wfSzList.h
 *
 * Object local to libfont. This is used to track the association between
 * rc, sizes[] and rf[] for a particular fh in a Font Object.
 *
 * dp Suresh <dp@netscape.com>
 */


#ifndef _wfSzList_H_
#define _wfSzList_H_

#include "libfont.h"

// Uses :	RenderingContext
//			RenderableFont
//			FontDisplayer
#include "Mnfrc.h"
#include "Mnfrf.h"

#include "wffpPeer.h"
#include "wfSzList.h"


class wfSizesList {
private:
  int sizesLen;	// -1 indicates sizes was never intialized.
  jdouble *sizes;
  struct nfrf * *rfs;
  int rfcount;
  int maxrfcount;
  const int rfAllocStep;

protected:
  void freeSizes();

public:
  // Constructor
  wfSizesList();

  // Destructor
  ~wfSizesList();

  // This can be used to see if the sizes list was ever initialized.
  int initialized();

  // WARNING: sizes that is passed in is not copied.
  int addSizes(jdouble *sizes);

  // Returns the array of sizes associated with the rc.
  // WARNING: Caller DO NOT free the sizes that is returned.
  jdouble *getSizes();

  // Remove a particular size from the size list for this rc
  int removeSize(jdouble size);

  // Query if a particular size is available
  int supportsSize(jdouble size);

  // Associate rf with size
  int addRf(struct nfrf *rf);

  // Remove an rf. Returns number of rf's removed. 0 if none.
  int removeRf(struct nfrf *rf);

  // Return zero if the rf does not exist. Else returns the number
  // of times this rf exists.
  int isRfExist(struct nfrf *rf);

  // number of rfs that are in this sizesList. This will be used to know
  // when to release a fonthandle by the FontObject
  int getRfCount();

  // Retrieve the rf that was associated with size
  struct nfrf *getRf(jdouble pointsize);
};

#endif /* _wfSzList_H_ */

