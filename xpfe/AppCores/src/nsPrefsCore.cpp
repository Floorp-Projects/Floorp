
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsPrefsCore.h"

#include "nsIPref.h"
#include "nsIURL.h"
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsFileSpec.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppShellCIDs.h"
#include "nsAppCoresManager.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"

#include "nsIScriptGlobalObject.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWebShellWindow.h"
#include "nsIDOMHTMLInputElement.h"

#include "plstr.h"
#include "prmem.h"

#include <ctype.h>

// Globals - how many K are we wasting by putting these in every file?
static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPrefsCoreIID,            NS_IDOMPREFSCORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,          nsIDOMDocument::GetIID());
static NS_DEFINE_IID(kIDocumentIID,             nsIDocument::GetIID());
static NS_DEFINE_IID(kIAppShellServiceIID,      NS_IAPPSHELL_SERVICE_IID);

static NS_DEFINE_IID(kPrefsCoreCID,             NS_PREFSCORE_CID);
static NS_DEFINE_IID(kBrowserWindowCID,         NS_BROWSER_WINDOW_CID);
static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
#ifdef NS_DEBUG
    static PRBool firstTime = PR_TRUE;
#endif

//----------------------------------------------------------------------------------------
nsPrefsCore::nsPrefsCore()
//----------------------------------------------------------------------------------------
:    mTreeScriptContext(nsnull)
,    mPanelScriptContext(nsnull)
,    mTreeWindow(nsnull)
,    mPanelWindow(nsnull)
,    mPrefs(nsnull)
{
    
    printf("Created nsPrefsCore\n");
#ifdef NS_DEBUG
    NS_ASSERTION(firstTime, "There can be only one");
    firstTime = PR_FALSE;
#endif
}

//----------------------------------------------------------------------------------------
nsPrefsCore::~nsPrefsCore()
//----------------------------------------------------------------------------------------
{
    NS_IF_RELEASE(mTreeScriptContext);
    NS_IF_RELEASE(mPanelScriptContext);

    NS_IF_RELEASE(mTreeWindow);
    NS_IF_RELEASE(mPanelWindow);

    nsServiceManager::ReleaseService(kPrefCID, mPrefs);
    
#ifdef NS_DEBUG
    NS_ASSERTION(firstTime, "There can be only one");
    firstTime = PR_TRUE;
#endif
}


NS_IMPL_ISUPPORTS_INHERITED(nsPrefsCore, nsBaseAppCore, nsIDOMPrefsCore)

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
//----------------------------------------------------------------------------------------
{
    NS_PRECONDITION(nsnull != aScriptObject, "null arg");
    nsresult res = NS_OK;
    if (nsnull == mScriptObject) 
    {
            res = NS_NewScriptPrefsCore(aContext, 
                                (nsISupports *)(nsIDOMPrefsCore*)this, 
                                nsnull, 
                                &mScriptObject);
    }

    *aScriptObject = mScriptObject;
    return res;
}

