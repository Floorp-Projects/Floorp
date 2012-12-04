/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
      objstream->WriteCompoundObject(obj, NS_GET_IID(nsISupports), true);
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

  uint32_t size = str.Length();
  if (size > 0 && str[size-1] == '=') {
    if (size > 1 && str[size-2] == '=') {
      size -= 2;
    } else {
      size -= 1;
    }
  }
  size = (size * 3) / 4;
  char* buf = PL_Base64Decode(str.BeginReading(), str.Length(), nullptr);
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY;
  nsCOMPtr<nsIInputStream> stream;
  nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream),
                                         Substring(buf, size));
  PR_Free(buf);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObjectInputStream> objstream =
      do_CreateInstance("@mozilla.org/binaryinputstream;1");
  if (!objstream)
    return NS_ERROR_OUT_OF_MEMORY;

  objstream->SetInputStream(stream);
  return objstream->ReadObject(true, obj);
}

NS_IMPL_ISUPPORTS1(nsSerializationHelper, nsISerializationHelper)

NS_IMETHODIMP
nsSerializationHelper::SerializeToString(nsISerializable *serializable,
                                         nsACString & _retval)
{
  return NS_SerializeToString(serializable, _retval);
}

NS_IMETHODIMP
nsSerializationHelper::DeserializeObject(const nsACString & input,
                                         nsISupports **_retval)
{
  return NS_DeserializeObject(input, _retval);
}
