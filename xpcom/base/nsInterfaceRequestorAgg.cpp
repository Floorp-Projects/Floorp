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

#include "nsInterfaceRequestorAgg.h"
#include "nsCOMPtr.h"

class nsInterfaceRequestorAgg : public nsIInterfaceRequestor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIINTERFACEREQUESTOR

  nsInterfaceRequestorAgg(nsIInterfaceRequestor *aFirst,
                          nsIInterfaceRequestor *aSecond)
    : mFirst(aFirst)
    , mSecond(aSecond) {}

  nsCOMPtr<nsIInterfaceRequestor> mFirst, mSecond;
};

// XXX This needs to support threadsafe refcounting until we fix bug 243591.
NS_IMPL_THREADSAFE_ISUPPORTS1(nsInterfaceRequestorAgg, nsIInterfaceRequestor)

NS_IMETHODIMP
nsInterfaceRequestorAgg::GetInterface(const nsIID &aIID, void **aResult)
{
  nsresult rv = NS_ERROR_NO_INTERFACE;
  if (mFirst)
    rv = mFirst->GetInterface(aIID, aResult);
  if (mSecond && NS_FAILED(rv))
    rv = mSecond->GetInterface(aIID, aResult);
  return rv;
}

nsresult
NS_NewInterfaceRequestorAggregation(nsIInterfaceRequestor *aFirst,
                                    nsIInterfaceRequestor *aSecond,
                                    nsIInterfaceRequestor **aResult)
{
  *aResult = new nsInterfaceRequestorAgg(aFirst, aSecond);
  if (!aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}
