#ifndef _nsPrefWindow_h_
#define _nsPrefWindow_h_

#include "nsIPrefWindow.h"
//#include "nsIAppShellComponentImpl.h"
#include "nsString.h"

class nsIDOMNode;
class nsIDOMHTMLInputElement;
class nsIDOMHTMLSelectElement;
class nsIDOMHTMLDivElement;
class nsIPref;
class nsIWebShellWindow;

//========================================================================================
class nsPrefWindow
//========================================================================================
  : public nsIPrefWindow
//  , public nsAppShellComponentImpl
{
  public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_PREFWINDOW_CID);

    nsPrefWindow();
    virtual ~nsPrefWindow();
                 

    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIPrefWindow interface functions.
    NS_DECL_NSIPREFWINDOW
    
	enum TypeOfPref
	{
	    eNoType        = 0
	  , eBool
	  , eInt
	  , eString
	  , ePath
	  , eColor
	};

    static nsPrefWindow* Get();
    static PRBool        InstanceExists();

  protected:
    
    nsresult             InitializePrefWidgets();
    nsresult             InitializeWidgetsRecursive(nsIDOMNode* inParentNode);
    nsresult             InitializeOneInputWidget(
                             nsIDOMHTMLInputElement* inElement,
                             const nsString& inWidgetType,
                             const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRInt16 inPrefOrdinal);
    nsresult             InitializeOneSelectWidget(
                             nsIDOMHTMLSelectElement* inElement,
							 const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRInt16 inPrefOrdinal);
    nsresult             InitializeOneColorWidget(
                             nsIDOMHTMLDivElement* inElement,
                             const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRUint32 inColor);
    nsresult             FinalizePrefWidgets();
    nsresult             FinalizeWidgetsRecursive(nsIDOMNode* inParentNode);
    nsresult             FinalizeOneInputWidget(
                             nsIDOMHTMLInputElement* inElement,
                             const nsString& inWidgetType,
                             const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRInt16 inPrefOrdinal);
    nsresult             FinalizeOneSelectWidget(
                             nsIDOMHTMLSelectElement* inElement,
							 const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRInt16 inPrefOrdinal);
    nsresult             FinalizeOneColorWidget(
                             nsIDOMHTMLDivElement* inElement,
                             const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRUint32 inColor);
    char*                GetSubstitution(nsString& formatstr);

  protected:

	static nsPrefWindow* sPrefWindow;
	static PRUint32      sInstanceCount;
    
        nsString             mTreeScript;     
    nsString             mPanelScript;     

    nsIDOMWindow*        mTreeFrame;
    nsIDOMWindow*        mPanelFrame;
    
    nsIPref*             mPrefs;

    char**               mSubStrings;
}; // class nsPrefWindow

#endif // _nsPrefWindow_h_
