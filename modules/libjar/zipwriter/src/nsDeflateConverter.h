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
 * The Original Code is Zip Writer Component.
 *
 * The Initial Developer of the Original Code is
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *      Lan Qiang <jameslan@gmail.com>
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
 * ***** END LICENSE BLOCK *****
 */

#ifndef _nsDeflateConverter_h_
#define _nsDeflateConverter_h_

#include "nsIStreamConverter.h"
#include "nsCOMPtr.h"
#include "nsIPipe.h"
#include "zlib.h"

#define DEFLATECONVERTER_CLASSNAME "Deflate converter"
#define DEFLATECONVERTER_CID { 0x461cd5dd, 0x73c6, 0x47a4, \
           { 0x8c, 0xc3, 0x60, 0x3b, 0x37, 0xd8, 0x4a, 0x61 } }

#define ZIP_BUFLEN (4 * 1024 - 1)

class nsDeflateConverter : public nsIStreamConverter
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIREQUESTOBSERVER
    NS_DECL_NSISTREAMLISTENER
    NS_DECL_NSISTREAMCONVERTER

    nsDeflateConverter()
    {
        // 6 is Z_DEFAULT_COMPRESSION but we need the actual value
        mLevel = 6;
    }

    nsDeflateConverter(PRInt32 level)
    {
        mLevel = level;
    }

private:

    ~nsDeflateConverter()
    {
    }
    
    enum WrapMode {
        WRAP_ZLIB,
        WRAP_GZIP,
        WRAP_NONE
    };

    WrapMode mWrapMode;
    PRUint32 mOffset;
    PRInt32 mLevel;
    nsCOMPtr<nsIStreamListener> mListener;
    nsCOMPtr<nsISupports> mContext;
    z_stream mZstream;
    unsigned char mWriteBuffer[ZIP_BUFLEN];

    nsresult Init();
    nsresult PushAvailableData(nsIRequest *aRequest, nsISupports *aContext);
};

#endif
