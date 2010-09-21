/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Saari <saari@netscape.com>
 *   Bobby Holley <bobbyholley@gmail.com>
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

#ifndef _nsGIFDecoder2_h
#define _nsGIFDecoder2_h

#include "nsCOMPtr.h"
#include "Decoder.h"
#include "imgIDecoderObserver.h"

#include "GIF2.h"

namespace mozilla {
namespace imagelib {
class RasterImage;

//////////////////////////////////////////////////////////////////////
// nsGIFDecoder2 Definition

class nsGIFDecoder2 : public Decoder
{
public:

  nsGIFDecoder2();
  ~nsGIFDecoder2();

  virtual void WriteInternal(const char* aBuffer, PRUint32 aCount);
  virtual void FinishInternal();

private:
  /* These functions will be called when the decoder has a decoded row,
   * frame size information, etc. */

  void      BeginGIF();
  nsresult  BeginImageFrame(gfx_depth aDepth);
  void      EndImageFrame();
  void      FlushImageData();
  void      FlushImageData(PRUint32 fromRow, PRUint32 rows);

  nsresult  GifWrite(const PRUint8 * buf, PRUint32 numbytes);
  PRUint32  OutputRow();
  PRBool    DoLzw(const PRUint8 *q);

  inline int ClearCode() const { return 1 << mGIFStruct.datasize; }

  PRInt32 mCurrentRow;
  PRInt32 mLastFlushedRow;

  PRUint8 *mImageData;       // Pointer to image data in either Cairo or 8bit format
  PRUint32 *mColormap;       // Current colormap to be used in Cairo format
  PRUint32 mColormapSize;
  PRUint32 mOldColor;        // The old value of the transparent pixel

  // The frame number of the currently-decoding frame when we're in the middle
  // of decoding it, and -1 otherwise.
  PRInt32 mCurrentFrame;

  PRUint8 mCurrentPass;
  PRUint8 mLastFlushedPass;
  PRUint8 mColorMask;        // Apply this to the pixel to keep within colormap
  PRPackedBool mGIFOpen;
  PRPackedBool mSawTransparency;

  gif_struct mGIFStruct;
};

} // namespace imagelib
} // namespace mozilla

#endif