//----------------------------------------------------------------------------------------
nsresult nsPrefsCore::InitializePrefsManager()
//----------------------------------------------------------------------------------------
{ 
	nsIPref* prefs;
	nsresult rv = nsServiceManager::GetService(kPrefCID, kIPrefIID, (nsISupports**)&prefs);
	if (NS_FAILED(rv))
	    return rv;
	if (!prefs)
	    return NS_ERROR_FAILURE;
	
	nsIFileLocator* locator;
	rv = nsServiceManager::GetService(kFileLocatorCID, kIFileLocatorIID, (nsISupports**)&locator);
	if (NS_FAILED(rv))
	    return rv;
	if (!locator)
	    return NS_ERROR_FAILURE;
	    
	nsFileSpec newPrefs;
	rv = locator->GetFileLocation(nsSpecialFileSpec::App_PreferencesFile50, &newPrefs);
#if 0
	if (NS_FAILED(rv) || !newPrefs.Exists())
	{
	    nsFileSpec oldPrefs;
	    rv = locator->GetFileLocation(App_PreferencesFile40, &oldPrefs);
		if (NS_FAILED(rv) || !oldPrefs.Exists())
		{
		    rv = locator->GetFileLocation(App_PreferencesFile30, &oldPrefs);
		}
		if (NS_SUCCEEDED(rv) && oldPrefs.Exists())
		{
		    nsFileSpec newParent;
		    rv = locator->GetFileLocation(App_PrefsDirectory50, &newParent);
		    if (NS_SUCCEEDED(rv))
		    {
			    oldPrefs.Copy(newParent);
			    const char* oldName = oldPrefs.GetLeafName();
			    newPrefs = newParent + oldName;
			    delete [] oldName;
			    newPrefs.Rename("prefs.js");
			}
		}
	}
#endif
    nsServiceManager::ReleaseService(kFileLocatorCID, locator);
    
	if (NS_SUCCEEDED(rv) && newPrefs.Exists())
        rv = prefs->Startup(newPrefs.GetCString());
    else
        rv = NS_ERROR_FAILURE;

    if (prefs && NS_FAILED(rv))
        nsServiceManager::ReleaseService(kPrefCID, prefs);
    
    if (NS_FAILED(rv))
        return rv;

    mPrefs = prefs;
    return NS_OK;
} // nsPrefsCore::InitializePrefsManager

//----------------------------------------------------------------------------------------
static PRBool CheckAndStrip(
    nsString& ioString,
    const char* inPrefix)
//----------------------------------------------------------------------------------------
{
    if (ioString.Find(inPrefix) != 0)
        return PR_FALSE;
    ioString.Cut(0, PL_strlen(inPrefix));
    return PR_TRUE;
}

//----------------------------------------------------------------------------------------
static PRInt16 CheckOrdinalAndStrip(nsString& ioString, PRInt16& outOrdinal)
//----------------------------------------------------------------------------------------
{
    PRInt32 colonPos = ioString.Find(':');
    if (colonPos <= 0)
        return PR_FALSE;
    char* intString = ioString.ToNewCString();
    intString[colonPos] = 0;
    if (!isdigit(*intString))
    {
        outOrdinal = 0;
        return PR_TRUE;
    }
    ioString.Cut(0, colonPos + 1);
    short result = 0;
    sscanf(intString, "%hd", &result);
    delete [] intString;
    outOrdinal = result;
    return PR_TRUE;
}

//----------------------------------------------------------------------------------------
static PRBool ParseElementIDString(
    nsString& ioWidgetIDString,
    nsPrefsCore::TypeOfPref& outType,
    PRInt16& outOrdinal)
// If the id in the HTML is "pref:bool:general.startup.browser".
//     outType will be set to eBool
//     ioWidgetIDString will be modified to "general.startup.browser".
//----------------------------------------------------------------------------------------
{
    if (!CheckAndStrip(ioWidgetIDString, "pref:"))
        return PR_FALSE;
    if (!CheckOrdinalAndStrip(ioWidgetIDString, outOrdinal))
        return PR_FALSE;
    if (CheckAndStrip(ioWidgetIDString, "bool:"))
    {
        outType = nsPrefsCore::eBool;
        return PR_TRUE;
    }
    if (CheckAndStrip(ioWidgetIDString, "int:"))
    {
        outType = nsPrefsCore::eInt;
        return PR_TRUE;
    }
    if (CheckAndStrip(ioWidgetIDString, "string:"))
    {
        outType = nsPrefsCore::eString;
        return PR_TRUE;
    }
    if (CheckAndStrip(ioWidgetIDString, "path:"))
    {
        outType = nsPrefsCore::ePath;
        return PR_TRUE;
    }
    return PR_FALSE;
} // ParseElementIDString

//----------------------------------------------------------------------------------------
nsresult nsPrefsCore::InitializeOneWidget(
    nsIDOMHTMLInputElement* inElement,
    const nsString& inWidgetType,
    const char* inPrefName,
    TypeOfPref inPrefType,
    PRInt16 inPrefOrdinal)
