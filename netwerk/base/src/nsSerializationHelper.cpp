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
 * The Original Code is mozilla.org networking code.
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

#include "nsSerializationHelper.h"

#include "plbase64.h"
#include "prmem.h"

#include "nsISerializable.h"
#include "nsIObjectOutputStream.h"
#include "nsIObjectInputStream.h"
#include "nsString.h"
#include "nsBase64Encoder.h"
#include "nsAutoPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsStringStream.h"

nsresult
NS_SerializeToString(nsISerializable* obj, nsCSubstring& str)
{
  nsRefPtr<nsBase64Encoder> stream(new nsBase64Encoder());
  if (!stream)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIObjectOutputStream> objstream =
      do_CreateInstance("@mozilla.org/binaryoutputstream;1");
  if (!objstream)
    return NS_ERROR_OUT_OF_MEMORY;

  objstream->SetOutputStream(stream);
  nsresult rv =
      objstream->WriteCompoundObject(obj, NS_GET_IID(nsISupports), PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);
  return stream->Finish(str);
}

nsresult
NS_DeserializeObject(const nsCSubstring& str, nsISupports** obj)
{
  // Base64 maps 3 binary bytes -> 4 ASCII bytes.  If the original byte array
  // does not have length 0 mod 3, the input is padded with zeros and the
  // output is padded with a corresponding number of trailing '=' (which are
  // then sometimes dropped).  To compute the correct length of the original
  // byte array, we have to subtract the number of trailing '=' and then
  // multiply by 3 and then divide by 4 (making sure this is an integer
  // division).

  PRUint32 size = str.Length();
  if (size > 0 && str[size-1] == '=') {
    if (size > 1 && str[size-2] == '=') {
      size -= 2;
    } else {
      size -= 1;
    }
  }
  size = (size * 3) / 4;
  char* buf = PL_Base64Decode(str.BeginReading(), str.Length(), nsnull);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY;
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream),
                                         Substring(buf, buf + size));
  PR_Free(buf);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObjectInputStream> objstream =
      do_CreateInstance("@mozilla.org/binaryinputstream;1");
  if (!objstream)
    return NS_ERROR_OUT_OF_MEMORY;

  objstream->SetInputStream(stream);
  return objstream->ReadObject(PR_TRUE, obj);
}

NS_IMPL_ISUPPORTS1(nsSerializationHelper, nsISerializationHelper)

NS_IMETHODIMP
nsSerializationHelper::SerializeToString(nsISerializable *serializable,
                                         nsACString & _retval NS_OUTPARAM)
{
  return NS_SerializeToString(serializable, _retval);
}

NS_IMETHODIMP
nsSerializationHelper::DeserializeObject(const nsACString & input,
                                         nsISupports **_retval NS_OUTPARAM)
{
  return NS_DeserializeObject(input, _retval);
}
