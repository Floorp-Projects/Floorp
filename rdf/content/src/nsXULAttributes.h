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

class nsIURL;

struct nsClassList {
  nsClassList(nsIAtom* aAtom)
    : mAtom(aAtom),
      mNext(nsnull)
  {
  }
  nsClassList(const nsClassList& aCopy)
    : mAtom(aCopy.mAtom),
      mNext(nsnull)
  {
    NS_ADDREF(mAtom);
    if (nsnull != aCopy.mNext) {
      mNext = new nsClassList(*(aCopy.mNext));
    }
  }
  ~nsClassList(void)
  {
    NS_RELEASE(mAtom);
    if (nsnull != mNext) {
      delete mNext;
    }
  }

  nsIAtom*      mAtom;
  nsClassList*  mNext;
};

////////////////////////////////////////////////////////////////////////

class nsXULAttribute : public nsIDOMAttr,
                       public nsIScriptObjectOwner
{
private:
    nsXULAttribute(nsIContent* aContent,
                   PRInt32 aNameSpaceID,
                   nsIAtom* aName,
                   const nsString& aValue);

    ~nsXULAttribute();

    friend nsresult
    NS_NewXULAttribute(nsXULAttribute** aResult,
                       nsIContent* aContent,
                       PRInt32 aNameSpaceID,
                       nsIAtom* aName,
                       const nsString& aValue);

public:

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

    PRInt32 GetNameSpaceID();
    nsIAtom* GetName();
    const nsString& GetValue();

    // Publicly exposed to make life easier. This is a private class
    // anyway.
    PRInt32     mNameSpaceID;
    nsIAtom*    mName;
    nsString    mValue;

private:
    nsIContent* mContent;
    void*       mScriptObject;
};

nsresult
NS_NewXULAttribute(nsXULAttribute** aResult,
                   nsIContent* aContent,
                   PRInt32 aNameSpaceID,
                   nsIAtom* aName,
                   const nsString& aValue);

////////////////////////////////////////////////////////////////////////

class nsXULAttributes : public nsIDOMNamedNodeMap,
                        public nsIScriptObjectOwner
{
public:
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
    void RemoveElementAt(PRInt32 index) { mAttributes.RemoveElementAt(index); };

    // Style Helpers
    nsresult GetClasses(nsVoidArray& aArray) const;
    nsresult HasClass(nsIAtom* aClass) const;

    nsresult UpdateClassList(const nsString& aValue);
    nsresult UpdateStyleRule(nsIURL* aDocURL, const nsString& aValue);
    nsresult GetInlineStyleRule(nsIStyleRule*& aRule);

private:
    friend nsresult
    NS_NewXULAttributes(nsXULAttributes** aResult, nsIContent* aContent);

    nsXULAttributes(nsIContent* aContent);
    virtual ~nsXULAttributes();

    static void
    ParseClasses(const nsString& aClassString, nsClassList** aClassList);

    nsIContent*   mContent;
    nsClassList*  mClassList;
    nsIStyleRule* mStyleRule;
    nsVoidArray   mAttributes;
    void*         mScriptObject;
};


nsresult
NS_NewXULAttributes(nsXULAttributes** aResult, nsIContent* aContent);

#endif // nsXULAttributes_h__

