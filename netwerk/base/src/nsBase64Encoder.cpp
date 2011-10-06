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
 * The Original Code is a base64 encoder stream.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christian Biesinger <cbiesinger@web.de> (Initial author)
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

#include "nsBase64Encoder.h"

#include "plbase64.h"
#include "prmem.h"

NS_IMPL_ISUPPORTS1(nsBase64Encoder, nsIOutputStream)

NS_IMETHODIMP
nsBase64Encoder::Close()
{
  return NS_OK;
}

NS_IMETHODIMP
nsBase64Encoder::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP
nsBase64Encoder::Write(const char* aBuf, PRUint32 aCount, PRUint32* _retval)
{
  mData.Append(aBuf, aCount);
  *_retval = aCount;
  return NS_OK;
}

NS_IMETHODIMP
nsBase64Encoder::WriteFrom(nsIInputStream* aStream, PRUint32 aCount,
                           PRUint32* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBase64Encoder::WriteSegments(nsReadSegmentFun aReader,
                               void* aClosure,
                               PRUint32 aCount,
                               PRUint32* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsBase64Encoder::IsNonBlocking(bool* aNonBlocking)
{
  *aNonBlocking = PR_FALSE;
  return NS_OK;
}

nsresult
nsBase64Encoder::Finish(nsCSubstring& result)
{
  char* b64 = PL_Base64Encode(mData.get(), mData.Length(), nsnull);
  if (!b64)
    return NS_ERROR_OUT_OF_MEMORY;

  result.Assign(b64);
  PR_Free(b64);
  // Free unneeded memory and allow reusing the object
  mData.Truncate();
  return NS_OK;
}
