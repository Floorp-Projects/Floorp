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

#include "nsPrefWindow.h"

#include "nsIPref.h"
#include "nsIURL.h"
#ifdef NECKO
#include "nsIIOService.h"
#include "nsIURL.h"
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
#endif // NECKO
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
#include "nsFileSpec.h"
#include "nsIFileSpecWithUI.h"
#include "nsFileStream.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsIScriptGlobalObject.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIWebShellWindow.h"
#include "nsIDOMHTMLInputElement.h"

#include "plstr.h"
#include "prprf.h"
#include "prmem.h"

#include <ctype.h>

// Globals - how many K are we wasting by putting these in every file?
static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,          nsIDOMDocument::GetIID());
static NS_DEFINE_IID(kIDocumentIID,             nsIDocument::GetIID());

static NS_DEFINE_IID(kIPrefIID, NS_IPREF_IID);
static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);

static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);

nsPrefWindow* nsPrefWindow::sPrefWindow = nsnull;

static void DOMWindowToWebShellWindow(nsIDOMWindow *DOMWindow, nsCOMPtr<nsIWebShellWindow> *webWindow);

//----------------------------------------------------------------------------------------
nsPrefWindow::nsPrefWindow()
//----------------------------------------------------------------------------------------
:    mTreeFrame(nsnull)
,    mPanelFrame(nsnull)
,    mPrefs(nsnull)
,    mSubStrings(nsnull)
{
	NS_INIT_REFCNT();
    
    printf("Created nsPrefWindow\n");
#ifdef NS_DEBUG
    NS_ASSERTION(!sPrefWindow, "There can be only one");
#endif

    // initialize substrings to null
    mSubStrings = new char*[PREFWINDOW_MAX_STRINGS+1];
    for (int i=0; i < PREFWINDOW_MAX_STRINGS; i++)
    	mSubStrings[i]=nsnull;
    mSubStrings[PREFWINDOW_MAX_STRINGS] = nsnull;
} // nsPrefWindow::nsPrefWindow

//----------------------------------------------------------------------------------------
nsPrefWindow::~nsPrefWindow()
//----------------------------------------------------------------------------------------
{
    NS_IF_RELEASE(mTreeFrame);
    NS_IF_RELEASE(mPanelFrame);
    nsServiceManager::ReleaseService(kPrefCID, mPrefs);
    if (mSubStrings)
    {
        for (int i=0; i< PREFWINDOW_MAX_STRINGS; i++)
            if (mSubStrings[i])
                delete[] mSubStrings[i];
        delete[] mSubStrings;
    }
    sPrefWindow = 0;
}

NS_IMPL_ISUPPORTS( nsPrefWindow, nsIPrefWindow::GetIID())

//----------------------------------------------------------------------------------------
/* static */ nsPrefWindow* nsPrefWindow::Get()
// Called by the factory.
//----------------------------------------------------------------------------------------
{
	if (sPrefWindow)
		NS_ADDREF(sPrefWindow);
	else
	{
		nsPrefWindow* prefWindow = new nsPrefWindow();
		nsresult rv;
		if (prefWindow)
			rv = prefWindow->QueryInterface(GetIID(), (void**)&sPrefWindow);
		else
			rv = NS_ERROR_OUT_OF_MEMORY;			
		if (NS_FAILED(rv) && prefWindow)
			delete prefWindow;
		else
		    sPrefWindow = prefWindow;
		// Note: QueryInterface AddRefs if it succeeds.
	}
	return sPrefWindow;
}

//----------------------------------------------------------------------------------------
/* static */ PRBool nsPrefWindow::InstanceExists()
//----------------------------------------------------------------------------------------
{
	return (sPrefWindow != nsnull);
}
//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefWindow::showWindow(
		const PRUnichar *id,
		nsIDOMWindow *currentFrontWin,
		const PRUnichar* panelURL)