//----------------------------------------------------------------------------------------
{
    // See comments in FinalizeOneWidget for an explanation of the subtree technique. When
    // initializing a widget, we have to check the subtree first, to see if the user has
    // visited that panel previously and changed the value.
    char tempPrefName[256];
    PL_strcpy(tempPrefName, "temp_tree.");
    PL_strcat(tempPrefName, inPrefName);
    switch (inPrefType)
    {
    	case eBool:
    	{
    	    PRBool boolVal;
    	    // Check the subtree first, then the real tree.
    	    // If the preference value is not set at all, let the HTML
    	    // determine the setting.
    	    if (NS_SUCCEEDED(mPrefs->GetBoolPref(tempPrefName, &boolVal))
    	    || NS_SUCCEEDED(mPrefs->GetBoolPref(inPrefName, &boolVal)))
    	    {
	    	    if (inWidgetType == "checkbox")
	    	    {
		    	    boolVal = (PRBool)(boolVal ^ inPrefOrdinal);
	        	    inElement->SetDefaultChecked(boolVal);
	        	    inElement->SetChecked(boolVal);
	    	    }
	    	    else if (inWidgetType == "radio" && inPrefOrdinal == boolVal)
	    	    {
                    // Radio pairs representing a boolean pref must have their
                    // ordinals "0" and "1". They work just like radio buttons
                    // representing int prefs.
                    // Turn on the radio whose ordinal matches the value.
                    // The others will turn off automatically.
                    inElement->SetDefaultChecked(PR_TRUE);
                    inElement->SetChecked(PR_TRUE);
	    	    }
        	}
    	    break;
    	}
    	case eInt:
    	{
    	    PRInt32 intVal;
    	    // Check the subtree first, then the real tree.
    	    // If the preference value is not set at all, let the HTML
    	    // determine the setting.
    	    if (NS_SUCCEEDED(mPrefs->GetIntPref(tempPrefName, &intVal))
    	    || NS_SUCCEEDED(mPrefs->GetIntPref(inPrefName, &intVal)))
    	    {
                // Turn on the radio whose ordinal matches the value.
                // The others will turn off automatically.
                if (inWidgetType == "radio" && inPrefOrdinal == intVal)
    	        {
                    inElement->SetDefaultChecked(PR_TRUE);
                    inElement->SetChecked(PR_TRUE);
                }
            }
    	    break;
    	}
    	case eString:
    	{
    	    // Check the subtree first, then the real tree.
    	    // If the preference value is not set at all, let the HTML
    	    // determine the setting.
    	    char* charVal;
    	    if (NS_SUCCEEDED(mPrefs->CopyCharPref(tempPrefName, &charVal))
    	    || NS_SUCCEEDED(mPrefs->CopyCharPref(inPrefName, &charVal)))
    	    {
    	        nsString newValue = charVal;
                PR_Free(charVal);
                inElement->SetValue(newValue);
    	    }
    	    break;
    	}
    	case ePath:
    	{
    	    // Check the subtree first, then the real tree.
    	    // If the preference value is not set at all, let the HTML
    	    // determine the setting.
    	    char* charVal;
    	    if (NS_SUCCEEDED(mPrefs->CopyPathPref(tempPrefName, &charVal))
    	    || NS_SUCCEEDED(mPrefs->CopyPathPref(inPrefName, &charVal)))
    	    {
    	        nsString newValue = charVal;
                PR_Free(charVal);
                inElement->SetValue(newValue);
    	    }
    	    break;
    	}
    }
    return NS_OK;
}

