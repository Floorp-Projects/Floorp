/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsIDOMWindow.h"
#include "nsICommonDialogs.h"
#include "nsCOMPtr.h"
#include "nsIScriptGlobalObject.h"
#include "nsXPComFactory.h"
#include "nsIComponentManager.h"

static NS_DEFINE_CID( kDialogParamBlock,          NS_DialogParamBlock_CID);

class nsCommonDialogs: public nsICommonDialogs
{
public:
enum {  eMsg =0, eCheckboxMsg =1, eIconURL =2 , eTitleMessage =3, eEditfield1Msg =4, eEditfield2Msg =5, eEditfield1Value = 6, eEditfield2Value = 7 };
enum { eButtonPressed = 0, eCheckboxState = 1, eNumberButtons = 2, eNumberEditfields =3, ePasswordEditfieldMask =4 };
			nsCommonDialogs();
  virtual	~nsCommonDialogs();
  NS_IMETHOD Alert(nsIDOMWindow *inParent, const PRUnichar *inMsg);
  NS_IMETHOD Confirm(nsIDOMWindow *inParent, const PRUnichar *inMsg, PRBool *_retval);
  NS_IMETHOD ConfirmCheck(nsIDOMWindow *inParent, const PRUnichar *inMsg, const PRUnichar *inCheckMsg, PRBool *outCheckValue, PRBool *_retval);
  NS_IMETHOD Prompt(nsIDOMWindow *inParent, const PRUnichar *inMsg, const PRUnichar *inDefaultText, PRUnichar **result, PRBool *_retval);
  NS_IMETHOD PromptUsernameAndPassword(nsIDOMWindow *inParent, const PRUnichar *inMsg, PRUnichar **outUser, PRUnichar **outPassword, PRBool *_retval);
  NS_IMETHOD PromptPassword(nsIDOMWindow *inParent, const PRUnichar *inMsg, PRUnichar **outPassword, PRBool *_retval);

  NS_IMETHOD DoDialog(nsIDOMWindow* inParent, nsIDialogParamBlock *ioParamBlock, const char *inChromeURL);
  NS_DECL_ISUPPORTS
private:
};

const char* kPromptURL="chrome://global/content/baseconfirm.xul";

const char* kQuestionIconURL ="chrome://global/skin/question-icon.gif";
const char* kAlertIconURL ="chrome://global/skin/alert-icon.gif";
const char* kWarningIconURL ="chrome://global/skin/message-icon.gif";

nsCommonDialogs::nsCommonDialogs()
{

}

nsCommonDialogs::~nsCommonDialogs()
{
}

NS_IMETHODIMP nsCommonDialogs::Alert(nsIDOMWindow *inParent, const PRUnichar *inMsg)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlock,
                                                      0,
                                                      nsIDialogParamBlock::GetIID(),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons, 1 );
	block->SetString( eMsg, inMsg );
	nsString url( kAlertIconURL );
	block->SetString( eIconURL, url.GetUnicode());
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	NS_IF_RELEASE( block );
	return rv;
}

NS_IMETHODIMP nsCommonDialogs::Confirm(nsIDOMWindow *inParent, const PRUnichar *inMsg, PRBool *_retval)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlock,
                                                      0,
                                                      nsIDialogParamBlock::GetIID(),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons,2 );
	block->SetString( eMsg, inMsg );
	nsString url( kQuestionIconURL );
	block->SetString( eIconURL, url.GetUnicode());
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	PRInt32 buttonPressed = 0;
	block->GetInt( eButtonPressed, &buttonPressed );
	*_retval = buttonPressed ? PR_FALSE : PR_TRUE;
	NS_IF_RELEASE( block );
	return rv;
}

NS_IMETHODIMP nsCommonDialogs::ConfirmCheck(nsIDOMWindow *inParent, const PRUnichar *inMsg, const PRUnichar *inCheckMsg, PRBool *outCheckValue, PRBool *_retval)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlock,
                                                      0,
                                                      nsIDialogParamBlock::GetIID(),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons,2 );
	block->SetString( eMsg, inMsg );
	nsString url( kQuestionIconURL );
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

NS_IMETHODIMP nsCommonDialogs::Prompt(nsIDOMWindow *inParent, const PRUnichar *inMsg, const PRUnichar *inDefaultText, PRUnichar **result, PRBool *_retval)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlock,
                                                      0,
                                                      nsIDialogParamBlock::GetIID(),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	block->SetInt( eNumberButtons,2 );
	block->SetString( eMsg, inMsg );
	nsString url( kQuestionIconURL );
	block->SetString( eIconURL, url.GetUnicode());
	block->SetInt( eNumberEditfields, 1 );
	block->SetString( eEditfield1Value, inDefaultText );
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	PRInt32 tempInt = 0;
	block->GetString( eEditfield1Value, result );
	*_retval = tempInt ? PR_FALSE : PR_TRUE;
	
	NS_IF_RELEASE( block );
	return rv;
}

NS_IMETHODIMP nsCommonDialogs::PromptUsernameAndPassword(nsIDOMWindow *inParent, const PRUnichar *inMsg, PRUnichar **outUser, PRUnichar **outPassword, PRBool *_retval)
{
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlock,
                                                      0,
                                                      nsIDialogParamBlock::GetIID(),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	NS_IF_RELEASE( block );
	return rv;
}

NS_IMETHODIMP nsCommonDialogs::PromptPassword(nsIDOMWindow *inParent, const PRUnichar *inMsg, PRUnichar **outPassword, PRBool *_retval)
{	
	nsresult rv;
	nsIDialogParamBlock* block = NULL;
	rv = nsComponentManager::CreateInstance( kDialogParamBlock,
                                                      0,
                                                      nsIDialogParamBlock::GetIID(),
                                                      (void**)&block );
      
	if ( NS_FAILED( rv ) )
		return rv;
	// Stuff in Parameters
	
	
	rv = DoDialog( inParent, block, kPromptURL );
	
	NS_IF_RELEASE( block );
	return rv;
}


 NS_IMETHODIMP nsCommonDialogs::DoDialog(nsIDOMWindow* inParent, nsIDialogParamBlock *ioParamBlock, const char *inChromeURL)
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
                                                    "svs%ip",
                                                    inChromeURL,
                                                    JSVAL_NULL,
                                                    "chrome,modal",
                                                    (const nsIID*)(&nsIDialogParamBlock::GetIID()),
                                                    (nsISupports*)ioParamBlock
                                                  );
                    if ( argv ) {
                        nsIDOMWindow *newWindow;
                        rv = inParent->OpenDialog( jsContext, argv, 4, &newWindow );
                        if ( NS_SUCCEEDED( rv ) )
                        {
    //                        newWindow->Release();
                        } else
                        {
                        }
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
 
 
 static NS_DEFINE_IID(kICommonDialogs, nsICommonDialogs::GetIID() );
NS_IMPL_ADDREF(nsCommonDialogs);
NS_IMPL_RELEASE(nsCommonDialogs);
NS_IMPL_QUERY_INTERFACE(nsCommonDialogs, kICommonDialogs);

// Entry point to create nsAppShellService factory instances...
NS_DEF_FACTORY(CommonDialogs, nsCommonDialogs)

nsresult NS_NewCommonDialogsFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIFactory* inst;
  
  inst = new nsCommonDialogsFactory;
  if (nsnull == inst)
  {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else
  {
    NS_ADDREF(inst);
  }
  *aResult = inst;
  return rv;
}