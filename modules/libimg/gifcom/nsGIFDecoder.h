/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation..
 * Portions created by Netscape Communications Corporation. are
 * Copyright (C) 1998 Netscape Communications Corporation.. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

/* -*- Mode: C; tab-width: 4 -*-
 *   nsGIFDecoder.cpp --- interface to gif decoder
 */
#ifndef _nsGIFDec_h
#define _nsGIFDec_h

#include "nsISupports.h"
#include "nsIImgDecoder.h"
#include "gif.h"

#define NS_GIFDECODER_CID \
{ 0x0d471b70, 0xbaf5, 0x11d2, \
{ 0x80, 0x2c, 0x00, 0x60, 0x08, 0x8f, 0x91, 0xa3 } }

//////////////////////////////////////////////////////////////////////
// GIF Decoder Definition

class GIFDecoder : public nsIImgDecoder   
{
public:
  GIFDecoder(il_container* aContainer);
  virtual ~GIFDecoder();
 
  NS_DECL_ISUPPORTS

  static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  /* stream */
  NS_IMETHOD ImgDInit();

  NS_IMETHOD ImgDWriteReady(PRUint32 *max_read);
  NS_IMETHOD ImgDWrite(const unsigned char *buf, int32 len);
  NS_IMETHOD ImgDComplete();
  NS_IMETHOD ImgDAbort();

  NS_IMETHOD_(il_container *) SetContainer(il_container *ic){ilContainer = ic; return ic;}
  NS_IMETHOD_(il_container *) GetContainer() {return ilContainer;}

  
private:
  il_container* ilContainer;
};

#endif
