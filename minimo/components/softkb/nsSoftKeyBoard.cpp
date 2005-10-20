/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Software KeyBoard
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Doug Turner <dougt@meer.net>
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

#ifdef WINCE
#include "windows.h"
#include "aygshell.h"
#endif

#include "memory.h"
#include "stdlib.h"

#include "nspr.h"

#include "nsXPCOM.h"
#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsString.h"
#include "nsArray.h"

#include "nsIGenericFactory.h"

#include "nsIServiceManager.h"
#include "nsIObserver.h"
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsICategoryManager.h"

#include "nsIDOMWindow.h"
#include "nsIDOM3EventTarget.h"
#include "nsIDOMEventGroup.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMEventReceiver.h"

#include "nsIDOMEvent.h"
#include "nsIDOMNSEvent.h"
#include "nsIContent.h"
#include "nsIFormControl.h"

#include "nsIDOMKeyEvent.h"
#include "nsIDOMEventListener.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"

#include "nsITimer.h"


#ifdef WINCE

#include "keybd.h"

// **************************************************************************
// Function Name: IsSmartphone
// 
// Purpose: Determine if platform is smartphone
//
// Arguments:
//	 none
//
// Return Values:
//  BOOL
//   TRUE if the current platform is a Smartphone platform
//   FALSE if the current platform is not a Smartphone platform
// 
// Description:  
//  This function retreives the current platforms type and then
//  does a case insensitive string comparison.

static BOOL IsSmartphone() 
{
    unsigned short platform[64];

    if (TRUE == SystemParametersInfo(SPI_GETPLATFORMTYPE,
                                     sizeof(platform),
                                     platform,
                                     0))
    {
      if (0 == _wcsicmp(L"Smartphone", platform)) 
      {
        return TRUE;
      }
    }
    return FALSE;   
}

// The multitap API not accessible.  We need a way to map 
BYTE GetKeyPress(PRUint32 actualKey, PRUint32 pressCount)
{
  BYTE ch = '*';

  switch (actualKey)
  {
    case '1':
    {
      pressCount %= 8;
      
      switch (pressCount)
      {
        case 1:
          ch = '.';
          break;
        case 2:
          ch = ',';
          break;
        case 3:
          ch = '-';
          break;
        case 4:
          ch = '?';
          break;
        case 5:
          ch = '!';
          break;
        case 6:
          ch = '\'';
          break;
        case 7:
          ch = '@';
          break;
        case 0:
          ch = ':';
          break;
      }
    }
    break;

    case '2':
    {
      pressCount %= 3;
      
      switch (pressCount)
      {
        case 1:
          ch = 'a';
          break;
        case 2:
          ch = 'b';
          break;
        case 0:
          ch = 'c';
          break;
      }
    }
    break;

    case '3':
    {
      pressCount %= 3;
      
      switch (pressCount)
      {
        case 1:
          ch = 'd';
          break;
        case 2:
          ch = 'e';
          break;
        case 0:
          ch = 'f';
          break;
      }
    }
    break;

    case '4':
    {
      pressCount %= 3;
      
      switch (pressCount)
      {
        case 1:
          ch = 'g';
          break;
        case 2:
          ch = 'h';
          break;
        case 0:
          ch = 'i';
          break;
      }
    }    
    break;

    case '5':
    {
      pressCount %= 3;
      
      switch (pressCount)
      {
        case 1:
          ch = 'j';
          break;
        case 2:
          ch = 'k';
          break;
        case 0:
          ch = 'l';
          break;
      }
    }
    break;


    case '6':
    {
      pressCount %= 3;
      
      switch (pressCount)
      {
        case 1:
          ch = 'm';
          break;
        case 2:
          ch = 'n';
          break;
        case 0:
          ch = 'o';
          break;
      }
    } 
    break;

    case '7':
    {
      pressCount %= 4;
      
      switch (pressCount)
      {
        case 1:
          ch = 'p';
          break;
        case 2:
          ch = 'q';
          break;
        case 3:
          ch = 'r';
          break;
        case 0:
          ch = 's';
          break;
      }
    } 
    break;

    case '8':
    {
      pressCount %= 3;
      
      switch (pressCount)
      {
        case 1:
          ch = 't';
          break;
        case 2:
          ch = 'u';
          break;
        case 0:
          ch = 'v';
          break;
      }
    }
    break;

    case '9':
    {
      pressCount %= 4;
      
      switch (pressCount)
      {
        case 1:
          ch = 'w';
          break;
        case 2:
          ch = 'x';
          break;
        case 3:
          ch = 'y';
          break;
        case 0:
          ch = 'z';
          break;
      }
    }
    break;
  }
  return ch;
}

