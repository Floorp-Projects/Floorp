/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */


/*

  A set of helper classes used to implement attributes.

*/

#ifndef nsXULAttributes_h__
#define nsXULAttributes_h__

#include "nsIDOMAttr.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIScriptObjectOwner.h"
#include "nsIStyleRule.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsVoidArray.h"

class nsIURI;

//----------------------------------------------------------------------

class nsClassList {
public:
    nsClassList(nsIAtom* aAtom)
        : mAtom(aAtom), mNext(nsnull)
    {
        MOZ_COUNT_CTOR(nsClassList);
    }


    nsClassList(const nsClassList& aCopy)
        : mAtom(aCopy.mAtom), mNext(nsnull)
    {
        MOZ_COUNT_CTOR(nsClassList);
        if (aCopy.mNext) mNext = new nsClassList(*(aCopy.mNext));
    }

    nsClassList& operator=(const nsClassList& aClassList)
    {
        if (this != &aClassList) {
            delete mNext;
            mNext = nsnull;
            mAtom = aClassList.mAtom;

            if (aClassList.mNext) {
                mNext = new nsClassList(*(aClassList.mNext));
            }
        }
        return *this;
    }

    ~nsClassList(void)
    {
        MOZ_COUNT_DTOR(nsClassList);
        delete mNext;
    }

    nsCOMPtr<nsIAtom> mAtom;
    nsClassList*      mNext;

    static PRBool
    HasClass(nsClassList* aList, nsIAtom* aClass);

    static nsresult
    GetClasses(nsClassList* aList, nsVoidArray& aArray);

    static nsresult
    ParseClasses(nsClassList** aList, const nsString& aValue);
};

////////////////////////////////////////////////////////////////////////

class nsXULAttribute : public nsIDOMAttr,
                       public nsIScriptObjectOwner
{
protected:
    nsXULAttribute(nsIContent* aContent,
                   PRInt32 aNameSpaceID,
                   nsIAtom* aName,
                   const nsString& aValue);

    virtual ~nsXULAttribute();

public:
    static nsresult
    Create(nsIContent* aContent,
           PRInt32 aNameSpaceID,
           nsIAtom* aName,
           const nsString& aValue,
           nsXULAttribute** aResult);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMNode interface
    NS_DECL_IDOMNODE

    // nsIDOMAttr interface
    NS_DECL_IDOMATTR

    // nsIScriptObjectOwner interface
    NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // Implementation methods
    void GetQualifiedName(nsString& aAttributeName);

    PRInt32 GetNameSpaceID() const { return mNameSpaceID; }
    nsIAtom* GetName() const { return mName; }
    void SetValueInternal(const nsString& aValue) { mValue = aValue; }

protected:
    PRInt32     mNameSpaceID;
    nsIAtom*    mName;
    nsString    mValue;
    nsIContent* mContent;
    void*       mScriptObject;
};


////////////////////////////////////////////////////////////////////////

class nsXULAttributes : public nsIDOMNamedNodeMap,
                        public nsIScriptObjectOwner
{
public:
    static nsresult
    Create(nsIContent* aElement, nsXULAttributes** aResult);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIDOMNamedNodeMap interface
    NS_DECL_IDOMNAMEDNODEMAP

    // nsIScriptObjectOwner interface
    NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
    NS_IMETHOD SetScriptObject(void *aScriptObject);

    // Implementation methods
    // VoidArray Helpers
    PRInt32 Count() { return mAttributes.Count(); };
    nsXULAttribute* ElementAt(PRInt32 i) { return (nsXULAttribute*)mAttributes.ElementAt(i); };
    void AppendElement(nsXULAttribute* aElement) { mAttributes.AppendElement((void*)aElement); };
    void RemoveElementAt(PRInt32 aIndex) { mAttributes.RemoveElementAt(aIndex); };

    // Style Helpers
    nsresult GetClasses(nsVoidArray& aArray) const;
    nsresult HasClass(nsIAtom* aClass) const;

    nsresult SetClassList(nsClassList* aClassList);
    nsresult UpdateClassList(const nsString& aValue);
    nsresult UpdateStyleRule(nsIURI* aDocURL, const nsString& aValue);

    nsresult SetInlineStyleRule(nsIStyleRule* aRule);
    nsresult GetInlineStyleRule(nsIStyleRule*& aRule);

protected:
    nsXULAttributes(nsIContent* aContent);
    virtual ~nsXULAttributes();

    nsIContent*            mContent;
    nsClassList*           mClassList;
    nsCOMPtr<nsIStyleRule> mStyleRule;
    nsVoidArray            mAttributes;
    void*                  mScriptObject;
};


#endif // nsXULAttributes_h__

