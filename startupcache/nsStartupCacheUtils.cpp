/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
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
 * The Original Code is Startup Cache.
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation <http://www.mozilla.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Benedict Hsieh <bhsieh@mozilla.com>
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

#include "nsStartupCacheUtils.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIInputStream.h"
#include "nsIStorageStream.h"
#include "nsIStringStream.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

nsresult
NS_NewObjectInputStreamFromBuffer(char* buffer, int len, 
                                  nsIObjectInputStream** stream)
{
  nsCOMPtr<nsIStringInputStream> stringStream
    = do_CreateInstance("@mozilla.org/io/string-input-stream;1");
  if (!stringStream)
    return NS_ERROR_OUT_OF_MEMORY;
  nsCOMPtr<nsIObjectInputStream> objectInput 
    = do_CreateInstance("@mozilla.org/binaryinputstream;1");
  if (!objectInput)
    return NS_ERROR_OUT_OF_MEMORY;

  stringStream->AdoptData(buffer, len);
  objectInput->SetInputStream(stringStream);

  NS_ADDREF(*stream = objectInput);
  return NS_OK;
}

// This is questionable API name and design, but we can't
// retrieve the wrapped stream from the objectOutputStream later...
nsresult
NS_NewObjectOutputWrappedStorageStream(nsIObjectOutputStream **wrapperStream,
                                       nsIStorageStream** stream)
{
  nsCOMPtr<nsIStorageStream> storageStream;
  nsresult rv = NS_NewStorageStream(256, (PRUint32)-1, 
                                    getter_AddRefs(storageStream));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIObjectOutputStream> objectOutput
    = do_CreateInstance("@mozilla.org/binaryoutputstream;1");
  if (!objectOutput)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIOutputStream> outputStream
    = do_QueryInterface(storageStream);

  objectOutput->SetOutputStream(outputStream);
  NS_ADDREF(*wrapperStream = objectOutput);
  NS_ADDREF(*stream = storageStream);
  return NS_OK;
}

nsresult
NS_NewBufferFromStorageStream(nsIStorageStream *storageStream, 
                              char** buffer, int* len)
{
  nsresult rv;
  nsCOMPtr<nsIInputStream> inputStream;
  rv = storageStream->NewInputStream(0, getter_AddRefs(inputStream));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 avail, read;
  rv = inputStream->Available(&avail);
  NS_ENSURE_SUCCESS(rv, rv);

  char* temp = new char[avail];
  if (!temp)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = inputStream->Read(temp, avail, &read);
  if (NS_SUCCEEDED(rv) && avail != read)
    rv = NS_ERROR_UNEXPECTED;

  if (NS_FAILED(rv)) {
    delete temp;
    return rv;
  }

  *len = avail;
  *buffer = temp;
  return NS_OK;
}