#endif

class nsSoftKeyBoard : public nsIDOMEventListener
{
public:
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  nsSoftKeyBoard(class nsSoftKeyBoardService* aService);

  nsresult Init(nsIDOMWindow *aWindow);
  nsresult Shutdown();
  nsresult GetAttachedWindow(nsIDOMWindow * *aAttachedWindow);

private:
  ~nsSoftKeyBoard();
  PRBool ShouldOpenKeyboardFor(nsIDOMEvent* aEvent);

  void CloseSIP();
  void OpenSIP();

  nsCOMPtr<nsIDOMWindow> mTopWindow;
  class nsSoftKeyBoardService* mService;

  enum
  {
    eNumbers = 0,
    eLowerCase = 1,
    eUpperCase = 2,
  };

  PRUint32 mUsage; 

  PRUint32 mCurrentDigit;
  PRInt32 mCurrentDigitCount;
  nsCOMPtr<nsITimer> mTimer;
};


class nsSoftKeyBoardService: public nsIObserver 
{
public:  
  nsSoftKeyBoardService();  
  virtual ~nsSoftKeyBoardService();  
  
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsCOMArray<nsSoftKeyBoard> mObjects;

  PRBool mUseSoftwareKeyboard;
};

NS_INTERFACE_MAP_BEGIN(nsSoftKeyBoard)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(nsSoftKeyBoard)
NS_IMPL_RELEASE(nsSoftKeyBoard)


nsSoftKeyBoard::nsSoftKeyBoard(nsSoftKeyBoardService* aService)
{
  NS_ASSERTION(aService, "Should not create this object without a valid service");

  mService = aService; // back pointer -- no reference

  mCurrentDigit = 0;
  mCurrentDigitCount = 0;
  mUsage = eLowerCase;
}

nsSoftKeyBoard::~nsSoftKeyBoard()
{
}
#ifdef WINCE

void SoftKeyboardTimerCB(nsITimer *aTimer, void *aClosure)
{
  UINT ch = (UINT) aClosure;
  UINT flag = KeyStateDownFlag; 
  
  PostKeybdMessage( (HWND)-1, 
                    0,
                    flag,
                    1,
                    &flag, /* ignore */
                    &ch);
}
#endif

