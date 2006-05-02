/* nsJARDirectoryInputStream.h
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
 * The Original Code is Mozilla libjar code.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

#ifndef nsJARDIRECTORYINPUTSTREAM_h__
#define nsJARDIRECTORYINPUTSTREAM_h__

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsIInputStream.h"
#include "nsJAR.h"

/*-------------------------------------------------------------------------
 * Class nsJARDirectoryInputStream declaration. This class represents a
 * stream whose contents are an application/http-index-format listing for
 * a directory in a zip file.
 *------------------------------------------------------------------------*/
class nsJARDirectoryInputStream : public nsIInputStream
{
  public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM

    static nsresult Create(nsIZipReader* aZip,
                           const nsACString& aJarDirSpec,
                           const char* aDir,
                           nsIInputStream** result);
  private:
    nsJARDirectoryInputStream();
    virtual ~nsJARDirectoryInputStream();

    nsresult 
    Init(nsIZipReader* aZip, const nsACString& aJarDirSpec, const char* aDir);

    PRUint32 CopyDataToBuffer(char* &aBuffer, PRUint32 &aCount);

  protected:
    nsIZipReader*           mZip;        // the zipReader
    nsresult                mStatus;     // current status of the stream
    PRUint32                mDirNameLen; // length of dirname
    nsCAutoString           mBuffer;     // storage for generated text of stream
    PRUint32                mArrPos;     // current position within mArray
    PRUint32                mBufPos;     // current position within mBuffer
    nsCStringArray          mArray;      // array of names in (zip) directory
};

#endif /* nsJARDIRECTORYINPUTSTREAM_h__ */
