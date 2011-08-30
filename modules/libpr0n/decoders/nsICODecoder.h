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
 * The Original Code is the Mozilla ICO Decoder.
 *
 * The Initial Developer of the Original Code is
 * Netscape.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Hyatt <hyatt@netscape.com> (Original Author)
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


#ifndef _nsICODecoder_h
#define _nsICODecoder_h

#include "nsAutoPtr.h"
#include "Decoder.h"
#include "imgIDecoderObserver.h"
#include "nsBMPDecoder.h"
#include "nsPNGDecoder.h"
#include "ICOFileHeaders.h"

namespace mozilla {
namespace imagelib {

class RasterImage;

class nsICODecoder : public Decoder
{
public:

  nsICODecoder();
  virtual ~nsICODecoder();

  virtual void WriteInternal(const char* aBuffer, PRUint32 aCount);
  virtual void FinishInternal();

private:
  // Processes a single dir entry of the icon resource
  void ProcessDirEntry(IconDirEntry& aTarget);
  // Sets the hotspot property of if we have a cursor
  void SetHotSpotIfCursor();
  // Creates a bitmap file header buffer, returns PR_TRUE if successful
  PRBool FillBitmapFileHeaderBuffer(PRInt8 *bfh);
  // Fixes the height of a BMP information header field
  void FillBitmapInformationBufferHeight(PRInt8 *bih);
  // Extract bitmap info header size count from BMP information header
  PRInt32 ExtractBIHSizeFromBitmap(PRInt8 *bih);
  // Extract bit count from BMP information header
  PRInt32 ExtractBPPFromBitmap(PRInt8 *bih);
  // Calculates the row size in bytes for the AND mask table
  PRUint32 CalcAlphaRowSize();
  // Obtains the number of colors from the BPP, mBPP must be filled in
  PRUint16 GetNumColors();

  PRUint16 mBPP; // Stores the images BPP
  PRUint32 mPos; // Keeps track of the position we have decoded up until
  PRUint16 mNumIcons; // Stores the number of icons in the ICO file
  PRUint16 mCurrIcon; // Stores the current dir entry index we are processing
  PRUint32 mImageOffset; // Stores the offset of the image data we want
  PRUint8 *mRow;      // Holds one raw line of the image
  PRInt32 mCurLine;   // Line index of the image that's currently being decoded
  PRUint32 mRowBytes; // How many bytes of the row were already received
  PRInt32 mOldLine;   // Previous index of the line 
  nsAutoPtr<Decoder> mContainedDecoder; // Contains either a BMP or PNG resource

  char mDirEntryArray[ICODIRENTRYSIZE]; // Holds the current dir entry buffer
  IconDirEntry mDirEntry; // Holds a decoded dir entry
  // Holds the potential bytes that can be a PNG signature
  char mSignature[PNGSIGNATURESIZE]; 
  // Holds the potential bytes for a bitmap information header
  char mBIHraw[40];
  // Stores whether or not the icon file we are processing has type 1 (icon)
  PRPackedBool mIsCursor;
  // Stores whether or not the contained resource is a PNG
  PRPackedBool mIsPNG;
};

} // namespace imagelib
} // namespace mozilla

#endif
