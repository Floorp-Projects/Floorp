#ifndef _nsStdAlert_h
#define _nsStdAlert_h

#include  "nsString.h"
#include "nsIXULWindowCallbacks.h"

class nsIWebShell;


class nsNetSupportDialog  : public nsIXULWindowCallbacks
{
public:
			nsNetSupportDialog();
	void 	Alert( const nsString &aText );
  	PRBool 	Confirm( const nsString &aText );

  	PRBool 	Prompt(	const nsString &aText,
  				 	const nsString &aDefault,
                          nsString &aResult );

  	PRBool 	PromptUserAndPassword(  const nsString &aText,
                                        nsString &aUser,
                                        nsString &aPassword );

    PRBool PromptPassword( 	const nsString &aText, nsString &aPassword );
    // COM
	NS_DECL_ISUPPORTS
private:
	void Init();
	nsresult   DoDialog(  nsString& inXULURL, PRBool &wasOKed );
	NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
	NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell);
	const nsString*	mDefault;
	nsString*	mUser;
	nsString*	mPassword;
	const nsString*	mMsg;
	nsIWebShell*	mWebShell;
};
#endif