NS_IMETHODIMP
nsSoftKeyBoard::HandleEvent(nsIDOMEvent* aEvent)
{
  if (!aEvent)
    return NS_OK;

  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));

  nsCOMPtr<nsIDOMEventTarget> target;
  nsevent->GetOriginalTarget(getter_AddRefs(target));
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);


  if (!targetContent || !targetContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL))
    return NS_OK;

  nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(targetContent));
  if (!formControl)
    return NS_OK;


  PRInt32 controlType = formControl->GetType();
      
  if (controlType != NS_FORM_TEXTAREA       &&  controlType != NS_FORM_INPUT_TEXT &&
      controlType != NS_FORM_INPUT_PASSWORD &&  controlType != NS_FORM_INPUT_FILE) 
  {
    return NS_OK;
  }

  nsAutoString eventType;
  aEvent->GetType(eventType);
  
  if (eventType.EqualsLiteral("keypress"))
  {
    PRUint32 keyCode;
    nsCOMPtr<nsIDOMKeyEvent> keyEvent(do_QueryInterface(aEvent));
    
    if (!keyEvent)
      return NS_OK;
    
    if (NS_FAILED(keyEvent->GetKeyCode(&keyCode)))
      return NS_OK;
    
    if (keyCode == nsIDOMKeyEvent::DOM_VK_RETURN && controlType != NS_FORM_TEXTAREA)
    {
      CloseSIP();
    }

#ifdef WINCE

    if (IsSmartphone())
    {

      PRUint32 charCode;
      keyEvent->GetCharCode(&charCode);

#if 0
      char buffer[2];
      sprintf(buffer, "%d = %d", keyCode, charCode);
      MessageBox(0, buffer, buffer, 0);
#endif
     
      /* value determined by inspection */
      if (keyCode == 120)
      {
        // We're using this key, no one else should
        aEvent->StopPropagation();
        aEvent->PreventDefault();

        if (mTimer)
          mTimer->Cancel(); 

        keybd_event(VK_SPACE, 0, 0, 0);
        keybd_event(VK_SPACE, 0, KEYEVENTF_KEYUP, 0);

        return NS_OK;
      }

      /* value determined by inspection */
      if (keyCode == 119)
      {
        // We're using this key, no one else should
        aEvent->StopPropagation();
        aEvent->PreventDefault();

        if (mTimer)
          mTimer->Cancel(); 
        
        mUsage++;
        if (mUsage>eUpperCase) 
          mUsage=eNumbers;

        return NS_OK;
      }

      if (mUsage == eNumbers)
        return NS_OK;

      if ( charCode > nsIDOMKeyEvent::DOM_VK_0 && charCode <= nsIDOMKeyEvent::DOM_VK_9) // [0-9)
      {
        // We're using this key, no one else should
        aEvent->StopPropagation();
        aEvent->PreventDefault();

        if (mTimer)
          mTimer->Cancel();
        
        if (mCurrentDigit != charCode)
        {
          mCurrentDigit = charCode;
          mCurrentDigitCount = 1;
        }
        else
        {
          mCurrentDigitCount++;
        }

        mTimer = do_CreateInstance("@mozilla.org/timer;1");
        if (!mTimer)
          return NS_OK;

        BYTE key = GetKeyPress(mCurrentDigit, mCurrentDigitCount);

        if (mUsage == eUpperCase)
          key = _toupper(key);

        PRUint32 closure = key;
        
        mTimer->InitWithFuncCallback(SoftKeyboardTimerCB,
                                     (void*)closure,
                                     700, 
                                     nsITimer::TYPE_ONE_SHOT);
        return NS_OK;
      }
      else
      {
        mCurrentDigit = 0;
        mCurrentDigitCount = 0;
      }
      
    }
#endif

    return NS_OK;
  }

  if (eventType.EqualsLiteral("focus"))
    OpenSIP();
  else
    CloseSIP();
  
  return NS_OK;
}

void
nsSoftKeyBoard::OpenSIP()
{
  // It is okay to CloseSip if there is a hardware keyboard
  // present, but it isn't nice to use a software keyboard
  // when a hardware one is present.

  if (!mService->mUseSoftwareKeyboard)
    return;

#ifdef WINCE
  if (IsSmartphone())
    return;

  HWND hWndSIP = ::FindWindow( _T( "SipWndClass" ), NULL );
  if (hWndSIP)
    ::ShowWindow( hWndSIP, SW_SHOW);

  hWndSIP = ::FindWindow( _T( "MS_SIPBUTTON" ), NULL );
  if (hWndSIP)
    ::ShowWindow( hWndSIP, SW_SHOW);

  SHSipPreference(NULL, SIP_UP);

  SHFullScreen(GetForegroundWindow(), SHFS_SHOWSIPBUTTON);

#endif
}

void
nsSoftKeyBoard::CloseSIP()
{
#ifdef WINCE
  if (IsSmartphone())
    return;

  HWND hWndSIP = FindWindow( _T( "SipWndClass" ), NULL );
  if (hWndSIP)
  {
    ShowWindow( hWndSIP, SW_HIDE );
    SetWindowPos(hWndSIP, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
  }

  hWndSIP = FindWindow( _T( "MS_SIPBUTTON" ), NULL );
  if (hWndSIP) 
  {
    ShowWindow( hWndSIP, SW_HIDE );
    SetWindowPos(hWndSIP, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
  }

  SHSipPreference(NULL, SIP_DOWN);

  SHFullScreen(GetForegroundWindow(), SHFS_HIDESIPBUTTON);
#endif
}

PRBool
nsSoftKeyBoard::ShouldOpenKeyboardFor(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMEventTarget> target;
  nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aEvent));
  nsevent->GetOriginalTarget(getter_AddRefs(target));
  nsCOMPtr<nsIContent> targetContent = do_QueryInterface(target);

  if (targetContent && targetContent->IsContentOfType(nsIContent::eHTML_FORM_CONTROL)) 
  {
    nsCOMPtr<nsIFormControl> formControl(do_QueryInterface(targetContent));
    if (formControl)
    {
      PRInt32 controlType = formControl->GetType();
      
      if (controlType == NS_FORM_TEXTAREA ||
          controlType == NS_FORM_INPUT_TEXT ||
          controlType == NS_FORM_INPUT_PASSWORD ||
          controlType == NS_FORM_INPUT_FILE) 
      {
        return PR_TRUE;
      }
    }
  }
  return PR_FALSE;
}


