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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   Milind Changire <changire@yahoo.com>
 */

#ifndef QMozillaContainer_h__
#define QMozillaContainer_h__

#include <qobject.h>
#include <qwidget.h>
#include "nsIWebShell.h"

class nsQtEventProcessor;
class nsIXlibWindowService;
class nsIPref;


class QMozillaContainer : public QWidget, public nsIWebShellContainer
{
	Q_OBJECT
public:
	QMozillaContainer( QWidget* parentWidget = NULL );
	virtual ~QMozillaContainer( );

	typedef enum { ReloadFromCache = 0, ReloadBypassCache } QMozillaReloadType;

public slots:
	void loadURL( const char *url );
	void reload( QMozillaReloadType type = ReloadFromCache );
	void forward();
	void back();
	void stop();


signals:
	void urlLoadStarted();
	void urlLoadProgressed( const char* url, int progressed, int max );
	void urlLoadEnded();


protected:
  virtual bool x11Event( XEvent* xevent );
	virtual void focusInEvent ( QFocusEvent * );
	virtual void focusOutEvent ( QFocusEvent * );

	
public:
  // nsISupports: macro to support the nsI* mozilla interfaces
	NS_DECL_ISUPPORTS
		
	// nsIWebShellContainer methods
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason);
	NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL);
	NS_IMETHOD ProgressLoadURL( nsIWebShell* aShell, const PRUnichar* aURL, 
															PRInt32 aProgress, PRInt32 aProgressMax);
	NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus);
	NS_IMETHOD NewWebShell(PRUint32 aChromeMask, PRBool aVisible, nsIWebShell *&aNewWebShell);
	NS_IMETHOD ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode);
	NS_IMETHOD CreatePopup( nsIDOMElement* aElement, nsIDOMElement* aPopupContent,
													PRInt32 aXPos, PRInt32 aYPos,
													const nsString& aPopupType, const nsString& anAnchorAlignment,
													const nsString& aPopupAlignment,
													nsIDOMWindowInternal* aWindow, nsIDOMWindowInternal** outPopup);
	NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);
	NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken);


private: // DATA
	int init();


	int										m_x, m_y, m_width, m_height; // webshell position and dimensions

	nsQtEventProcessor		*m_MozillaEventProcessor;    // local class

	nsIXlibWindowService  *m_WindowService;            // Mozilla class
	nsIPref               *m_Prefs;                    // Mozilla class
	nsIWebShell						*m_WebShell;                 // Mozilla class


};
#endif  // QMozillaContainer_h__

