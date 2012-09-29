/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include "nsIStorageStream.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCOMPtr.h"

int main()
{
  char kData[4096];
  memset(kData, 0, sizeof(kData));

  nsresult rv;
  nsCOMPtr<nsIStorageStream> stor;

  rv = NS_NewStorageStream(4096, UINT32_MAX, getter_AddRefs(stor));
  if (NS_FAILED(rv))
    return -1;

  nsCOMPtr<nsIOutputStream> out;
  rv = stor->GetOutputStream(0, getter_AddRefs(out));
  if (NS_FAILED(rv))
    return -1;

  uint32_t n;

  rv = out->Write(kData, sizeof(kData), &n);
  if (NS_FAILED(rv))
    return -1;

  rv = out->Write(kData, sizeof(kData), &n);
  if (NS_FAILED(rv))
    return -1;

  rv = out->Close();
  if (NS_FAILED(rv))
    return -1;

  out = nullptr;
  
  nsCOMPtr<nsIInputStream> in;
  rv = stor->NewInputStream(0, getter_AddRefs(in));
  if (NS_FAILED(rv))
    return -1;

  char buf[4096];

  // consume contents of input stream
  do {
    rv = in->Read(buf, sizeof(buf), &n);
    if (NS_FAILED(rv))
      return -1;
  } while (n != 0);

  rv = in->Close();
  if (NS_FAILED(rv))
    return -1;
  in = nullptr;

  // now, write 3 more full 4k segments + 11 bytes, starting at 8192
  // total written equals 20491 bytes

  rv = stor->GetOutputStream(8192, getter_AddRefs(out));
  if (NS_FAILED(rv))
    return -1;

  rv = out->Write(kData, sizeof(kData), &n);
  if (NS_FAILED(rv))
    return -1;

  rv = out->Write(kData, sizeof(kData), &n);
  if (NS_FAILED(rv))
    return -1;

  rv = out->Write(kData, sizeof(kData), &n);
  if (NS_FAILED(rv))
    return -1;

  rv = out->Write(kData, 11, &n);
  if (NS_FAILED(rv))
    return -1;

  rv = out->Close();
  if (NS_FAILED(rv))
    return -1;

  out = nullptr;

  // now, read all
  rv = stor->NewInputStream(0, getter_AddRefs(in));
  if (NS_FAILED(rv))
    return -1;

  // consume contents of input stream
  do {
    rv = in->Read(buf, sizeof(buf), &n);
    if (NS_FAILED(rv))
      return -1;
  } while (n != 0);

  rv = in->Close();
  if (NS_FAILED(rv))
    return -1;
  in = nullptr;

  return 0;
}
