/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

// Local Includes
#include "nsNetSupportDialog.h"

// Helper Classes
#include "nsAppShellCIDs.h"
#include "nsCOMPtr.h"

//Interfaces Needed
#include "nsIAppShellService.h"
#include "nsICommonDialogs.h"
#include "nsIDOMWindowInternal.h"
#include "nsIServiceManager.h"
#include "nsIXULWindow.h"
#include "nsIInterfaceRequestor.h"

/* Define Class IDs */
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

PRBool GetNSIPrompt( nsCOMPtr<nsIPrompt> & outPrompt )
{
   nsCOMPtr<nsIAppShellService> appShellService(do_GetService(kAppShellServiceCID));
   if(!appShellService)
      return PR_FALSE;

   nsCOMPtr<nsIXULWindow> xulWindow;
   appShellService->GetHiddenWindow(getter_AddRefs(xulWindow));
   outPrompt = do_GetInterface(xulWindow);
   if(outPrompt)
  	   return PR_TRUE;
   return PR_FALSE;
} 

nsNetSupportDialog::nsNetSupportDialog()
{
	NS_INIT_REFCNT();
}

nsNetSupportDialog::~nsNetSupportDialog()
{
}



NS_IMETHODIMP nsNetSupportDialog::Alert(const PRUnichar *dialogTitle, const PRUnichar *text)
{

	 nsresult rv = NS_ERROR_FAILURE;
	 nsCOMPtr< nsIPrompt> dialogService;
     if( GetNSIPrompt( dialogService ) )
        rv = dialogService->Alert(dialogTitle, text);

	 return rv;
}

NS_IMETHODIMP	nsNetSupportDialog::AlertCheck(const PRUnichar *dialogTitle, 
                                               const PRUnichar *text, 
                                               const PRUnichar *checkMsg, 
                                               PRBool *checkValue)
{

	nsresult rv = NS_ERROR_FAILURE;
	nsCOMPtr< nsIPrompt> dialogService;
    if( GetNSIPrompt( dialogService ) )
    	rv = dialogService->AlertCheck(dialogTitle, text, checkMsg, checkValue);

	return rv;
}


NS_IMETHODIMP nsNetSupportDialog::Confirm(const PRUnichar *dialogTitle, const PRUnichar *text, PRBool *returnValue)
{

	nsresult rv = NS_ERROR_FAILURE;
	nsCOMPtr< nsIPrompt> dialogService;
    if( GetNSIPrompt( dialogService ) )
    	rv = dialogService->Confirm(dialogTitle, text, returnValue);

	return rv;
}

NS_IMETHODIMP	nsNetSupportDialog::ConfirmCheck(const PRUnichar *dialogTitle, 
                                               const PRUnichar *text, 
                                               const PRUnichar *checkMsg, 
                                               PRBool *checkValue, 
                                               PRBool *returnValue)
{

	nsresult rv = NS_ERROR_FAILURE;
	nsCOMPtr< nsIPrompt> dialogService;
    if( GetNSIPrompt( dialogService ) )
    	rv = dialogService->ConfirmCheck(dialogTitle, text, checkMsg, checkValue, returnValue);

	return rv;
}