//----------------------------------------------------------------------------------------
nsresult nsPrefsCore::InitializeWidgetsRecursive(nsIDOMNode* inParentNode)
//----------------------------------------------------------------------------------------
{
    if (!inParentNode)
        return NS_OK;
    
    PRBool hasChildren;
    inParentNode->HasChildNodes(&hasChildren); 
    if (hasChildren)
    {
	    //nsCOMPtr<nsIDOMNodeList> childList;
	    //inParentNode->GetChildNodes(getter_AddRefs(childList));
	    nsCOMPtr<nsIDOMNode> nextChild;
	    nsresult aResult = inParentNode->GetFirstChild(getter_AddRefs(nextChild));
	    while (NS_SUCCEEDED(aResult) && nextChild)
	    {
	        nsCOMPtr<nsIDOMNode> child = nextChild;
	        InitializeWidgetsRecursive(child);
	        aResult = child->GetNextSibling(getter_AddRefs(nextChild));
	    }
    }
    // OK, the buck stops here. Do the real work.
    PRUint16 aNodeType;
    nsresult rv = inParentNode->GetNodeType(&aNodeType);
    if (aNodeType == nsIDOMNode::ELEMENT_NODE)
    {
        nsCOMPtr<nsIDOMHTMLInputElement> element = do_QueryInterface(inParentNode);
        if (element)
        {
            nsString prefName;
            TypeOfPref prefType;
            PRInt16 ordinal;
            element->GetId( prefName);            
            if (ParseElementIDString(prefName, prefType, ordinal))
            {
                nsString widgetType;
                element->GetType(widgetType);
                char* prefNameString = prefName.ToNewCString();
                InitializeOneWidget(element, widgetType, prefNameString, prefType, ordinal);
                delete [] prefNameString;
            }
        }
    }
    return NS_OK;
} // InitializeWidgetsRecursive

//----------------------------------------------------------------------------------------
nsresult nsPrefsCore::InitializePrefWidgets()
//----------------------------------------------------------------------------------------
{
    NS_ASSERTION(mPanelWindow, "panel window is null");
    NS_ASSERTION(mPrefs, "prefs pointer is null");
    if (!mPanelWindow || !mPrefs)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMDocument> aDOMDoc;
    mPanelWindow->GetDocument(getter_AddRefs(aDOMDoc));
    return InitializeWidgetsRecursive(aDOMDoc);
    
} // nsPrefsCore::InitializePrefWidgets

//----------------------------------------------------------------------------------------
nsresult nsPrefsCore::FinalizeOneWidget(
    nsIDOMHTMLInputElement* inElement,
    const nsString& inWidgetType,
    const char* inPrefName,
    TypeOfPref inPrefType,
    PRInt16 inPrefOrdinal)
//----------------------------------------------------------------------------------------
{
    // As each panel is replaced, the values of its widgets are written out to a subtree
    // of the prefs tree with root at "temp_tree". This subtree is rather sparse, since it
    // only contains prefs (if any) that are represented by widgets in panels that the user
    // visits. If the user clicks "OK" at the end, then prefs in this subtree will be
    // copied back over to the real tree. This subtree will be deleted at the end
    // in either case (OK or Cancel).
    char tempPrefName[256];
    PL_strcpy(tempPrefName, "temp_tree.");
    PL_strcat(tempPrefName, inPrefName);
    switch (inPrefType)
    {
    	case eBool:
    	{
    	    PRBool boolVal;
    	    nsresult rv = inElement->GetChecked(&boolVal);
    	    if (NS_FAILED(rv))
    	        return rv;
    	    if (inWidgetType == "checkbox")
    	    {
	   		        boolVal = (PRBool)(boolVal ^ inPrefOrdinal);    	    
    			    mPrefs->SetBoolPref(tempPrefName, boolVal);
	        }
	        else if (inWidgetType == "radio" && boolVal)
	    	    {
		    	    // The radio that is ON writes out its ordinal. Others do nothing.
	                mPrefs->SetBoolPref(tempPrefName, inPrefOrdinal);
	    	    }
    	    break;
    	}
    	case eInt:
    	{
            if (inWidgetType == "radio")
	        {
	    	    // The radio that is ON writes out its ordinal. Others do nothing.
                PRBool boolVal;
                nsresult rv = inElement->GetChecked(&boolVal);
                if (NS_FAILED(rv) || !boolVal)
    	            return rv;
                mPrefs->SetBoolPref(tempPrefName, inPrefOrdinal);
            }
    	    break;
    	}
    	case eString:
    	{
    	    nsString fieldValue;
            nsresult rv = inElement->GetValue(fieldValue);
    	    if (NS_FAILED(rv))
    	        return rv;
    	    char* s = fieldValue.ToNewCString();
    	    mPrefs->SetCharPref(tempPrefName, s);
    	    delete [] s;
    	    break;
    	}
    	case ePath:
    	{
    	    nsString fieldValue;
            nsresult rv = inElement->GetValue(fieldValue);
    	    if (NS_FAILED(rv))
    	        return rv;
    	    char* s = fieldValue.ToNewCString();
    	    mPrefs->SetPathPref(tempPrefName, s, PR_TRUE);
    	    delete [] s;
    	    break;
    	    break;
    	}
    }
//    if (inWidgetType == "checkbox" || inWidgetType = "radio")
//    {
//        inElement->SetAttribute(attributeToSet, newValue);
//    }
    return NS_OK;
}

