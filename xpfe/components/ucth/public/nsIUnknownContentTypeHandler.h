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
#ifndef __nsIUnknownContentTypeHandler_h
#define __nsIUnknownContentTypeHandler_h

#include "nsIAppShellComponent.h"

class nsIURL;
class nsIDocumentLoader;

// a6cf90ef-15b3-11d2-932e-00805f8add32
#define NS_IUNKNOWNCONTENTTYPEHANDLER_IID \
    { 0xa6cf90ef, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }
#define NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID NS_IAPPSHELLCOMPONENT_PROGID "/unknownContentType"
#define NS_IUNKNOWNCONTENTTYPEHANDLER_CLASSNAME "Mozilla Unknown Content-Type Handler Component"

/*----------------------- nsIUnknownContentTypeHandler -------------------------
| This file describes the interface for Mozilla's "unknown content-type        |
| handler" component.  This component is notified whenever the browser         |
| encounters data of some unknown content (mime) type.                         |
|                                                                              |
| This component provides one component-specific member function:              |
| HandleUnknownContentType.  This method's areguments include:                 |
|   o the URL at which the content resides                                     |
|   o the content-type                                                         |
|   o the window that encountered the unknown content.                         |
|                                                                              |
| The expected response by implementors of this interface is to display a      |
| dialog prompting the user for direction on how to handle the content         |
| (e.g., download it, pick an application to use to view it, etc.) and then    |
| carry out that request (perhaps with assistance from other components).      |
------------------------------------------------------------------------------*/
struct nsIUnknownContentTypeHandler : public nsIAppShellComponent {
    NS_DEFINE_STATIC_IID_ACCESSOR( NS_IUNKNOWNCONTENTTYPEHANDLER_IID )

    NS_IMETHOD HandleUnknownContentType( nsIURL *aURL,
                                         const char *aContentType,
                                         nsIDocumentLoader *aDocLoader ) = 0;
}; // nsIUnknownContentTypeHandler

#define NS_DECL_IUNKNOWNCONTENTTYPEHANDLER                              \
    NS_IMETHOD HandleUnknownContentType( nsIURL *aURL,                    \
                                         const char *aContentType,        \
                                         nsIDocumentLoader *aDocLoader );
    
#endif