// The ID is currently only used to display debugging text on the console.
//----------------------------------------------------------------------------------------
{ 
#ifdef NS_DEBUG
	nsOutputConsoleStream stream;
    char buf[512];
#endif
	if (mPrefs)
	{
#ifdef NS_DEBUG
	    stream << "еее showWindow called twice! Existing pref object reused by "
	    	<< nsString(id).ToCString(buf, sizeof(buf))
	    	<< nsEndl;
#endif
    }
    else
    {
#ifdef NS_DEBUG
	    stream << "Pref object initialized by "
	    	<< nsString(id).ToCString(buf, sizeof(buf))
	    	<< nsEndl;
#endif
	    nsIPref* prefs = nsnull;
	    nsresult rv = nsServiceManager::GetService(
	    	kPrefCID, nsIPref::GetIID(), (nsISupports**)&prefs);
	    if (NS_FAILED(rv))
	        return rv;
	    mPrefs = prefs;
    }
    if (!mPrefs)
        return NS_ERROR_FAILURE;

    // (code adapted from nsToolkitCore::ShowModal. yeesh.)
    nsCOMPtr<nsIURI> urlObj;
    char * urlStr = "chrome://pref/content/";
    nsresult rv;
#ifndef NECKO
    rv = NS_NewURL(getter_AddRefs(urlObj), urlStr);
#else
    NS_WITH_SERVICE(nsIIOService, service, kIOServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsIURI *uri = nsnull;
    rv = service->NewURI(urlStr, nsnull, &uri);
    if (NS_FAILED(rv)) return rv;

    rv = uri->QueryInterface(nsIURI::GetIID(), (void**)&urlObj);
    NS_RELEASE(uri);
#endif // NECKO
    if (NS_FAILED(rv))
        return rv;

    NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    nsIXULWindowCallbacks *cb = nsnull;
    nsCOMPtr<nsIWebShellWindow> parent;
    DOMWindowToWebShellWindow(currentFrontWin, &parent);
    rv = appShell->RunModalDialog(nsnull, parent, urlObj,
                               NS_CHROME_ALL_CHROME | NS_CHROME_OPEN_AS_DIALOG,
                               cb, 504, 436);
    return rv;
} // nsPrefWindow::showWindow()

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
    PRInt32 colonPos = ioString.FindChar(':');
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
    nsPrefWindow::TypeOfPref& outType,
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
        outType = nsPrefWindow::eBool;
        return PR_TRUE;
    }
    if (CheckAndStrip(ioWidgetIDString, "int:"))
    {
        outType = nsPrefWindow::eInt;
        return PR_TRUE;
    }
    if (CheckAndStrip(ioWidgetIDString, "string:"))
    {
        outType = nsPrefWindow::eString;
        return PR_TRUE;
    }
    if (CheckAndStrip(ioWidgetIDString, "path:"))
    {
        outType = nsPrefWindow::ePath;
        return PR_TRUE;
    }
    return PR_FALSE;
} // ParseElementIDString

//----------------------------------------------------------------------------------------
nsresult nsPrefWindow::InitializeOneWidget(
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
                if (inWidgetType == "radio")
                {
                    // Turn on the radio whose ordinal matches the value.
                    // The others will turn off automatically.
                    if (inPrefOrdinal == intVal)
                    {
                        inElement->SetDefaultChecked(PR_TRUE);
                        inElement->SetChecked(PR_TRUE);
                    }
                }
                else if (inWidgetType == "text")
                {
                    char charVal[32];
                    sprintf(charVal, "%d", (int)intVal);
                    nsString newValue(charVal);
                    inElement->SetValue(newValue);
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
            nsIFileSpec *specVal;
            nsresult rv = mPrefs->GetFilePref(tempPrefName, &specVal);
            if (NS_FAILED(rv))
                rv = mPrefs->GetFilePref(inPrefName, &specVal);            
            if (NS_FAILED(rv))
                return rv;
            char* newValue;
            specVal->GetNativePath(&newValue);
            inElement->SetValue(newValue);
            NS_RELEASE(specVal);
            break;
        }
	case eNoType:
	{
		NS_ASSERTION(0, "eNoType not handled");
	}
    }
    return NS_OK;
} // nsPrefWindow::InitializeOneWidget

