#ifndef _nsPrefWindow_h_
#define _nsPrefWindow_h_

#include "nsIPrefWindow.h"
#include "nsString.h"

class nsIDOMNode;
class nsIDOMHTMLInputElement;
class nsIPref;

//========================================================================================
class nsPrefWindow
//========================================================================================
  : public nsIPrefWindow 
{
  public:

    nsPrefWindow();
    virtual ~nsPrefWindow();
                 

    NS_DECL_ISUPPORTS

	NS_IMETHOD Init(const PRUnichar *id);
	NS_IMETHOD ShowWindow(nsIDOMWindow *currentFrontWin);
	NS_IMETHOD ChangePanel(const PRUnichar *url);
	NS_IMETHOD PanelLoaded(nsIDOMWindow *win);
	NS_IMETHOD SavePrefs();
	NS_IMETHOD CancelPrefs();
	NS_IMETHOD SetSubstitutionVar(PRUint32 stringnum, const char *val);
    
	enum TypeOfPref
	{
	    eNoType        = 0
	  , eBool
	  , eInt
	  , eString
	  , ePath
	};

  protected:
    
    nsresult             InitializePrefWidgets();
    nsresult             InitializeWidgetsRecursive(nsIDOMNode* inParentNode);
    nsresult             InitializeOneWidget(
                             nsIDOMHTMLInputElement* inElement,
                             const nsString& inWidgetType,
                             const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRInt16 inPrefOrdinal);
    nsresult             FinalizePrefWidgets();
    nsresult             FinalizeWidgetsRecursive(nsIDOMNode* inParentNode);
    nsresult             FinalizeOneWidget(
                             nsIDOMHTMLInputElement* inElement,
                             const nsString& inWidgetType,
                             const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRInt16 inPrefOrdinal);
    char*                GetSubstitution(nsString& formatstr);

  protected:

    nsString             mTreeScript;     
    nsString             mPanelScript;     

    nsIDOMWindow*        mTreeWindow;
    nsIDOMWindow*        mPanelWindow;
    
    nsIPref*             mPrefs;

    char**               mSubStrings;
}; // class nsPrefWindow

#endif // _nsPrefWindow_h_
