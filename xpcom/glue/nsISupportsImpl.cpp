/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupportsImpl.h"

nsresult NS_FASTCALL
NS_TableDrivenQI(void* aThis, REFNSIID aIID, void **aInstancePtr,
                 const QITableEntry* entries)
{
  do {
    if (aIID.Equals(*entries->iid)) {
      nsISupports* r =
        reinterpret_cast<nsISupports*>
                        (reinterpret_cast<char*>(aThis) + entries->offset);
      NS_ADDREF(r);
      *aInstancePtr = r;
      return NS_OK;
    }

    ++entries;
  } while (entries->iid);

  *aInstancePtr = nullptr;
  return NS_ERROR_NO_INTERFACE;
}