NS_IMETHODIMP	nsNetSupportDialog::UniversalDialog
	(const PRUnichar *inTitleMessage,
	const PRUnichar *inDialogTitle, /* e.g., alert, confirm, prompt, prompt password */
	const PRUnichar *inMsg, /* main message for dialog */
	const PRUnichar *inCheckboxMsg, /* message for checkbox */
	const PRUnichar *inButton0Text, /* text for first button */
	const PRUnichar *inButton1Text, /* text for second button */
	const PRUnichar *inButton2Text, /* text for third button */
	const PRUnichar *inButton3Text, /* text for fourth button */
	const PRUnichar *inEditfield1Msg, /*message for first edit field */
	const PRUnichar *inEditfield2Msg, /* message for second edit field */
	PRUnichar **inoutEditfield1Value, /* initial and final value for first edit field */
	PRUnichar **inoutEditfield2Value, /* initial and final value for second edit field */
	const PRUnichar *inIConURL, /* url of icon to be displayed in dialog */
		/* examples are
		   "chrome://global/skin/question-icon.gif" for question mark,
		   "chrome://global/skin/alert-icon.gif" for exclamation mark
		*/
	PRBool *inoutCheckboxState, /* initial and final state of check box */
	PRInt32 inNumberButtons, /* total number of buttons (0 to 4) */
	PRInt32 inNumberEditfields, /* total number of edit fields (0 to 2) */
	PRInt32 inEditField1Password, /* is first edit field a password field */
	PRInt32 *outButtonPressed) /* number of button that was pressed (0 to 3) */
{
	nsresult rv = NS_ERROR_FAILURE;
	nsCOMPtr< nsIPrompt> dialogService;
    if( GetNSIPrompt( dialogService ) )
    {
        	rv = dialogService->UniversalDialog(
                        inTitleMessage, inDialogTitle, inMsg, inCheckboxMsg,
                        inButton0Text, inButton1Text, inButton2Text, inButton3Text,
                        inEditfield1Msg, inEditfield2Msg, inoutEditfield1Value,
                        inoutEditfield2Value, inIConURL, inoutCheckboxState, inNumberButtons,
                        inNumberEditfields, inEditField1Password, outButtonPressed);
	 }
	 return rv;
}

NS_IMETHODIMP nsNetSupportDialog::Prompt(const PRUnichar *dialogTitle, 
                                         const PRUnichar *text,
                                         const PRUnichar *passwordRealm,
                                         PRUint32 savePassword,
                                         const PRUnichar *defaultText, 
                                         PRUnichar **resultText,
                                         PRBool *returnValue)
{

	nsresult rv = NS_ERROR_FAILURE;
	nsCOMPtr< nsIPrompt> dialogService;
    if( GetNSIPrompt( dialogService ) )
		rv = dialogService->Prompt(dialogTitle, text, passwordRealm, savePassword, defaultText, resultText, returnValue);
	 
	return rv;	
}

NS_IMETHODIMP nsNetSupportDialog::PromptUsernameAndPassword(const PRUnichar *dialogTitle, 
                                                            const PRUnichar *text,
                                                            const PRUnichar *passwordRealm,
                                                            PRUint32 savePassword, 
                                                            PRUnichar **user,
                                                            PRUnichar **pwd,
                                                            PRBool *returnValue)
{

	nsresult rv = NS_ERROR_FAILURE;
	nsCOMPtr< nsIPrompt> dialogService;
    if( GetNSIPrompt( dialogService ) )
    	rv = dialogService->PromptUsernameAndPassword(dialogTitle, text, passwordRealm, savePassword, user, pwd, returnValue);
	return rv;	
}

NS_IMETHODIMP nsNetSupportDialog::PromptPassword(const PRUnichar *dialogTitle, 
                                                 const PRUnichar *text,
                                                 const PRUnichar *passwordRealm, 
                                                 PRUint32 savePassword, 
                                                 PRUnichar **pwd,
                                                 PRBool *_retval)
{
	nsresult rv = NS_ERROR_FAILURE;
	nsCOMPtr< nsIPrompt> dialogService;
    if( GetNSIPrompt( dialogService ) )
    	rv = dialogService->PromptPassword(dialogTitle, text, passwordRealm, savePassword, pwd, _retval);
	 
	return rv;	
}


nsresult nsNetSupportDialog::Select(const PRUnichar *inDialogTitle, 
                                    const PRUnichar *inMsg,
                                    PRUint32 inCount, 
                                    const PRUnichar **inList,
                                    PRInt32 *outSelection, 
                                    PRBool *_retval)
{
	nsresult rv = NS_ERROR_FAILURE;
	nsCOMPtr< nsIPrompt> dialogService;
    if( GetNSIPrompt( dialogService ) )
    	rv = dialogService->Select( inDialogTitle, inMsg,  inCount, inList, outSelection, _retval);
	 
	return rv;	
}

NS_IMPL_ISUPPORTS1( nsNetSupportDialog, nsIPrompt );
