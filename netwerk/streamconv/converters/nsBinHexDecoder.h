/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A decoder for Mac Bin Hex 4.0.

// This decoder is currently only intended to be used on NON-Mac platforms. It isn't hooked up to
// code which would actually save the file to disk. As a result, we can't leverage the resource fork.
// This makes this decoder most unhelpful for the Mac. Our assumption is that if you save a bin hex file
// on the mac and try to open it, stuffit or some other tool is already going to be on the Mac which knows how
// to handle bin hex. On windows and unix, that's not the case. We need client code to strip out the data fork.
// So this decoder currently just strips out the data fork.

// Note: it's possible that we can eventually turn this decoder into both a decoder and into a file stream (much
// like the apple double decoder) so on the Mac, if we are saving to disk, we can invoke the decoder as a file stream
// and it will process the resource fork and do the right magic.

#ifndef nsBinHexDecoder_h__
#define nsBinHexDecoder_h__

#include "nsIStreamConverter.h"
#include "nsIChannel.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#define NS_BINHEXDECODER_CID                         \
{ /* 301DEA42-6850-4cda-8945-81F7DBC2186B */         \
  0x301dea42, 0x6850, 0x4cda,                        \
  { 0x89, 0x45, 0x81, 0xf7, 0xdb, 0xc2, 0x18, 0x6b } \
}

typedef struct _binhex_header
{
  PRUint32 type, creator;
  PRUint16 flags;
  PRInt32 dlen, rlen;
} binhex_header;

typedef union
{
  unsigned char c[4];
  PRUint32      val;
} longbuf;

#define BINHEX_STATE_START    0
#define BINHEX_STATE_FNAME    1
#define BINHEX_STATE_HEADER   2
#define BINHEX_STATE_HCRC     3
#define BINHEX_STATE_DFORK    4
#define BINHEX_STATE_DCRC     5
#define BINHEX_STATE_RFORK    6
#define BINHEX_STATE_RCRC     7
#define BINHEX_STATE_FINISH   8
#define BINHEX_STATE_DONE     9
/* #define BINHEX_STATE_ERROR  10 */

class nsBinHexDecoder : public nsIStreamConverter
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

  nsBinHexDecoder();

protected:
  virtual ~nsBinHexDecoder();

  PRInt16  GetNextChar(PRUint32 numBytesInBuffer);
  nsresult ProcessNextChunk(nsIRequest * aRequest, nsISupports * aContext, PRUint32 numBytesInBuffer);
  nsresult ProcessNextState(nsIRequest * aRequest, nsISupports * aContext);
  nsresult DetectContentType(nsIRequest * aRequest, const nsAFlatCString &aFilename);

protected:
  nsCOMPtr<nsIStreamListener> mNextListener;

  // the input and output streams form a pipe...they need to be passed around together..
  nsCOMPtr<nsIOutputStream>     mOutputStream;     // output stream
  nsCOMPtr<nsIInputStream>      mInputStream;

  PRInt16   mState;      /* current state */
  PRUint16  mCRC;        /* cumulative CRC */
  PRUint16  mFileCRC;    /* CRC value from file */
  longbuf   mOctetBuf;   /* buffer for decoded 6-bit values     */
  PRInt16   mOctetin;    /* current input position in octetbuf */
  PRInt16   mDonePos;    /* ending position in octetbuf */
  PRInt16   mInCRC;      /* flag set when reading a CRC */

  // Bin Hex Header Information
  binhex_header mHeader;
  nsCString mName;       /* fsspec for the output file */

  // unfortunately we are going to need 2 8K buffers here. One for the data we are currently digesting. Another
  // for the outgoing decoded data. I tried getting them to share a buffer but things didn't work out so nicely.
  char * mDataBuffer; // temporary holding pen for the incoming data.
  char * mOutgoingBuffer; // temporary holding pen for the incoming data.
  PRUint32 mPosInDataBuffer;

  unsigned char mRlebuf;  /* buffer for last run length encoding value */

  PRUint32 mCount;        /* generic counter */
  PRInt16 mMarker;        /* flag indicating maker */

  PRInt32 mPosInbuff;     /* the index of the inbuff.  */
  PRInt32 mPosOutputBuff; /* the position of the out buff.    */
};

#endif /* nsBinHexDecoder_h__ */
