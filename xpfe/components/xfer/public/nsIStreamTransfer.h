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
#ifndef __nsIStreamTransfer_h
#define __nsIStreamTransfer_h

#include "nsIAppShellComponent.h"

class nsIURL;

/* a6cf90f0-15b3-11d2-932e-00805f8add32 */
#define NS_ISTREAMTRANSFER_IID \
    { 0xa6cf90f0, 0x15b3, 0x11d2, { 0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32 } }
#define NS_ISTREAMTRANSFER_PROGID NS_IAPPSHELLCOMPONENT_PROGID "/xfer"
#define NS_ISTREAMTRANSFER_CLASSNAME "Mozilla Stream Transfer Component"

/*----------------------------- nsIStreamTransfer ------------------------------
| This file describes Mozilla's general-purpose "stream transfer" component.   |
| This component is used to display a progress dialog while performing any of  |
| a variety of different "stream transfers."                                   |
|                                                                              |
| Basically, this component transfers data from an input stream to an          |
| output stream while displaying a progress dialog.  The component also        |
| offers some standard implementations of commonly-used input and output       |
| stream handlers (such as prompting for a destination file, writing the       |
| input stream to an output file, loading an input URL, etc.).                 |
|                                                                              |
| Select the appropriate transfer function depending on your requirements.     |
|                                                                              |
| Note that all methods are "asynchronous" in that they return before the      |
| stream transfer is completed.  The result generally indicates only whether   |
| or not the stream transfer was successfully initiated.                       |
------------------------------------------------------------------------------*/
struct nsIStreamTransfer : public nsIAppShellComponent {
    NS_DEFINE_STATIC_IID_ACCESSOR( NS_ISTREAMTRANSFER_IID )

    /*-------------------- SelectFileAndTransferLocation -----------------------
    | Prompt the user for a destination file and then transfer the data using  |
    | the argument URL as source to that file, while showing a progress        |
    | dialog.                                                                  |
    --------------------------------------------------------------------------*/
    NS_IMETHOD SelectFileAndTransferLocation( nsIURL *aURL ) = 0;

}; // nsIStreamTransfer

#define NS_DECL_ISTREAMTRANSFER \
    NS_IMETHOD SelectFileAndTransferLocation( nsIURL *aURL );
    
#endif