//----------------------------------------------------------------------------------------
nsresult nsPrefWindow::InitializeWidgetsRecursive(nsIDOMNode* inParentNode)
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
    if (NS_SUCCEEDED(rv) && aNodeType == nsIDOMNode::ELEMENT_NODE)
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
                char* prefNameString = GetSubstitution(prefName);
                InitializeOneWidget(element, widgetType, prefNameString,
                                    prefType, ordinal);
                PR_Free(prefNameString);
            }
        }
    }
    return NS_OK;
} // InitializeWidgetsRecursive

//----------------------------------------------------------------------------------------
nsresult nsPrefWindow::InitializePrefWidgets()
//----------------------------------------------------------------------------------------
{
    NS_ASSERTION(mPanelFrame, "panel window is null");
    NS_ASSERTION(mPrefs, "prefs pointer is null");
    if (!mPanelFrame || !mPrefs)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMDocument> aDOMDoc;
    mPanelFrame->GetDocument(getter_AddRefs(aDOMDoc));
    return InitializeWidgetsRecursive(aDOMDoc);
    
} // nsPrefWindow::InitializePrefWidgets

//----------------------------------------------------------------------------------------
nsresult nsPrefWindow::FinalizeOneWidget(
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
                mPrefs->SetIntPref(tempPrefName, inPrefOrdinal);
            }
            else if (inWidgetType == "text")
            {
                nsString fieldValue;
                nsresult rv = inElement->GetValue(fieldValue);
                if (NS_FAILED(rv))
                    return rv;
                char* s = fieldValue.ToNewCString();
                mPrefs->SetIntPref(tempPrefName, atoi(s));
                delete [] s;
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
            nsIFileSpecWithUI* specValue = NS_CreateFileSpecWithUI();
            if (!specValue)
            	return NS_ERROR_FAILURE;
            nsCAutoString str(fieldValue);
            specValue->SetNativePath((char*)(const char*)str);
            mPrefs->SetFilePref(tempPrefName, specValue, PR_TRUE);
            NS_RELEASE(specValue);
            break;
        }
	case eNoType:
	{
		NS_ASSERTION(0, "eNoType not handled");
	}
    }
//    if (inWidgetType == "checkbox" || inWidgetType = "radio")
//    {
//        inElement->SetAttribute(attributeToSet, newValue);
//    }
    return NS_OK;
} // nsPrefWindow::FinalizeOneWidget

//----------------------------------------------------------------------------------------
nsresult nsPrefWindow::FinalizeWidgetsRecursive(nsIDOMNode* inParentNode)
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
    if (NS_SUCCEEDED(rv) && aNodeType == nsIDOMNode::ELEMENT_NODE)
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
                char* prefNameString = GetSubstitution(prefName);
                FinalizeOneWidget(element, widgetType, prefNameString, prefType, ordinal);
                PR_Free(prefNameString);
            }
        }
    }
    return NS_OK;
} // FinalizeWidgetsRecursive

//----------------------------------------------------------------------------------------
nsresult nsPrefWindow::FinalizePrefWidgets()
//----------------------------------------------------------------------------------------
{
    NS_ASSERTION(mPanelFrame, "panel window is null");
    NS_ASSERTION(mPrefs, "prefs pointer is null");
    if (!mPanelFrame || !mPrefs)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMDocument> aDOMDoc;
    mPanelFrame->GetDocument(getter_AddRefs(aDOMDoc));
    return FinalizeWidgetsRecursive(aDOMDoc);
    
} // nsPrefWindow::FinalizePrefWidgets

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefWindow::changePanel(const PRUnichar* aURL)
// Start loading of a new prefs panel.
//----------------------------------------------------------------------------------------
{
    NS_ASSERTION(mPanelFrame, "panel window is null");
    if (!mPanelFrame)
        return NS_OK;

    nsresult rv = FinalizePrefWidgets();
    if (NS_FAILED(rv))
        return rv;

    nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(mPanelFrame));
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
NS_IMETHODIMP nsPrefWindow::panelLoaded(nsIDOMWindow* aWin)
// Callback after loading of a new prefs panel.
//----------------------------------------------------------------------------------------
{
    // Out with the old!
    
    if (mPanelFrame != aWin)
    {
        NS_IF_RELEASE(mPanelFrame);
        mPanelFrame = aWin;
        NS_IF_ADDREF(mPanelFrame);
    }
    
    // In with the new!
    if (mPanelFrame)
    {
        nsresult rv = InitializePrefWidgets();
        if (NS_FAILED(rv))
            return rv;
    }
    return NS_OK;
}


