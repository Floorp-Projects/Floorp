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
#ifndef __nsISampleAppShellComponent_h
#define __nsISampleAppShellComponent_h

#include "nsIAppShellComponent.h"

/* a6cf90fe-15b3-11d2-932e-00805f8add32 */
#define NS_ISAMPLEAPPSHELLCOMPONENT_IID \
    { 0xa6cf90fe, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }
#define NS_ISAMPLEAPPSHELLCOMPONENT_PROGID NS_IAPPSHELLCOMPONENT_PROGID "/sample"
#define NS_ISAMPLEAPPSHELLCOMPONENT_CLASSNAME "Mozilla Sample App Shell Component"

/*------------------------ nsISampleAppShellComponent --------------------------
| This file describes the interface for a sample "application shell            |
| component."                                                                  |
|                                                                              |
| The sample component demonstrates most of what you need to know to           |
| implement your own appshell components.  In addition, it provides some       |
| facilities for testing XUL dialogs.                                          |
|                                                                              |
| You don't need to put those in your component!                               |
------------------------------------------------------------------------------*/
struct nsISampleAppShellComponent : public nsIAppShellComponent {
    NS_DEFINE_STATIC_IID_ACCESSOR( NS_ISAMPLEAPPSHELLCOMPONENT_IID )

    /*---------------------------- DoDialogTests -------------------------------
    | Displays a dialog from which you can click buttons to perform these      |
    | dialogs tests:                                                           |
    |   o Non-modal dialog from C++                                            |
    |   o Non-modal dialog from JavaScript                                     |
    |   o Modal dialog from C++                                                |
    |   o Modal dialog from JavaScript                                         |
    --------------------------------------------------------------------------*/
    NS_IMETHOD DoDialogTests() = 0;
}; // nsISampleAppShellComponent

#define NS_DECL_ISAMPLEAPPSHELLCOMPONENT \
    NS_IMETHOD DoDialogTests();
    
#endif
