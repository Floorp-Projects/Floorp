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
#include "nsIAppShellComponentImpl.h"
#include "nsIFindComponent.h"
#include "nsISearchContext.h"

class nsITextServicesDocument;

// {4AA267A0-F81D-11d2-8067-00600811A9C3}
#define NS_FINDCOMPONENT_CID \
    { 0x4aa267a0, 0xf81d, 0x11d2, { 0x80, 0x67, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3} }

class nsFindComponent : public nsIFindComponent, public nsAppShellComponentImpl
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_FINDCOMPONENT_CID );

    // ctor/dtor
               nsFindComponent();
    virtual    ~nsFindComponent();

    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIAppShellComponent interface functions.
    NS_DECL_NSIAPPSHELLCOMPONENT

    // This class implements the nsIFindComponent interface functions.
    NS_DECL_NSIFINDCOMPONENT

    // "Context" for this implementation.
    class Context : public nsISearchContext
    {
    	public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSISEARCHCONTEXT
        
										Context();
				virtual 		~Context();
				NS_IMETHOD	Init( nsIWebShell *aWebShell,
				               nsIEditor* aEditor,
			                 const nsString& lastSearchString,
			                 const nsString& lastReplaceString,
			                 PRBool lastCaseSensitive,
			                 PRBool lastSearchBackwards,
			                 PRBool lastWrapSearch);

				NS_IMETHOD	Reset(nsIWebShell *aNewWebShell);
				NS_IMETHOD	DoFind(PRBool *aDidFind);
				NS_IMETHOD	DoReplace();

        // Utility to construct new TS document from our webshell.
        NS_IMETHOD  MakeTSDocument(nsIWebShell* aWebShell, nsITextServicesDocument** aDoc);
        NS_IMETHOD  GetCurrentBlockIndex(nsITextServicesDocument *aDoc, PRInt32 *outBlockIndex);
        NS_IMETHOD  SetupDocForSearch(nsITextServicesDocument *aDoc, PRInt32 *outBlockOffset);

        nsIWebShell* mTargetWebShell;			// weak link. Don't hold a reference
        nsIEditor*   mEditor;							// weak link. Don't hold a reference
        nsString     mSearchString;
        nsString     mReplaceString;
        PRBool       mCaseSensitive;
        PRBool       mSearchBackwards;
        PRBool       mWrapSearch;
        nsIDOMWindow *mFindDialog;

    }; // nsFindComponent::Context

protected:
    nsString                     mLastSearchString;
    nsString                     mLastReplaceString;
    PRBool                       mLastCaseSensitive;
    PRBool                       mLastSearchBackwards;
    PRBool                       mLastWrapSearch;
    nsInstanceCounter            mInstanceCounter;
}; // nsFindComponent