nsresult
nsSoftKeyBoard::Init(nsIDOMWindow *aWindow)
{
  mTopWindow = aWindow;

  nsCOMPtr<nsPIDOMWindow> privateWindow = do_QueryInterface(aWindow);
  
  if (!privateWindow)
    return NS_ERROR_UNEXPECTED;
  
  nsIChromeEventHandler *chromeEventHandler = privateWindow->GetChromeEventHandler();
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(chromeEventHandler));
  if (!receiver)
    return NS_ERROR_UNEXPECTED;
  
  receiver->AddEventListener(NS_LITERAL_STRING("focus"), this, PR_TRUE);
  receiver->AddEventListener(NS_LITERAL_STRING("blur"), this, PR_TRUE);
  receiver->AddEventListener(NS_LITERAL_STRING("keypress"), this, PR_TRUE);

  
  return NS_OK;
}

nsresult
nsSoftKeyBoard::Shutdown()
{
  nsCOMPtr<nsPIDOMWindow> privateWindow = do_QueryInterface(mTopWindow);
  
  if (!privateWindow)
    return NS_ERROR_UNEXPECTED;
  
  nsIChromeEventHandler *chromeEventHandler = privateWindow->GetChromeEventHandler();
  nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(chromeEventHandler));
  if (!receiver)
    return NS_ERROR_UNEXPECTED;

  receiver->RemoveEventListener(NS_LITERAL_STRING("focus"), this, PR_TRUE);
  receiver->RemoveEventListener(NS_LITERAL_STRING("blur"), this, PR_TRUE);
  
  mTopWindow = nsnull;

  return NS_OK;
}

nsresult 
nsSoftKeyBoard::GetAttachedWindow(nsIDOMWindow * *aAttachedWindow)
{
  NS_IF_ADDREF(*aAttachedWindow = mTopWindow);
  return NS_OK;
}

nsSoftKeyBoardService::nsSoftKeyBoardService()  
{
  mUseSoftwareKeyboard = PR_TRUE;
}  

nsSoftKeyBoardService::~nsSoftKeyBoardService()  
{
}  

NS_IMPL_ISUPPORTS1(nsSoftKeyBoardService, nsIObserver)

NS_IMETHODIMP
nsSoftKeyBoardService::Observe(nsISupports *aSubject, const char *aTopic, const PRUnichar *aData)
{
  nsresult rv;

  if (!strcmp(aTopic,"domwindowopened")) 
  {
    nsCOMPtr<nsIDOMWindow> chromeWindow = do_QueryInterface(aSubject);
    
    nsSoftKeyBoard* sn = new nsSoftKeyBoard(this);

    if (!sn)
      return NS_ERROR_OUT_OF_MEMORY;

    sn->Init(chromeWindow);
    
    mObjects.AppendObject(sn);  // the array owns the only reference to sn.

    return NS_OK;
  }
  
  if (!strcmp(aTopic,"domwindowclosed")) 
  {
    nsCOMPtr<nsIDOMWindow> chromeWindow = do_QueryInterface(aSubject);
    // need to find it in our array
  
    PRInt32 count = mObjects.Count();
    for (PRInt32 i = 0; i < count; i++)
    {
      nsSoftKeyBoard* sn = mObjects[i];
      nsCOMPtr<nsIDOMWindow> attachedWindow;
      sn->GetAttachedWindow(getter_AddRefs(attachedWindow));

      if (attachedWindow == chromeWindow) 
      {
        sn->Shutdown();
        mObjects.RemoveObjectAt(i);
        return NS_OK;
      }
    }
    return NS_OK;
  }
  
  if (!strcmp(aTopic,"xpcom-startup")) 
  {
    nsCOMPtr<nsIWindowWatcher> windowWatcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    windowWatcher->RegisterNotification(this);

    nsCOMPtr<nsIPrefBranch2> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    prefBranch->AddObserver("skey.", this, PR_FALSE);

    return NS_OK;
  }

  if (!strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID)) 
  {
    nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(aSubject);
    nsXPIDLCString cstr;
    
    const char* pref = NS_ConvertUCS2toUTF8(aData).get();
    
    if (!strcmp(pref, "skey.enabled"))
    {
      PRBool enabled;
      prefBranch->GetBoolPref(pref, &enabled);

      mUseSoftwareKeyboard = enabled;
    }
    return NS_OK;
  }
  return NS_OK;
}


