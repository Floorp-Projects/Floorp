/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Terry Hayes <thayes@netscape.com>
*/

/*
 * Dialog services for PIP.  THIS FILE SHOULD MOVE TO pippki.
 */
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDialogParamBlock.h"
#include "nsIComponentManager.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "jsapi.h"

#include "nsNSSDialogs.h"

/**
 * Common class that provides a standard dialog display function, 
 * and uses the hidden window if one can't be determined from
 * context
 */
class nsNSSDialogHelper
{
public:
  static nsresult openDialog(
                  nsIDOMWindowInternal *window,
                  const char *url,
                  PRBool modal,
                  nsIDialogParamBlock *params);
};

static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

nsresult
nsNSSDialogHelper::openDialog(
    nsIDOMWindowInternal *window,
    const char *url,
    PRBool modal,
    nsIDialogParamBlock *params)
{
  nsresult rv;
  nsCOMPtr<nsIDOMWindowInternal> hiddenWindow;

  if (!window) {
    NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = appShell->GetHiddenDOMWindow( getter_AddRefs( hiddenWindow ) );
    if ( NS_FAILED( rv ) ) return rv;

    window = hiddenWindow;
  }

  // Get JS context from parent window.
  nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface( window, &rv );
  if (!sgo) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptContext> context;
  sgo->GetContext( getter_AddRefs( context ) );
  if (!context) return NS_ERROR_FAILURE;

  JSContext *jsContext = (JSContext*)context->GetNativeContext();
  if (!jsContext) return NS_ERROR_FAILURE;

  void *stackPtr;
  jsval *argv = JS_PushArguments( jsContext,
                                  &stackPtr,
                                  "sss%ip",
                                  url,
                                  "_blank",
                                  "centerscreen,chrome,modal,titlebar",
                                  &NS_GET_IID(nsIDialogParamBlock),
                                  (nsISupports*)params
                                );
  if ( !argv ) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDOMWindowInternal> newWindow;
  rv = window->OpenDialog( jsContext, argv, 4, getter_AddRefs(newWindow) );
   
  JS_PopArguments( jsContext, stackPtr );

  return rv;
}

/* ==== */
static NS_DEFINE_CID(kDialogParamBlockCID, NS_DialogParamBlock_CID);

nsNSSDialogs::nsNSSDialogs()
{
  NS_INIT_ISUPPORTS();
}

nsNSSDialogs::~nsNSSDialogs()
{
}

NS_IMPL_ISUPPORTS1(nsNSSDialogs, nsINSSDialogs)

nsresult
nsNSSDialogs::SetPassword(nsIInterfaceRequestor *ctx,
                          const PRUnichar *tokenName, PRBool* _canceled)
{
  nsresult rv;

  *_canceled = PR_FALSE;

  nsCOMPtr<nsIDialogParamBlock> block = do_CreateInstance(kDialogParamBlockCID);

  if (!block) return NS_ERROR_FAILURE;

  rv = block->SetString(1, tokenName);
  rv = nsNSSDialogHelper::openDialog(nsnull, 
                                "chrome://pipnss/content/changepassword.xul",
                                PR_TRUE, block);
  if (NS_FAILED(rv)) return rv;

  PRInt32 status;

  rv = block->GetInt(1, &status);
  if (NS_FAILED(rv)) return rv;

  *_canceled = (status == 0)?PR_TRUE:PR_FALSE;

  return rv;
}
