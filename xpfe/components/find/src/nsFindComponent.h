/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsIFindComponent.h"
#include "nsISearchContext.h"
#include "nsCOMPtr.h"
#include "nsIFindAndReplace.h"


/*

    Warning: this code is soon to become obsolete. The Find code has moved
    into mozilla/embedding/components/find, and is now shared between
    embedding apps and mozilla.
    
    This code remains because editor needs its find and replace functionality,
    for now.
    
    Simon Fraser sfraser@netscape.com

*/


class nsITextServicesDocument;

// {4AA267A0-F81D-11d2-8067-00600811A9C3}
#define NS_FINDCOMPONENT_CID \
    { 0x4aa267a0, 0xf81d, 0x11d2, { 0x80, 0x67, 0x0, 0x60, 0x8, 0x11, 0xa9, 0xc3} }

class nsFindComponent : public nsIFindComponent
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR( NS_FINDCOMPONENT_CID );

    // ctor/dtor
               nsFindComponent();
    virtual    ~nsFindComponent();

    // This class implements the nsISupports interface functions.
    NS_DECL_ISUPPORTS

    // This class implements the nsIFindComponent interface functions.
    NS_DECL_NSIFINDCOMPONENT

    // "Context" for this implementation.
    class Context : public nsISearchContext
    {
        public:
        NS_DECL_ISUPPORTS
        NS_DECL_NSISEARCHCONTEXT
        
        Context();
        virtual ~Context();
        NS_IMETHOD Init( nsIDOMWindowInternal *aWindow,
                         nsIEditorShell* aEditorShell,
                         const nsString& lastSearchString,
                         const nsString& lastReplaceString,
                         PRBool lastCaseSensitive,
                         PRBool lastSearchBackwards,
                         PRBool lastWrapSearch);

         NS_IMETHOD Reset(nsIDOMWindowInternal *aNewWindow);
         NS_IMETHOD DoFind(PRBool *aDidFind);
         NS_IMETHOD DoReplace(PRBool aAllOccurrences, PRBool *aDidFind);

        // Utility to construct new TS document from our webshell.
        NS_IMETHOD  MakeTSDocument(nsIDOMWindowInternal* aWindow, nsITextServicesDocument** aDoc);

        nsIDOMWindowInternal*     mTargetWindow; // weak link. Don't hold a reference
        nsIEditorShell*   mEditorShell;          // weak link. Don't hold a reference
        nsCOMPtr<nsIFindAndReplace> mTSFind;
        nsString       mSearchString;
        nsString       mReplaceString;
        PRBool         mCaseSensitive;
        PRBool         mSearchBackwards;
        PRBool         mWrapSearch;
        nsIDOMWindowInternal*  mFindDialog;
        nsIDOMWindowInternal*  mReplaceDialog;

    }; // nsFindComponent::Context

protected:
    nsString                     mLastSearchString;
    nsString                     mLastReplaceString;
    PRBool                       mLastCaseSensitive;
    PRBool                       mLastSearchBackwards;
    PRBool                       mLastWrapSearch;
}; // nsFindComponent