//----------------------------------------------------------------------------------------
nsresult nsPrefsCore::FinalizeWidgetsRecursive(nsIDOMNode* inParentNode)
//----------------------------------------------------------------------------------------
{
    if (!inParentNode)
        return NS_OK;
    
    PRBool hasChildren;
    inParentNode->HasChildNodes(&hasChildren); 
    if (hasChildren)
    {
	    //nsCOMPtr<nsIDOMNodeList> childList;
	    //inParentNode->GetChildNodes(getter_AddRefs(childList));
	    nsCOMPtr<nsIDOMNode> nextChild;
	    nsresult aResult = inParentNode->GetFirstChild(getter_AddRefs(nextChild));
	    while (NS_SUCCEEDED(aResult) && nextChild)
	    {
	        nsCOMPtr<nsIDOMNode> child = nextChild;
	        FinalizeWidgetsRecursive(child);
	        aResult = child->GetNextSibling(getter_AddRefs(nextChild));
	    }
    }
    // OK, the buck stops here. Do the real work.
    PRUint16 aNodeType;
    nsresult rv = inParentNode->GetNodeType(&aNodeType);
    if (aNodeType == nsIDOMNode::ELEMENT_NODE)
    {
        nsCOMPtr<nsIDOMHTMLInputElement> element = do_QueryInterface(inParentNode);
        if (element)
        {
            nsString prefName;
            TypeOfPref prefType;
            PRInt16 ordinal;
            element->GetId( prefName);            
            if (ParseElementIDString(prefName, prefType, ordinal))
            {
                nsString widgetType;
                element->GetType(widgetType);
                char* prefNameString = prefName.ToNewCString();
                FinalizeOneWidget(element, widgetType, prefNameString, prefType, ordinal);
                delete [] prefNameString;
            }
        }
    }
    return NS_OK;
} // FinalizeWidgetsRecursive

