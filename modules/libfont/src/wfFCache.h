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
