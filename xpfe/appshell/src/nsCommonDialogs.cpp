/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsIDOMWindowInternal.h"
#include "nsCommonDialogs.h"
#include "nsCOMPtr.h"
#include "nsIScriptGlobalObject.h"
#include "nsXPComFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"

static NS_DEFINE_CID( kDialogParamBlockCID,          NS_DialogParamBlock_CID);
 


const char* kPromptURL="chrome://global/content/commonDialog.xul";

const char* kQuestionIconURL ="chrome://global/skin/question-icon.gif";
const char* kAlertIconURL ="chrome://global/skin/alert-icon.gif";
const char* kWarningIconURL ="chrome://global/skin/message-icon.gif";

nsCommonDialogs::nsCommonDialogs()
{
	NS_INIT_REFCNT();
}

nsCommonDialogs::~nsCommonDialogs()
{
}

NS_IMETHODIMP nsCommonDialogs::Alert(nsIDOMWindowInternal *inParent,  const PRUnichar *inWindowTitle, const PRUnichar *inMsg)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlockCID,
                                                      0,
                                                      NS_GET_IID(nsIDialogParamBlock),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons, 1 );
	block->SetString( eMsg, inMsg );
	
	block->SetString( eDialogTitle,inWindowTitle );
   
	nsString url; url.AssignWithConversion( kAlertIconURL );
	block->SetString( eIconURL, url.GetUnicode());
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	NS_IF_RELEASE( block );
	return rv;
}



NS_IMETHODIMP nsCommonDialogs::AlertCheck(nsIDOMWindowInternal *inParent,  const PRUnichar *inWindowTitle,const PRUnichar *inMsg, const PRUnichar *inCheckMsg, PRBool *outCheckValue)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlockCID,
                                                      0,
                                                      NS_GET_IID(nsIDialogParamBlock),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons,1 );
	block->SetString( eMsg, inMsg );

	block->SetString( eDialogTitle, inWindowTitle );

	nsString url; url.AssignWithConversion( kAlertIconURL );

	block->SetString( eIconURL, url.GetUnicode());
	block->SetString( eCheckboxMsg, inCheckMsg );
	block->SetInt(eCheckboxState, *outCheckValue );
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	block->GetInt(eCheckboxState, outCheckValue  );
	
    NS_IF_RELEASE( block );
	return rv;
}

NS_IMETHODIMP nsCommonDialogs::Confirm(nsIDOMWindowInternal *inParent, const PRUnichar *inWindowTitle, const PRUnichar *inMsg, PRBool *_retval)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlockCID,
                                                      0,
                                                      NS_GET_IID(nsIDialogParamBlock),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons,2 );
	block->SetString( eMsg, inMsg );
	
	block->SetString( eDialogTitle, inWindowTitle );
   
	nsString url; url.AssignWithConversion( kQuestionIconURL );
	block->SetString( eIconURL, url.GetUnicode());
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	PRInt32 buttonPressed = 0;
	block->GetInt( eButtonPressed, &buttonPressed );
	*_retval = buttonPressed ? PR_FALSE : PR_TRUE;
	NS_IF_RELEASE( block );
	return rv;
}

NS_IMETHODIMP nsCommonDialogs::ConfirmCheck(nsIDOMWindowInternal *inParent,  const PRUnichar *inWindowTitle,const PRUnichar *inMsg, const PRUnichar *inCheckMsg, PRBool *outCheckValue, PRBool *_retval)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlockCID,
                                                      0,
                                                      NS_GET_IID(nsIDialogParamBlock),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons,2 );
	block->SetString( eMsg, inMsg );

	block->SetString( eDialogTitle, inWindowTitle );

	nsString url; url.AssignWithConversion( kQuestionIconURL );
	block->SetString( eIconURL, url.GetUnicode());
	block->SetString( eCheckboxMsg, inCheckMsg );
	block->SetInt(eCheckboxState, *outCheckValue );
	
	rv = DoDialog( inParent, block, kPromptURL );
	PRInt32 tempInt = 0;
	block->GetInt( eButtonPressed, &tempInt );
	*_retval = tempInt ? PR_FALSE : PR_TRUE;
	
	block->GetInt(eCheckboxState, & tempInt  );
	*outCheckValue = PRBool( tempInt );
	
	NS_IF_RELEASE( block );
	return rv;
}

