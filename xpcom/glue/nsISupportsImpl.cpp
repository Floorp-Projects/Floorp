/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupportsImpl.h"

nsresult NS_FASTCALL
NS_TableDrivenQI(void* aThis, const QITableEntry* entries,
                 REFNSIID aIID, void **aInstancePtr)
{
  while (entries->iid) {
    if (aIID.Equals(*entries->iid)) {
      nsISupports* r =
        reinterpret_cast<nsISupports*>
                        (reinterpret_cast<char*>(aThis) + entries->offset);
      NS_ADDREF(r);
      *aInstancePtr = r;
      return NS_OK;
    }

    ++entries;
  }

  *aInstancePtr = nsnull;
  return NS_ERROR_NO_INTERFACE;
}