//----------------------------------------------------------------------------------------
nsresult nsPrefsCore::FinalizePrefWidgets()
//----------------------------------------------------------------------------------------
{
    NS_ASSERTION(mPanelWindow, "panel window is null");
    NS_ASSERTION(mPrefs, "prefs pointer is null");
    if (!mPanelWindow || !mPrefs)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMDocument> aDOMDoc;
    mPanelWindow->GetDocument(getter_AddRefs(aDOMDoc));
    return FinalizeWidgetsRecursive(aDOMDoc);
    
} // nsPrefsCore::FinalizePrefWidgets

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::Init(const nsString& aId)
//----------------------------------------------------------------------------------------
{ 
    nsresult rv = nsBaseAppCore::Init(aId);
    if (NS_FAILED(rv))
        return rv;
    
	rv = InitializePrefsManager();
    if (NS_FAILED(rv))
        return rv;

    return NS_OK;
} // nsPrefsCore::Init

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::ShowWindow(nsIDOMWindow* /*aCurrentFrontWin*/)
//----------------------------------------------------------------------------------------
{
    // Get app shell service.
    nsIAppShellService *appShell;
    nsresult rv = nsServiceManager::GetService(
                        kAppShellServiceCID,
                        kIAppShellServiceIID,
                        (nsISupports**)&appShell);

    if (NS_FAILED(rv))
        return rv;

    nsString controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";

    nsCOMPtr<nsIURL> url;
    rv = NS_NewURL(getter_AddRefs(url), "resource:/res/samples/PrefsWindow.html");
    if (NS_FAILED(rv))
        return rv;

    // Create "save to disk" nsIXULCallbacks...
    //nsIXULWindowCallbacks *cb = new nsFindDialogCallbacks( aURL, aContentType );
    nsIXULWindowCallbacks *cb = nsnull;

    nsIWebShellWindow* newWindow;
    rv = appShell->CreateDialogWindow(
                       nsnull,
                       url,
                       controllerCID,
                       newWindow,
                       nsnull,
                       cb,
                       504, 346 );
    if (newWindow != nsnull)
        newWindow->ShowModal();
    return rv;
} // nsPrefsCore::ShowWindow

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::ChangePanel(const nsString& aURL)
// Start loading of a new prefs panel.
//----------------------------------------------------------------------------------------
{
    NS_ASSERTION(mPanelWindow, "panel window is null");
    if (!mPanelWindow)
        return NS_OK;

    nsresult rv = FinalizePrefWidgets();
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(mPanelWindow));
    if (!globalScript)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIWebShell> webshell;
    globalScript->GetWebShell(getter_AddRefs(webshell));
    if (!webshell)
        return NS_ERROR_FAILURE;
    webshell->LoadURL(aURL);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::PanelLoaded(nsIDOMWindow* aWin)
// Callback after loading of a new prefs panel.
//----------------------------------------------------------------------------------------
{
    // Out with the old!
    
    if (mPanelWindow != aWin)
    {
        NS_IF_RELEASE(mPanelWindow);
        mPanelWindow = aWin;
        NS_IF_ADDREF(mPanelWindow);
    }
    
    // In with the new!
    if (mPanelWindow)
    {
        mPanelScriptContext = GetScriptContext(mPanelWindow);
        nsresult rv = InitializePrefWidgets();
        if (NS_FAILED(rv))
            return rv;
    }
	return NS_OK;
}

//----------------------------------------------------------------------------------------
static nsCOMPtr<nsIWebShellWindow>
	DOMWindowToWebShellWindow(nsIDOMWindow *DOMWindow)
// horribly complicated routine simply to convert from one to the other
//----------------------------------------------------------------------------------------	
{
    nsCOMPtr<nsIWebShellWindow> webWindow;
    nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(DOMWindow));
    nsCOMPtr<nsIWebShell> webshell;
    if (globalScript)
        globalScript->GetWebShell(getter_AddRefs(webshell));
    if (webshell)
    {
        nsCOMPtr<nsIWebShellContainer> webshellContainer;
        webshell->GetContainer(*getter_AddRefs(webshellContainer));
        webWindow = do_QueryInterface(webshellContainer);
    }
    return webWindow;
}

//----------------------------------------------------------------------------------------
static nsresult Close(nsIDOMWindow*& dw)
//----------------------------------------------------------------------------------------	
{
    if (!dw)
        return NS_ERROR_FAILURE;
    nsIDOMWindow* top;
    dw->GetTop(&top);
    if (!top)
        return NS_ERROR_FAILURE;
    nsCOMPtr<nsIWebShellWindow> parent = DOMWindowToWebShellWindow(top);
    if (parent)
        parent->Close();
    NS_IF_RELEASE(dw);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::SavePrefs()
//----------------------------------------------------------------------------------------
{
    FinalizePrefWidgets();
	// Do the prefs stuff...
	mPrefs->CopyPrefsTree("temp_tree", "");
	mPrefs->DeleteBranch("temp_tree");
	mPrefs->SavePrefFile();
	// Then close    
	return Close(mPanelWindow);
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefsCore::CancelPrefs()
//----------------------------------------------------------------------------------------
{
	// Do the prefs stuff...
	mPrefs->DeleteBranch("temp_tree");
	
	// Then close    
	return Close(mPanelWindow);
}
