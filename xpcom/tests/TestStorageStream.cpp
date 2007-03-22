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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
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

  rv = NS_NewStorageStream(4096, PR_UINT32_MAX, getter_AddRefs(stor));
  if (NS_FAILED(rv))
    return -1;

  nsCOMPtr<nsIOutputStream> out;
  rv = stor->GetOutputStream(0, getter_AddRefs(out));
  if (NS_FAILED(rv))
    return -1;

  PRUint32 n;

  rv = out->Write(kData, sizeof(kData), &n);
  if (NS_FAILED(rv))
    return -1;

  rv = out->Write(kData, sizeof(kData), &n);
  if (NS_FAILED(rv))
    return -1;

  rv = out->Close();
  if (NS_FAILED(rv))
    return -1;

  out = nsnull;
  
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
  in = nsnull;

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

  out = nsnull;

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
  in = nsnull;

  return 0;
}
