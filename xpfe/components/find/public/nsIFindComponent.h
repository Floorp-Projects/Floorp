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
#ifndef __nsIFindComponent_h
#define __nsIFindComponent_h

#include "nsIAppShellComponent.h"

class nsIDocument;

// a6cf90ee-15b3-11d2-932e-00805f8add32
#define NS_IFINDCOMPONENT_IID \
    { 0xa6cf90ee, 0x15b3, 0x11d2, {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} };
#define NS_IFINDCOMPONENT_PROGID NS_IAPPSHELLCOMPONENT_PROGID "/find"
#define NS_IFINDCOMPONENT_CLASSNAME "Mozilla Find Component"

/*----------------------------- nsIFindComponent -------------------------------
| This file describes the interface for Mozilla's pluggable "find" component   |
| that provides the implementation of the browser's "find in document"         |
| function (and perhaps will be generalized to support similar searching in    |
| other contexts).                                                             |
|                                                                              |
| The component itself is a singleton (one per executing application).  To     |
| handle searching on a per-document basis, this interface supplies a          |
| CreateContext() function that creates a search context (generally, on a      |
| per document, and thus per-browser-window, basis).                           |
|                                                                              |
| The component itself will "remember" the last string searched (via any       |
| context).  This way, a search in a new context (browser window) will be      |
| able to search for the same string (by default).                             |
|                                                                              |
| Clients (e.g., the browser window object) will generally use this interface  |
| in this manner:                                                              |
|   1. Hold a reference to the singleton "find component".                     |
|   2. On initial search, ask that component to create a search "context."     |
|   3. Reset the context whenever the underlying document changes.             |
|   4. Forward "find" and "find next" requests to the component, along         |
|      with the appropriate search context object.                             |
|   5. Release() the search context object and the find component when the     |
|      browser window closes.                                                  |
------------------------------------------------------------------------------*/
struct nsIFindComponent : public nsISupports {
    NS_DEFINE_STATIC_IID_ACCESSOR( NS_IFINDCOMPONENT_IID )

    /*---------------------------- CreateContext -------------------------------
    | Create a "search context" for the given document.  Subsequent Find and   |
    | FindNext requests that provide the returned search context will find     |
    | the appropriate search string in aDocument.                              |
    |                                                                          |
    | The result is of the xpcom equivalent of an opaque type.  It's true type |
    | is defined by the implementation of this interface.  Clients ought never |
    | have to do QueryInterface to convert this to something more elaborate.   |
    | Clients do have to call Release() when they're no longer interested in   |
    | this search context.                                                     |
    --------------------------------------------------------------------------*/
    NS_IMETHOD CreateContext( nsIDocument *aDocument,
                              nsISupports **aResult ) = 0;

    /*--------------------------------- Find -----------------------------------
    | Finds the "first" occurrence of a string in the given search context     |
    | (i.e., document).                                                        |
    |                                                                          |
    | Please note that you don't provide the string to search for!             |
    |                                                                          |
    | This might seem odd, but that's the way it's designed.  Prompting the    |
    | user for the string (and for various search options such as "ignore      |
    | case" and "search backward") is up to the implementation of this         |
    | component.                                                               |
    --------------------------------------------------------------------------*/
    NS_IMETHOD Find( nsISupports *aContext ) = 0;

    /*------------------------------- FindNext ---------------------------------
    | Finds the next occurrence (of the previously searched for string) in     |
    | the given search context (document).                                     |
    |                                                                          |
    | If no previous Find has been performed with this context, then the       |
    | find component will use the last find performed for any context.         |
    --------------------------------------------------------------------------*/
    NS_IMETHOD FindNext( nsISupports *aContext ) = 0;

    /*----------------------------- ResetContext -------------------------------
    | Reset the given search context to search a new document.  This is        |
    | intended to be called when the user goes to a new page.                  |
    --------------------------------------------------------------------------*/
    NS_IMETHOD ResetContext( nsISupports *aContext,
                             nsIDocument *aDocument ) = 0;

}; // nsIFindComponent

#define NS_DECL_IFINDCOMPONENT \
    NS_IMETHOD CreateContext( nsIDocument *aDocument,  \
                              nsISupports **aResult ); \
    NS_IMETHOD Find( nsISupports *aContext );          \
    NS_IMETHOD FindNext( nsISupports *aContext );      \
    NS_IMETHOD ResetContext( nsISupports *aContext,    \
                             nsIDocument *aDocument );

#endif
