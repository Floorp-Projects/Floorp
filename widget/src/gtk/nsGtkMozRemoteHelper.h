/*
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "nsString.h"
#include "nsIDOMWindowInternal.h"

#ifndef __nsGtkMozRemoteHelper_h__
#define __nsGtkMozRemoteHelper_h__

class nsGtkMozRemoteHelper
{
public:
  nsGtkMozRemoteHelper();
  virtual ~nsGtkMozRemoteHelper();

  // interaction from the outside world
  static        void SetupVersion         (GdkWindow *aWindow);
  static    gboolean HandlePropertyChange (GtkWidget *aWidget, GdkEventProperty *aEvent);

 private:

  // internal methods
  static       void  EnsureAtoms     (void);
  static       void  ParseCommand    (const char *aCommand, char **aResponse);
  static       void  FindLastInList  (nsCString &aString, nsCString &retString, PRUint32 *aIndexRet);
  static       char *BuildResponse   (const char *aError, const char *aMessage);

  // these are for the actions
  static NS_METHOD   OpenURLDialog   (void);
  static NS_METHOD   OpenURL         (const char *aURL, PRBool aNewWindow);
  static NS_METHOD   OpenFileDialog  (void);
  static NS_METHOD   OpenFile        (const char *aURL);
  static NS_METHOD   SaveAsDialog    (void);
  static NS_METHOD   SaveAs          (const char *aFileName, const char *aType);
  static NS_METHOD   MailTo          (const PRUnichar *aToList);
  static NS_METHOD   AddBookmark     (const char *aURL, const char *aTitle);

  // utility functions for getting windows
  static NS_METHOD   GetHiddenWindow      (nsIDOMWindowInternal **_retval);
  static NS_METHOD   GetLastBrowserWindow (nsIDOMWindowInternal **_retval);
  static NS_METHOD   OpenXULWindow        (const char *aChromeURL, nsIDOMWindowInternal *aParent,
					   const char *aWindowFeatures,
					   const PRUnichar *aName, const PRUnichar *aURL);
 
  static        Atom sMozVersionAtom;
  static        Atom sMozLockAtom;
  static        Atom sMozCommandAtom;
  static        Atom sMozResponseAtom;


};

#endif /* __nsGtkMozRemoteHelper_h__ */
