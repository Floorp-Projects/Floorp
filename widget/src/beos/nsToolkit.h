/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef TOOLKIT_H      
#define TOOLKIT_H

#include "nsdefs.h"
#include "nsIToolkit.h"

#include <OS.h>

struct MethodInfo;

/**
 * Wrapper around the thread running the message pump.
 * The toolkit abstraction is necessary because the message pump must
 * execute within the same thread that created the widget under Win32.
 */ 

class nsToolkit : public nsIToolkit
{
public:
            NS_DECL_ISUPPORTS

                            nsToolkit();
            NS_IMETHOD      Init(PRThread *aThread);
            void            CallMethod(MethodInfo *info);
			void			CallMethodAsync(MethodInfo *info);
            // Return whether the current thread is the application's Gui thread.  
            PRBool          IsGuiThread(void)      { return (PRBool)(mGuiThread == PR_GetCurrentThread());}
            PRThread*       GetGuiThread(void)       { return mGuiThread;   }
			void			Kill();

private:
            virtual         ~nsToolkit();
            void            CreateUIThread(void);

protected:
    // Thread Id of the "main" Gui thread.
    PRThread    *mGuiThread;
	static void	RunPump(void* arg);
	void		GetInterface();
	bool		cached;
	bool		localthread;
	port_id		eventport;
	sem_id		syncsem;
};

#define WM_CALLMETHOD   'CAme'

class  nsWindow;

#endif  // TOOLKIT_H
