/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1999
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

#ifndef nsUnknownDecoder_h__
#define nsUnknownDecoder_h__

#include "nsIStreamConverter.h"
#include "nsIChannel.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#define NS_UNKNOWNDECODER_CID                        \
{ /* 7d7008a0-c49a-11d3-9b22-0080c7cb1080 */         \
    0x7d7008a0,                                      \
    0xc49a,                                          \
    0x11d3,                                          \
    {0x9b, 0x22, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80}       \
}


class nsUnknownDecoder : public nsIStreamConverter
{
public:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIStreamConverter methods
  NS_DECL_NSISTREAMCONVERTER

  // nsIStreamListener methods
  NS_DECL_NSISTREAMLISTENER

  // nsIRequestObserver methods
  NS_DECL_NSIREQUESTOBSERVER

  nsUnknownDecoder();

protected:
  virtual ~nsUnknownDecoder();

  virtual void DetermineContentType(nsIRequest* aRequest);
  nsresult FireListenerNotifications(nsIRequest* request, nsISupports *aCtxt);

protected:
  nsCOMPtr<nsIStreamListener> mNextListener;

  // Function to use to check whether sniffing some potentially
  // dangerous types (eg HTML) is ok for this request.  We can disable
  // sniffing for local files if needed using this.  Just a security
  // precation thingy... who knows when we suddenly need to flip this
  // pref?
  PRBool AllowSniffing(nsIRequest* aRequest);
  
  // Various sniffer functions.  Returning PR_TRUE means that a type
  // was determined; PR_FALSE means no luck.
  PRBool TryContentSniffers(nsIRequest* aRequest);
  PRBool SniffForHTML(nsIRequest* aRequest);
  PRBool SniffForXML(nsIRequest* aRequest);

  // SniffURI guesses at the content type based on the URI (typically
  // using the extentsion)
  PRBool SniffURI(nsIRequest* aRequest);

  // LastDitchSniff guesses at text/plain vs. application/octet-stream
  // by just looking at whether the data contains null bytes, and
  // maybe at the fraction of chars with high bit set.  Use this only
  // as a last-ditch attempt to decide a content type!
  PRBool LastDitchSniff(nsIRequest* aRequest);

  /**
   * An entry struct for our array of sniffers.  Each entry has either
   * a type associated with it (set these with the SNIFFER_ENTRY macro)
   * or a function to be executed (set these with the
   * SNIFFER_ENTRY_WITH_FUNC macro).  The function should take a single
   * nsIRequest* and returns PRBool -- PR_TRUE if it sets mContentType,
   * PR_FALSE otherwise
   */
  struct nsSnifferEntry {
    typedef PRBool (nsUnknownDecoder::*TypeSniffFunc)(nsIRequest* aRequest);
    
    const char* mBytes;
    PRUint32 mByteLen;
    
    // Exactly one of mMimeType and mContentTypeSniffer should be set non-null
    const char* mMimeType;
    TypeSniffFunc mContentTypeSniffer;
  };

#define SNIFFER_ENTRY(_bytes, _type) \
  { _bytes, sizeof(_bytes) - 1, _type, nsnull }

#define SNIFFER_ENTRY_WITH_FUNC(_bytes, _func) \
  { _bytes, sizeof(_bytes) - 1, nsnull, _func }

  static nsSnifferEntry sSnifferEntries[];
  static PRUint32 sSnifferEntryNum;
  
  char *mBuffer;
  PRUint32 mBufferLen;
  PRBool mRequireHTMLsuffix;

  nsCString mContentType;

};

#define NS_BINARYDETECTOR_CID                        \
{ /* a2027ec6-ba0d-4c72-805d-148233f5f33c */         \
    0xa2027ec6,                                      \
    0xba0d,                                          \
    0x4c72,                                          \
    {0x80, 0x5d, 0x14, 0x82, 0x33, 0xf5, 0xf3, 0x3c} \
}

/**
 * Class that detects whether a data stream is text or binary.  This reuses
 * most of nsUnknownDecoder except the actual content-type determination logic
 * -- our overridden DetermineContentType simply calls LastDitchSniff and sets
 * the type to APPLICATION_GUESS_FROM_EXT if the data is detected as binary.
 */
class nsBinaryDetector : public nsUnknownDecoder
{
protected:
  virtual void DetermineContentType(nsIRequest* aRequest);
};

#endif /* nsUnknownDecoder_h__ */

