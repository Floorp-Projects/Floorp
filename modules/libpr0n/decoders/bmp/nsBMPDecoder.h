/* vim:set tw=80 expandtab softtabstop=4 ts=4 sw=4: */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/LGPL 2.1/GPL 2.0
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
 * The Original Code is the Mozilla BMP Decoder.
 * 
 * The Initial Developer of the Original Code is Christian Biesinger
 * <cbiesinger@web.de>.  Portions created by Christian Biesinger are
 * Copyright (C) 2001 Christian Biesinger.  All
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


#ifndef _nsBMPDecoder_h
#define _nsBMPDecoder_h

#include "nsCOMPtr.h"
#include "imgIDecoder.h"
#include "imgIContainer.h"
#include "imgIDecoderObserver.h"
#include "gfxIImageFrame.h"

#define NS_BMPDECODER_CID \
{ /* {78c61626-4d1f-4843-9364-4652d98ff6e1} */ \
  0x78c61626, \
  0x4d1f, \
  0x4843, \
  { 0x93, 0x64, 0x46, 0x52, 0xd9, 0x8f, 0xf6, 0xe1 } \
}

struct BMPFILEHEADER {
    char signature[2]; // String "BM"
    PRUint32 filesize;
    PRInt32 reserved; // Zero
    PRUint32 dataoffset; // Offset to raster data

    PRUint32 bihsize;
};

struct BMPINFOHEADER {
    PRUint32 width;
    PRUint32 height;
    PRUint16 planes; // =1
    PRUint16 bpp; // Bits per pixel.
    // The rest of the header is not available in OS/2 BMP Files
    PRUint32 compression; // 0=no compression 1=8bit RLE 2=4bit RLE
    PRUint32 image_size; // (compressed) image size. Can be 0 if compression==0
    PRUint32 xppm; // Pixels per meter, horizontal
    PRUint32 yppm; // Pixels per meter, vertical
    PRUint32 colors; // Used Colors
    PRUint32 important_colors; // Number of important colors. 0=all
};

struct colorTable {
    PRUint8 red;
    PRUint8 green;
    PRUint8 blue;
};

struct bitFields {
    PRUint32 red;
    PRUint32 green;
    PRUint32 blue;
};

#define DOCOPY(dest, src) nsCRT::memcpy(dest, src, sizeof(dest))

#if defined WORDS_BIGENDIAN || defined IS_BIG_ENDIAN
#define LITTLE_TO_NATIVE16(x) ((((x) & 0xFF) << 8) | ((x) >> 8))
#define LITTLE_TO_NATIVE32(x) ((((x) & 0xFF) << 24) | \
                               ((((x) >> 8) & 0xFF) << 16) | \
                               ((((x) >> 16) & 0xFF) << 8) | \
                               ((x) >> 24))
#else
#define LITTLE_TO_NATIVE16(x) x
#define LITTLE_TO_NATIVE32(x) x
#endif

#define BI_RLE8 1
#define BI_RLE4 2
#define BI_BITFIELDS 3

#if defined(XP_PC) || defined(XP_BEOS) || defined(MOZ_WIDGET_PHOTON)
#define GFXFORMAT gfxIFormats::BGR
#else
#define USE_RGB
#define GFXFORMAT gfxIFormats::RGB
#endif

class nsBMPDecoder : public imgIDecoder
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_IMGIDECODER
    
    nsBMPDecoder();
    virtual ~nsBMPDecoder();

private:
    /** Processes the data.
     * @param aBuffer Data to process.
     * @oaram count Number of bytes in mBuffer */
    NS_METHOD ProcessData(const char* aBuffer, PRUint32 aCount);

    /** Sets the pixel data in aDecoded to the given values.
     * The variable passed in as aDecoded will be moved on 3 bytes! */
    inline nsresult SetPixel(PRUint8*& aDecoded, PRUint8 aIdx);
    inline nsresult SetPixel(PRUint8*& aDecoded, PRUint8 aRed, PRUint8 aGreen, PRUint8 aBlue);

    /** Sets one or two pixels; it is ensured that aPos is <= mBIH.width
     * @param aDecoded where the data is stored. Will be moved 3 or 6 bytes,
     * depending on whether one or two pixels are written.
     * @param aData The values for the two pixels
     * @param aPos Current position. Is incremented by one or two. */
    inline nsresult Set4BitPixel(PRUint8*& aDecoded, PRUint8 aData, PRUint32& aPos);

    /** Sets the image data at specified position. mCurLine is used
     * to get the row
     * @param aData The data */
    inline nsresult SetData(PRUint8* aData);

    nsCOMPtr<imgIDecoderObserver> mObserver;

    nsCOMPtr<imgIContainer> mImage;
    nsCOMPtr<gfxIImageFrame> mFrame;

    PRUint32 mPos;

    BMPFILEHEADER mBFH;
    BMPINFOHEADER mBIH;
    char mRawBuf[36];

    PRUint32 mLOH; // Length of the header

    PRUint32 mNumColors;
    colorTable *mColors;

    PRUint8 *mRow; // Holds one raw line of the image
    PRUint32 mRowBytes; // How many bytes of the row were already received
    PRUint32 mCurLine;

    void ProcessFileHeader();
    void ProcessInfoHeader();
};


#endif
