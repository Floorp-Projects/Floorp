/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsSerializationHelper.h"

#include "mozilla/Base64.h"
#include "nsISerializable.h"
#include "nsIObjectOutputStream.h"
#include "nsIObjectInputStream.h"
#include "nsString.h"
#include "nsBase64Encoder.h"
#include "nsComponentManagerUtils.h"
#include "nsStringStream.h"

using namespace mozilla;

nsresult NS_SerializeToString(nsISerializable* obj, nsACString& str) {
  RefPtr<nsBase64Encoder> stream(new nsBase64Encoder());
  if (!stream) return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIObjectOutputStream> objstream = NS_NewObjectOutputStream(stream);
  nsresult rv =
      objstream->WriteCompoundObject(obj, NS_GET_IID(nsISupports), true);
  NS_ENSURE_SUCCESS(rv, rv);
  return stream->Finish(str);
}

nsresult NS_DeserializeObject(const nsACString& str, nsISupports** obj) {
  nsCString decodedData;
  nsresult rv = Base64Decode(str, decodedData);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIInputStream> stream;
  rv = NS_NewCStringInputStream(getter_AddRefs(stream), std::move(decodedData));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObjectInputStream> objstream = NS_NewObjectInputStream(stream);
  return objstream->ReadObject(true, obj);
}

NS_IMPL_ISUPPORTS(nsSerializationHelper, nsISerializationHelper)

NS_IMETHODIMP
nsSerializationHelper::SerializeToString(nsISerializable* serializable,
                                         nsACString& _retval) {
  return NS_SerializeToString(serializable, _retval);
}

NS_IMETHODIMP
nsSerializationHelper::DeserializeObject(const nsACString& input,
                                         nsISupports** _retval) {
  return NS_DeserializeObject(input, _retval);
}
