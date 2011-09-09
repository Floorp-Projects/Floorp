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
 * The Original Code is the Mozilla BMP Decoder.
 *
 * The Initial Developer of the Original Code is
 * Christian Biesinger <cbiesinger@web.de>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bobby Holley <bobbyholley@gmail.com>
 *   Brian R. Bondy <netzen@gmail.com>
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


#ifndef _nsBMPDecoder_h
#define _nsBMPDecoder_h

#include "nsAutoPtr.h"
#include "imgIDecoderObserver.h"
#include "gfxColor.h"
#include "Decoder.h"
#include "BMPFileHeaders.h"

namespace mozilla {
namespace imagelib {

class RasterImage;

/**
 * Decoder for BMP-Files, as used by Windows and OS/2
 */
class nsBMPDecoder : public Decoder
{
public:

    nsBMPDecoder();
    ~nsBMPDecoder();

    // Specifies whether or not the BMP file will contain alpha data
    // If set to true and the BMP is 32BPP, the alpha data will be
    // retrieved from the 4th byte of image data per pixel 
    void SetUseAlphaData(PRBool useAlphaData);
    // Obtains the bits per pixel from the internal BIH header
    PRInt32 GetBitsPerPixel() const;
    // Obtains the width from the internal BIH header
    PRInt32 GetWidth() const;
    // Obtains the height from the internal BIH header
    PRInt32 GetHeight() const;
    // Obtains the internal output image buffer
    PRUint32* GetImageData();
    // Obtains the size of the compressed image resource
    PRInt32 GetCompressedImageSize() const;
    // Obtains whether or not a BMP file had alpha data in its 4th byte
    // for 32BPP bitmaps.  Only use after the bitmap has been processed.
    PRBool HasAlphaData() const;

    virtual void WriteInternal(const char* aBuffer, PRUint32 aCount);
    virtual void FinishInternal();

private:

    /** Calculates the red-, green- and blueshift in mBitFields using
     * the bitmasks from mBitFields */
    NS_METHOD CalcBitShift();

    PRUint32 mPos;

    BMPFILEHEADER mBFH;
    BMPINFOHEADER mBIH;
    char mRawBuf[36];

    PRUint32 mLOH; ///< Length of the header

    PRUint32 mNumColors; ///< The number of used colors, i.e. the number of entries in mColors
    colorTable *mColors;

    bitFields mBitFields;

    PRUint32 *mImageData; ///< Pointer to the image data for the frame
    PRUint8 *mRow;      ///< Holds one raw line of the image
    PRUint32 mRowBytes; ///< How many bytes of the row were already received
    PRInt32 mCurLine;   ///< Index of the line of the image that's currently being decoded
    PRInt32 mOldLine;   ///< Previous index of the line 
    PRInt32 mCurPos;    ///< Index in the current line of the image

    ERLEState mState;   ///< Maintains the current state of the RLE decoding
    PRUint32 mStateData;///< Decoding information that is needed depending on mState

    /** Set mBFH from the raw data in mRawBuf, converting from little-endian
     * data to native data as necessary */
    void ProcessFileHeader();
    /** Set mBIH from the raw data in mRawBuf, converting from little-endian
     * data to native data as necessary */
    void ProcessInfoHeader();

    // Stores whether the image data may store alpha data, or if
    // the alpha data is unspecified and filled with a padding byte of 0. 
    // When a 32BPP bitmap is stored in an ICO or CUR file, its 4th byte
    // is used for alpha transparency.  When it is stored in a BMP, its
    // 4th byte is reserved and is always 0.
    // Reference: 
    // http://en.wikipedia.org/wiki/ICO_(file_format)#cite_note-9
    // Bitmaps where the alpha bytes are all 0 should be fully visible.
    PRPackedBool mUseAlphaData;
    // Whether the 4th byte alpha data was found to be non zero and hence used.
    PRPackedBool mHaveAlphaData;
};

/** Sets the pixel data in aDecoded to the given values.
 * @param aDecoded pointer to pixel to be set, will be incremented to point to the next pixel.
 */
static inline void SetPixel(PRUint32*& aDecoded, PRUint8 aRed, PRUint8 aGreen, PRUint8 aBlue, PRUint8 aAlpha = 0xFF)
{
    *aDecoded++ = GFX_PACKED_PIXEL(aAlpha, aRed, aGreen, aBlue);
}

static inline void SetPixel(PRUint32*& aDecoded, PRUint8 idx, colorTable* aColors)
{
    SetPixel(aDecoded, aColors[idx].red, aColors[idx].green, aColors[idx].blue);
}

/** Sets two (or one if aCount = 1) pixels
 * @param aDecoded where the data is stored. Will be moved 4 resp 8 bytes
 * depending on whether one or two pixels are written.
 * @param aData The values for the two pixels
 * @param aCount Current count. Is decremented by one or two.
 */
inline void Set4BitPixel(PRUint32*& aDecoded, PRUint8 aData,
                         PRUint32& aCount, colorTable* aColors)
{
    PRUint8 idx = aData >> 4;
    SetPixel(aDecoded, idx, aColors);
    if (--aCount > 0) {
        idx = aData & 0xF;
        SetPixel(aDecoded, idx, aColors);
        --aCount;
    }
}

} // namespace imagelib
} // namespace mozilla


#endif