//----------------------------------------------------------------------------------------    
static void DOMWindowToWebShellWindow(
              nsIDOMWindow *DOMWindow,
              nsCOMPtr<nsIWebShellWindow> *webWindow)
//----------------------------------------------------------------------------------------    
{
  if (!DOMWindow)
    return; // with webWindow unchanged -- its constructor gives it a null ptr

  nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(DOMWindow));
  nsCOMPtr<nsIWebShell> webshell, rootWebshell;
  if (globalScript)
    globalScript->GetWebShell(getter_AddRefs(webshell));
  if (webshell)
    webshell->GetRootWebShellEvenIfChrome(*getter_AddRefs(rootWebshell));
  if (rootWebshell)
  {
    nsCOMPtr<nsIWebShellContainer> webshellContainer;
    rootWebshell->GetContainer(*getter_AddRefs(webshellContainer));
    *webWindow = do_QueryInterface(webshellContainer);
  }
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
    nsCOMPtr<nsIWebShellWindow> parent;
    DOMWindowToWebShellWindow(top, &parent);
    if (parent)
        parent->Close();
    NS_IF_RELEASE(dw);
    return NS_OK;
}

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefWindow::savePrefs()
//----------------------------------------------------------------------------------------
{
    FinalizePrefWidgets();
    if (mPrefs)
    {
        // Do the prefs stuff...
        mPrefs->CopyPrefsTree("temp_tree", "");
        mPrefs->DeleteBranch("temp_tree");
        mPrefs->SavePrefFile();
    }
    // Then close    
    return Close(mPanelFrame);
} // nsPrefWindow::SavePrefs

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefWindow::cancelPrefs()
//----------------------------------------------------------------------------------------
{
    // Do the prefs stuff...
    if (mPrefs)
        mPrefs->DeleteBranch("temp_tree");
    
    // Then close    
    return Close(mPanelFrame);
}

//----------------------------------------------------------------------------------------
char* nsPrefWindow::GetSubstitution(nsString& formatstr)
//----------------------------------------------------------------------------------------
{
#define substring(_i) mSubStrings[_i] ? mSubStrings[_i] : ""
	char *cformatstr = formatstr.ToNewCString();
	// for now use PR_smprintf and hardcode the strings as parameters
	char* result = PR_smprintf(
				cformatstr,
				substring(0),
				substring(1),
				substring(2),
				substring(3),
				substring(4),
				substring(5),
				substring(6),
				substring(7),
				substring(8),
				substring(9));
	delete[] cformatstr;
	return result;
} // nsPrefWindow::GetSubstitution

//----------------------------------------------------------------------------------------
NS_IMETHODIMP nsPrefWindow::setSubstitutionVar(
	PRUint32 aStringnum,
    const char* aVal)
//----------------------------------------------------------------------------------------
{

	if (aStringnum < PREFWINDOW_MAX_STRINGS)
	{
		NS_WARNING("substitution string number to large");
		return NS_ERROR_UNEXPECTED;
	}
	if (mSubStrings[aStringnum])
		delete[] mSubStrings[aStringnum];
	nsString s(aVal);
	mSubStrings[aStringnum] = s.ToNewCString();
	return NS_OK;
} // nsPrefWindow::SetSubstitutionVar

