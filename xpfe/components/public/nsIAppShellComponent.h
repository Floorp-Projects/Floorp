/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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
#ifndef __nsIAppShellComponent_h
#define __nsIAppShellComponent_h

#include "nsISupports.h"

class nsIAppShellService;
class nsICmdLineService;

// a6cf90ed-15b3-11d2-932e-00805f8add32
#define NS_IAPPSHELLCOMPONENT_IID \
    { 0xa6cf90ed, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} };
#define NS_IAPPSHELLCOMPONENT_PROGID "component://netscape/appshell/component"
#define NS_IAPPSHELLCOMPONENT_CLASSNAME "Mozilla AppShell Component"

/*--------------------------- nsIAppShellComponent -----------------------------
| This interface describes the fundamental communications between the          |
| application shell and its dynamically installable "components."  Components  |
| are self-contained bits of browser client application functionality.  They   |
| range in size from the general-purpose "Find dialog" up to the "browser."    |
|                                                                              |
| The interface has three main purposes:                                       |
|   o It provides a means for the app shell to identify and load components    |
|     dynamically.                                                             |
|   o It enables those components to contribute to the browser client          |
|     application User Interface (e.g., by adding menu options, etc.).         |
|   o It gives the app shell (and other components) a way to broadcast         |
|     interesting events to other components.                                  |
|                                                                              |
| [Currently, only the first purpose is exploited.]                            |
|                                                                              |
| All app shell components must:                                               |
|   1. Implement this interface.                                               |
|   2. Publish and implement additional interfaces, as necessary, to           |
|      support component-specific function.  For example, the "find            |
|      component" also implements the nsIFindComponent interface which         |
|      adds functions specific to that component.                              |
|   3. Register (using the Mozilla XPCOM component registration facilities)    |
|      using a ProgID of the form:                                             |
|       component://netscape/appshell/component/foo                            |
|      where "foo" is a component-specific identifier (which should match the  |
|      component-specific interface name).                                     |
|                                                                              |
| The app shell will instantiate each such component at application start-up   |
| and invoke the components Initialize() member function, passing a pointer    |
| to the app shell and to the command-line service corresponding to the        |
| command-line arguments used to start the app shell.                          |
|                                                                              |
| The component should implement this function as necessary.  Typically, a     |
| component will register interest in an appropriate set of events (e.g.,      |
| browser windows opening).  Some components, such as the browser, will        |
| examine the command line argements and open windows.                         |
|                                                                              |
| Upon return, the app shell will maintain a reference to the component and    |
| notify it when events of interest occur.                                     |
------------------------------------------------------------------------------*/
struct nsIAppShellComponent : public nsISupports {
    NS_DEFINE_STATIC_IID_ACCESSOR( NS_IAPPSHELLCOMPONENT_IID )

    /*------------------------------ Initialize --------------------------------
    | Called at application startup.                                           |
    --------------------------------------------------------------------------*/
    NS_IMETHOD Initialize( nsIAppShellService *appShell,
                           nsICmdLineService *args ) = 0;

    /*------------------------- HandleAppShellEvent ----------------------------
    | This function is called (by the app shell, when its                      |
    | BroadcastAppShellEvent member function is called) to broadcast           |
    | application shell events that might be of interest to other components.  |
    |                                                                          |
    | aComponent is the ProgID of the component that generated the event.      |
    |                                                                          |
    | anEvent is an event code (defined by that component; should be a         |
    | constant declared in that component's interface declaration).            |
    |                                                                          |
    | eventData is an arbitrary interface pointer that is component-defined    |
    | event data.  Implementors of this function must QueryInterface to        |
    | convert this interface pointer to the component/event specific           |
    | interface.                                                               |
    --------------------------------------------------------------------------*/
    //NS_IMETHOD HandleAppShellEvent( const char  *aComponent,
    //                                int          anEvent,
    //                                nsISupports *eventData );
}; // nsIAppShellComponent

#define NS_DECL_IAPPSHELLCOMPONENT \
    NS_IMETHOD Initialize( nsIAppShellService *appShell, \
                           nsICmdLineService *args );

#endif