/* Note: It would be nice if someone someday converts all the other dialogs so that they
   all call UniversalDialog rather than calling on DoDialog directly.  This should save
   a few bytes of memory
*/
NS_IMETHODIMP nsCommonDialogs::UniversalDialog
	(nsIDOMWindowInternal *inParent,
	const PRUnichar *inTitleMessage,
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
	nsresult rv;

	/* check for at least one button */
	if (inNumberButtons < 1) {
		rv = NS_ERROR_FAILURE;
	}

	/* create parameter block */

	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance
		(kDialogParamBlockCID, 0, NS_GET_IID(nsIDialogParamBlock), (void**)&block );
	if (NS_FAILED(rv)) {
		return rv;
	}

	/* load up input parameters */

	block->SetString(eTitleMessage, inTitleMessage);
	block->SetString(eDialogTitle, inDialogTitle);
	block->SetString(eMsg, inMsg);
	block->SetString(eCheckboxMsg, inCheckboxMsg);
	if (inNumberButtons >= 4) {
		block->SetString(eButton3Text, inButton3Text);
	}
	if (inNumberButtons >= 3) {
		block->SetString(eButton2Text, inButton2Text);
	}
	if (inNumberButtons >= 2) {
		block->SetString(eButton1Text, inButton1Text);
	}
	if (inNumberButtons >= 1) {
		block->SetString(eButton0Text, inButton0Text);
	}
	if (inNumberEditfields >= 2) {
		block->SetString(eEditfield2Msg, inEditfield2Msg);
		block->SetString(eEditfield2Value, *inoutEditfield2Value);
	}
	if (inNumberEditfields >= 1) {
		block->SetString(eEditfield1Msg, inEditfield1Msg);
		block->SetString(eEditfield1Value, *inoutEditfield1Value);
		block->SetInt(eEditField1Password, inEditField1Password);
	}
	if (inIConURL) {
		block->SetString(eIconURL, inIConURL);
	} else {
		block->SetString(eIconURL, NS_ConvertASCIItoUCS2(kQuestionIconURL).GetUnicode());
	}
	if (inCheckboxMsg) {
		block->SetInt(eCheckboxState, *inoutCheckboxState);
	}
	block->SetInt(eNumberButtons, inNumberButtons);
	block->SetInt(eNumberEditfields, inNumberEditfields);

	/* perform the dialog */

	rv = DoDialog( inParent, block, kPromptURL);

	/* get back output parameters */

	if (outButtonPressed) {
		block->GetInt(eButtonPressed, outButtonPressed);
	}
	if (inCheckboxMsg && inoutCheckboxState) {
		block->GetInt(eCheckboxState, inoutCheckboxState);
	}
	if ((inNumberEditfields >= 2) && inoutEditfield2Value) {
		block->GetString(eEditfield2Value, inoutEditfield2Value);
	}
	if ((inNumberEditfields >= 1) && inoutEditfield1Value) {
		block->GetString(eEditfield1Value, inoutEditfield1Value);
	}

	/* destroy parameter block and return */	

	NS_IF_RELEASE(block);
	return rv;
}

NS_IMETHODIMP nsCommonDialogs::Prompt(nsIDOMWindowInternal *inParent, const PRUnichar *inWindowTitle, const PRUnichar *inMsg, const PRUnichar *inDefaultText, PRUnichar **result, PRBool *_retval)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlockCID,
                                                      0,
                                                      NS_GET_IID(nsIDialogParamBlock),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons,2 );
	block->SetString( eMsg, inMsg );
	
	block->SetString( eDialogTitle, inWindowTitle );
	
	nsString url; url.AssignWithConversion( kQuestionIconURL );
	block->SetString( eIconURL, url.GetUnicode());
	block->SetInt( eNumberEditfields, 1 );
	block->SetString( eEditfield1Value, inDefaultText );
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	
	block->GetString( eEditfield1Value, result );
	PRInt32 tempInt = 0;
	block->GetInt( eButtonPressed, &tempInt );
	*_retval = tempInt ? PR_FALSE : PR_TRUE;
	
	NS_IF_RELEASE( block );
	return rv;
}

NS_IMETHODIMP nsCommonDialogs::PromptUsernameAndPassword(nsIDOMWindowInternal *inParent, const PRUnichar *inWindowTitle, const PRUnichar *inMsg, PRUnichar **outUser, PRUnichar **outPassword, PRBool *_retval)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlockCID,
                                                      0,
                                                      NS_GET_IID(nsIDialogParamBlock),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons,2 );
	block->SetString( eMsg, inMsg );
	
	block->SetString( eDialogTitle, inWindowTitle );

	nsString url; url.AssignWithConversion( kQuestionIconURL );
	block->SetString( eIconURL, url.GetUnicode());
	block->SetInt( eNumberEditfields, 2 );
	block->SetString( eEditfield1Value, *outUser );
	block->SetString( eEditfield2Value, *outPassword );
	
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	block->GetString( eEditfield1Value, outUser );
	block->GetString( eEditfield2Value, outPassword );
	PRInt32 tempInt = 0;
	block->GetInt( eButtonPressed, &tempInt );
	*_retval = tempInt ? PR_FALSE : PR_TRUE;
	NS_IF_RELEASE( block );
	return rv;
}

