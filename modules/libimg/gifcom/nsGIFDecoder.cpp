/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
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

/*
 *   nsGIFDecoder.cpp --- interface to gif decoder
 */

#include "nsGIFDecoder.h"
#include "nsCOMPtr.h"

//////////////////////////////////////////////////////////////////////
// GIF Decoder Implementation

NS_IMPL_ISUPPORTS1(GIFDecoder, nsIImgDecoder);

GIFDecoder::GIFDecoder(il_container* aContainer)
{
  NS_INIT_REFCNT();
  ilContainer = aContainer;
}

GIFDecoder::~GIFDecoder(void)
{
}


NS_METHOD
GIFDecoder::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsresult rv;
  if (aOuter) return NS_ERROR_NO_AGGREGATION;

  il_container *ic = new il_container();
  if (!ic) return NS_ERROR_OUT_OF_MEMORY;

  GIFDecoder *decoder = new GIFDecoder(ic);
  if (!decoder) {
    delete ic;
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(decoder);
  rv = decoder->QueryInterface(aIID, aResult);
  NS_RELEASE(decoder);

  /* why are we creating and destroying this object for no reason? */
  delete ic; /* is a place holder */

  return rv;
}


NS_IMETHODIMP
GIFDecoder::ImgDInit()
{
   PRBool ret=PR_FALSE;

   if(ilContainer != NULL) {
     ret=il_gif_init(ilContainer);
   } 
   if(ret)
       return NS_OK;
   else
   return NS_ERROR_FAILURE;  
}


NS_IMETHODIMP 
GIFDecoder::ImgDWriteReady(PRUint32 *max_read)
{

  if(ilContainer != NULL) {
     *max_read = il_gif_write_ready(ilContainer);
  }
  return NS_OK;
}

NS_IMETHODIMP
GIFDecoder::ImgDWrite(const unsigned char *buf, int32 len)
{
  int ret = 0;

  if( ilContainer != NULL ) {
     ret = il_gif_write(ilContainer, buf,len);
     if(ret != 0)
         return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
GIFDecoder::ImgDComplete()
{
  if( ilContainer != NULL ) {
     il_gif_complete(ilContainer);
  }
  return NS_OK;
}

NS_IMETHODIMP 
GIFDecoder::ImgDAbort()
{
  if( ilContainer != NULL ) {
    il_gif_abort(ilContainer);
  }
  return NS_OK;
}
 

