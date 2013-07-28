/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

NS_IMPL_ISUPPORTS1(nsFormSigningDialog, nsIFormSigningDialog)

NS_IMETHODIMP
nsFormSigningDialog::ConfirmSignText(nsIInterfaceRequestor *aContext, 
                                     const nsAString &aHost,
                                     const nsAString &aSignText,
                                     const PRUnichar **aCertNickList,
                                     const PRUnichar **aCertDetailsList,
                                     uint32_t aCount, int32_t *aSelectedIndex,
                                     nsAString &aPassword, bool *aCanceled) 
{
  *aCanceled = true;

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

  uint32_t i;
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

  int32_t status;
  rv = block->GetInt(0, &status);
  NS_ENSURE_SUCCESS(rv, rv);

  if (status == 0) {
    *aCanceled = true;
  }
  else {
    *aCanceled = false;

    rv = block->GetInt(1, aSelectedIndex);
    NS_ENSURE_SUCCESS(rv, rv);

    nsXPIDLString pw;
    rv = block->GetString(0, getter_Copies(pw));
    NS_ENSURE_SUCCESS(rv, rv);

    aPassword = pw;
  }

  return NS_OK;
}