//------------------------------------------------------------------------------
//  XPCOM REGISTRATION BELOW
//------------------------------------------------------------------------------

#define SoftKeyBoard_CID \
{ 0x46f2dcf5, 0x25a1, 0x4459, \
{0xa6, 0x30, 0x13, 0x53, 0x9f, 0xe5, 0xa8, 0x6b} }

#define SoftKeyBoard_ContractID "@mozilla.org/softkeyboard;1"

#define SoftKeyBoardService_CID \
{ 0xde3dc3a1, 0x420f, 0x4cec, \
{0x9c, 0x3d, 0xf7, 0x71, 0xab, 0x22, 0xae, 0xf7} }

#define SoftKeyBoardService_ContractID "@mozilla.org/softkbservice/service;1"


static NS_METHOD SoftKeyBoardServiceRegistration(nsIComponentManager *aCompMgr,
                                           nsIFile *aPath,
                                           const char *registryLocation,
                                           const char *componentType,
                                           const nsModuleComponentInfo *info)
{
  nsresult rv;
  
  nsCOMPtr<nsIServiceManager> servman = do_QueryInterface((nsISupports*)aCompMgr, &rv);
  if (NS_FAILED(rv))
    return rv;
  
  
  nsCOMPtr<nsICategoryManager> catman;
  servman->GetServiceByContractID(NS_CATEGORYMANAGER_CONTRACTID, 
                                  NS_GET_IID(nsICategoryManager), 
                                  getter_AddRefs(catman));
  
  if (NS_FAILED(rv))
    return rv;
  
  char* previous = nsnull;
  rv = catman->AddCategoryEntry("xpcom-startup",
                                "SoftKeyBoardService", 
                                SoftKeyBoardService_ContractID,
                                PR_TRUE, 
                                PR_TRUE, 
                                &previous);
  if (previous)
    nsMemory::Free(previous);
  
  return rv;
}

static NS_METHOD SoftKeyBoardServiceUnregistration(nsIComponentManager *aCompMgr,
                                             nsIFile *aPath,
                                             const char *registryLocation,
                                             const nsModuleComponentInfo *info)
{
  nsresult rv;
  
  nsCOMPtr<nsIServiceManager> servman = do_QueryInterface((nsISupports*)aCompMgr, &rv);
  if (NS_FAILED(rv))
    return rv;
  
  nsCOMPtr<nsICategoryManager> catman;
  servman->GetServiceByContractID(NS_CATEGORYMANAGER_CONTRACTID, 
                                  NS_GET_IID(nsICategoryManager), 
                                  getter_AddRefs(catman));
  
  if (NS_FAILED(rv))
    return rv;
  
  rv = catman->DeleteCategoryEntry("xpcom-startup",
                                   "SoftKeyBoardService", 
                                   PR_TRUE);
  
  return rv;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSoftKeyBoardService)



static const nsModuleComponentInfo components[] =
{
  { "SoftKeyBoardService", 
    SoftKeyBoardService_CID, 
    SoftKeyBoardService_ContractID,
    nsSoftKeyBoardServiceConstructor,
    SoftKeyBoardServiceRegistration,
    SoftKeyBoardServiceUnregistration
  }
  
};

NS_IMPL_NSGETMODULE(SoftKeyBoardModule, components)
