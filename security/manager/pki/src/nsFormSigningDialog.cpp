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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsFormSigningDialog.h"
#include "nsNSSDialogHelper.h"
#include "nsCOMPtr.h"
#include "nsIDialogParamBlock.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsLiteralString.h"
#include "nsXPIDLString.h"

nsFormSigningDialog::nsFormSigningDialog()
{
}

nsFormSigningDialog::~nsFormSigningDialog()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsFormSigningDialog, nsIFormSigningDialog)

NS_IMETHODIMP
nsFormSigningDialog::ConfirmSignText(nsIInterfaceRequestor *aContext, 
                                     const nsAString &aHost,
                                     const nsAString &aSignText,
                                     const PRUnichar **aCertNickList,
                                     const PRUnichar **aCertDetailsList,
                                     PRUint32 aCount, PRInt32 *aSelectedIndex,
                                     nsAString &aPassword, bool *aCanceled) 
{
  *aCanceled = PR_TRUE;

  // Get the parent window for the dialog
  nsCOMPtr<nsIDOMWindow> parent = do_GetInterface(aContext);

  nsresult rv;
  nsCOMPtr<nsIDialogParamBlock> block =
    do_CreateInstance(NS_DIALOGPARAMBLOCK_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  block->SetNumberStrings(3 + aCount * 2);

  rv = block->SetString(0, PromiseFlatString(aHost).get());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = block->SetString(1, PromiseFlatString(aSignText).get());
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 i;
  for (i = 0; i < aCount; ++i) {
    rv = block->SetString(2 + 2 * i, aCertNickList[i]);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = block->SetString(2 + (2 * i + 1), aCertDetailsList[i]);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = block->SetInt(0, aCount);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsNSSDialogHelper::openDialog(parent,
                                     "chrome://pippki/content/formsigning.xul",
                                     block);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 status;
  rv = block->GetInt(0, &status);
  NS_ENSURE_SUCCESS(rv, rv);

  if (status == 0) {
    *aCanceled = PR_TRUE;
  }
  else {
    *aCanceled = PR_FALSE;

    rv = block->GetInt(1, aSelectedIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLString pw;
    rv = block->GetString(0, getter_Copies(pw));
    NS_ENSURE_SUCCESS(rv, rv);

    aPassword = pw;
  }

  return NS_OK;
}