NS_IMETHODIMP nsCommonDialogs::PromptPassword(nsIDOMWindowInternal *inParent,  const PRUnichar *inWindowTitle, const PRUnichar *inMsg, PRUnichar **outPassword, PRBool *_retval)
{	
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlockCID,
                                                      0,
                                                      NS_GET_IID(nsIDialogParamBlock),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons,2 );
	block->SetString( eMsg, inMsg );
	
	block->SetString( eDialogTitle, inWindowTitle );
	
	nsString url; url.AssignWithConversion( kQuestionIconURL );
	block->SetString( eIconURL, url.GetUnicode());
	block->SetInt( eNumberEditfields, 1 );
	block->SetInt( eEditField1Password, 1 );
	rv = DoDialog( inParent, block, kPromptURL );
	block->GetString( eEditfield1Value, outPassword );
	PRInt32 tempInt = 0;
	block->GetInt( eButtonPressed, &tempInt );
	*_retval = tempInt ? PR_FALSE : PR_TRUE;
	
	NS_IF_RELEASE( block );
	return rv;
}



nsresult nsCommonDialogs::Select(nsIDOMWindowInternal *inParent, const PRUnichar *inDialogTitle, const PRUnichar* inMsg, PRUint32 inCount, const PRUnichar **inList, PRInt32 *outSelection, PRBool *_retval)
{	
	nsresult rv;
	const PRInt32 eSelection = 2 ;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlockCID,
                                                      0,
                                                      NS_GET_IID(nsIDialogParamBlock),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	block->SetNumberStrings( inCount + 2 );
	if (inDialogTitle) {
		block->SetString( 0, inDialogTitle );
	}
	
	block->SetString(1, inMsg );
	block->SetInt( eSelection, inCount );
	for ( PRUint32 i = 2; i<= inCount+1; i++ )
	{
		nsAutoString temp(inList[i-2]  );
		const PRUnichar* text = temp.GetUnicode();
		
		block->SetString( i, text);
	}
	*outSelection = -1;
	static NS_DEFINE_CID(	kCommonDialogsCID, NS_CommonDialog_CID );
	NS_WITH_SERVICE(nsICommonDialogs, dialog, kCommonDialogsCID, &rv);
	 if ( NS_SUCCEEDED( rv ) )
	 {
 		rv = dialog->DoDialog( inParent, block, "chrome://global/content/selectDialog.xul" );
	
		PRInt32 buttonPressed = 0;
		block->GetInt( nsICommonDialogs::eButtonPressed, &buttonPressed );
		block->GetInt( eSelection, outSelection );
		*_retval = buttonPressed ? PR_FALSE : PR_TRUE;
		NS_IF_RELEASE( block );
	}
	return rv;
}


 NS_IMETHODIMP nsCommonDialogs::DoDialog(nsIDOMWindowInternal* inParent, nsIDialogParamBlock *ioParamBlock, const char *inChromeURL)
 {
  nsresult rv = NS_OK;

    if ( inParent && ioParamBlock &&inChromeURL )
    {
        // Get JS context from parent window.
        nsCOMPtr<nsIScriptGlobalObject> sgo = do_QueryInterface( inParent, &rv );
        if ( NS_SUCCEEDED( rv ) && sgo )
        {
            nsCOMPtr<nsIScriptContext> context;
            sgo->GetContext( getter_AddRefs( context ) );
            if ( context )
            {
                JSContext *jsContext = (JSContext*)context->GetNativeContext();
                if ( jsContext ) {
                    void *stackPtr;
                    jsval *argv = JS_PushArguments( jsContext,
                                                    &stackPtr,
                                                    "sss%ip",
                                                    inChromeURL,
                                                    "_blank",
                                                    "centerscreen,chrome,modal,titlebar",
                                                    (const nsIID*)(&NS_GET_IID(nsIDialogParamBlock)),
                                                    (nsISupports*)ioParamBlock
                                                  );
                    if ( argv ) {
                        nsCOMPtr<nsIDOMWindowInternal> newWindow;
                        rv = inParent->OpenDialog( jsContext, argv, 4, getter_AddRefs(newWindow) );
                        JS_PopArguments( jsContext, stackPtr );
                    }
                    else
                    {
                    	
                        NS_WARNING( "JS_PushArguments failed\n" );
                        rv = NS_ERROR_FAILURE;
                    }
                }
                else
                {
                    NS_WARNING(" GetNativeContext failed\n" );
                    rv = NS_ERROR_FAILURE;
                }
            }
            else
            {
                NS_WARNING( "GetContext failed\n" );
                rv = NS_ERROR_FAILURE;
            }
        }
        else
        {
            NS_WARNING( "QueryInterface (for nsIScriptGlobalObject) failed \n" );
        }
    }
    else
    {
        NS_WARNING( " OpenDialogWithArg was passed a null pointer!\n" );
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
 }
 
NS_IMPL_ISUPPORTS1(nsCommonDialogs, nsICommonDialogs)
