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
 *   Stuart Parmenter <pavlov@netscape.com>
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

#ifndef nsJPEGDecoder_h__
#define nsJPEGDecoder_h__

#include "RasterImage.h"
/* On Windows systems, RasterImage.h brings in 'windows.h', which defines INT32.
 * But the jpeg decoder has its own definition of INT32. To avoid build issues,
 * we need to undefine the version from 'windows.h'. */
#undef INT32

#include "Decoder.h"

#include "nsAutoPtr.h"

#include "imgIDecoderObserver.h"
#include "nsIInputStream.h"
#include "nsIPipe.h"
#include "qcms.h"

extern "C" {
#include "jpeglib.h"
}

#include <setjmp.h>

namespace mozilla {
namespace imagelib {

typedef struct {
    struct jpeg_error_mgr pub;  /* "public" fields for IJG library*/
    jmp_buf setjmp_buffer;      /* For handling catastropic errors */
} decoder_error_mgr;

typedef enum {
    JPEG_HEADER,                          /* Reading JFIF headers */
    JPEG_START_DECOMPRESS,
    JPEG_DECOMPRESS_PROGRESSIVE,          /* Output progressive pixels */
    JPEG_DECOMPRESS_SEQUENTIAL,           /* Output sequential pixels */
    JPEG_DONE,
    JPEG_SINK_NON_JPEG_TRAILER,          /* Some image files have a */
                                         /* non-JPEG trailer */
    JPEG_ERROR    
} jstate;

class RasterImage;

class nsJPEGDecoder : public Decoder
{
public:
  nsJPEGDecoder();
  virtual ~nsJPEGDecoder();

  virtual void InitInternal();
  virtual void WriteInternal(const char* aBuffer, PRUint32 aCount);
  virtual void FinishInternal();

  void NotifyDone();

protected:
  void OutputScanlines(PRBool* suspend);

public:
  PRUint8 *mImageData;

  struct jpeg_decompress_struct mInfo;
  struct jpeg_source_mgr mSourceMgr;
  decoder_error_mgr mErr;
  jstate mState;

  PRUint32 mBytesToSkip;

  const JOCTET *mSegment;   // The current segment we are decoding from
  PRUint32 mSegmentLen;     // amount of data in mSegment

  JOCTET *mBackBuffer;
  PRUint32 mBackBufferLen; // Offset of end of active backtrack data
  PRUint32 mBackBufferSize; // size in bytes what mBackBuffer was created with
  PRUint32 mBackBufferUnreadLen; // amount of data currently in mBackBuffer

  JOCTET  *mProfile;
  PRUint32 mProfileLength;

  qcms_profile *mInProfile;
  qcms_transform *mTransform;

  PRPackedBool mReading;
};

} // namespace imagelib
} // namespace mozilla

#endif // nsJPEGDecoder_h__
