/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
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
 * The Original Code is the Mozilla XBM Decoder.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Aaron Kaluszka <ask@swva.net>
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


#ifndef _nsXBMDecoder_h
#define _nsXBMDecoder_h

#include "nsCOMPtr.h"
#include "imgIDecoder.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"

#define NS_XBMDECODER_CID \
{ /* {dbfd145d-3298-4f3c-902f-2c5e1a1494ce} */ \
  0xdbfd145d, \
  0x3298, \
  0x4f3c, \
  { 0x90, 0x2f, 0x2c, 0x5e, 0x1a, 0x14, 0x94, 0xce } \
}

class nsXBMDecoder : public imgIDecoder
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IMGIDECODER
    
    nsXBMDecoder();
    virtual ~nsXBMDecoder();

    nsresult ProcessData(const char* aData, PRUint32 aCount);
private:
    static NS_METHOD ReadSegCb(nsIInputStream* aIn, void* aClosure,
                               const char* aFromRawSegment, PRUint32 aToOffset,
                               PRUint32 aCount, PRUint32 *aWriteCount);

    nsCOMPtr<imgIDecoderObserver> mObserver;

    nsCOMPtr<imgIContainer> mImage;
    nsCOMPtr<gfxIImageFrame> mFrame;

    PRInt32 mCurRow;
    PRInt32 mCurCol;

    char* mBuf; // Holds the received data
    char* mPos;
    PRUint32 mBufSize; // number of bytes in mBuf

    PRInt32 mWidth;
    PRInt32 mHeight;
    PRBool mIsX10; // X10 flavor XBM?

    PRUint8* mRow; // Hold the decoded row
    PRUint8* mAlphaRow; // alpha data for the row

    enum {
        RECV_HEADER,
        RECV_SEEK,
        RECV_DATA,
        RECV_DONE
    } mState;
};


#endif
