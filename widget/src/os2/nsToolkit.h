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

#ifndef _toolkit_h      
#define _toolkit_h

#include "nsIToolkit.h"
#include "nsWidgetDefs.h"
#include "prmon.h"

// This is a wrapper around the 'pm thread' which runs the message queue.
// Need to do this to 1) ensure windows are always created/destroyed etc.
// in the same thread, 2) because there's no guarantee that a calling
// thread will have a message queue.
//
// In normal operation, a top level window will be created with a null
// toolkit.  This creates a new toolkit & init's with the current thread.
// There is an assumption that there is an nsAppShell for that thread,
// which becomes the 'PM thread'.  A null thread can be passed in, in
// which case a new thread is created & set to be the PM thread - this
// shouldn't really happen because there's no condition for thread#1 to
// exit!
//
// To allow non-pm threads to call pm functions, a slightly contorted
// mechanism is used.
//
// The object which wishes to do tasks in the pm thread (eg. nsWindow) must
// derive from the nsSwitchToPMThread class.
// At task-time, create an instance of MethodInfo and call the 'CallMethod'
// method in the widget's toolkit.  This will call back into the object's
// 'CallMethod' in the pm thread, and not return until it does.
//
// The good news is that you probably don't need to worry about this!
//
// What you may need to 'worry about' is the 'SendMsg' method.  When you
// want to call WinSendMsg & you're not certain you're in the PM thread,
// use this one, and the right thing will happen.

struct MethodInfo;

class nsToolkit : public nsIToolkit
{
 public:
   nsToolkit();
   virtual ~nsToolkit();

   NS_DECL_ISUPPORTS

   NS_IMETHOD Init( PRThread *aThread);

   nsresult CallMethod( MethodInfo *info);
   MRESULT  SendMsg( HWND hwnd, ULONG msg, MPARAM mp1 = 0, MPARAM mp2 = 0);

   // Return whether the current thread is the application's PM thread.  
   PRBool    IsPMThread()        { return (PRBool)(mPMThread == PR_GetCurrentThread());}
   PRThread *GetPMThread()       { return mPMThread; }
   HWND      GetDispatchWindow() { return mDispatchWnd; }

   void CreateInternalWindow( PRThread *aThread);

 private:
   void CreatePMThread();

 protected:
   // Handle of the window used to receive dispatch messages.
   HWND       mDispatchWnd;
   // Thread Id of the PM thread.
   PRThread  *mPMThread;
   // Monitor used to coordinate dispatch
   PRMonitor *mMonitor;
};

// Interface to derive things from
class nsSwitchToPMThread
{
 public:
   // No return code: if this is a problem then 
   virtual nsresult CallMethod( MethodInfo *info) = 0;
};

// Structure used for passing the information 
struct MethodInfo
{
   nsSwitchToPMThread *target;
   UINT                methodId;
   int                 nArgs;
   ULONG              *args;

   MethodInfo( nsSwitchToPMThread *obj, UINT id,
               int numArgs = 0, ULONG *arguments = 0)
   {
      target   = obj;
      methodId = id;
      nArgs    = numArgs;
      args     = arguments;
   }
};

#endif
