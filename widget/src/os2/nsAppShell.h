/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nsAppShell_h
#define _nsAppShell_h

#include "nsWidgetDefs.h"
#include "nsIAppShell.h"

// Native PM Application shell wrapper.
class nsAppShell : public nsIAppShell
{
 public:
   nsAppShell();
   virtual ~nsAppShell();

   // nsISupports
   NS_DECL_ISUPPORTS

   // nsIAppShell
   NS_IMETHOD Create( int *argc, char **argv);
   NS_IMETHOD SetDispatchListener( nsDispatchListener *aDispatchListener);
   NS_IMETHOD Spinup() { return NS_OK; }
   NS_IMETHOD Run(); 
   NS_IMETHOD Spindown() { return NS_OK; }
   NS_IMETHOD PushThreadEventQueue();
   NS_IMETHOD PopThreadEventQueue();
   NS_IMETHOD Exit();

   NS_IMETHOD GetNativeEvent( PRBool &aRealEvent, void *&aEvent);
   NS_IMETHOD EventIsForModalWindow( PRBool aRealEvent, void *aEvent,
                                     nsIWidget *aWidget, PRBool *aForWindow);
   NS_IMETHOD DispatchNativeEvent( PRBool aRealEvent, void *aEvent);

   // return the HMQ for NS_NATIVE_SHELL, fwiw
   virtual void    *GetNativeData( PRUint32 aDataType);

  private:
   nsDispatchListener *mDispatchListener;
   HAB                 mHab;
   HMQ                 mHmq;
   BOOL                mQuitNow;
   QMSG                mQmsg;
};

// obtain an appropriate appshell object
extern "C" nsresult NS_CreateAppshell( nsIAppShell **aAppShell);

#endif
