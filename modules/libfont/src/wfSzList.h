/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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

