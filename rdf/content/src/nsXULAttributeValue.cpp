/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Edward Kandrot <kandrot@netscape.com>
 */


#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsXULAttributeValue.h"

const PRUint32 nsXULAttributeValue::kMaxAtomValueLength = 12;


nsXULAttributeValue::nsXULAttributeValue()
{
    mValue = nsnull;
}


nsXULAttributeValue::~nsXULAttributeValue()
{
    if (mValue)
    {
        ReleaseValue();
    }
}



nsresult nsXULAttributeValue::GetValue( nsAWritableString& aResult )
{
    nsresult rv = NS_OK;
    if (! mValue) {
        aResult.Truncate();
    }
    else if (IsStringValue())
    {
        aResult.Assign((const PRUnichar*) mValue);
    }
    else
    {
        nsIAtom* atom = (nsIAtom*)(PRWord(mValue) & ~PRWord(kTypeMask));
        rv = atom->ToString(aResult);
    }
    return rv;
}


nsresult nsXULAttributeValue::SetValue(const nsAReadableString& aValue, PRBool forceAtom)
{   
    nsCOMPtr<nsIAtom> newAtom;
    
    // Atomize the value if it is short, or if it is the 'id'
    // attribute. We atomize the 'id' attribute to "prime" the global
    // atom table: the style system frequently asks for it, and if the
    // table is "unprimed" we see quite a bit of thrashing as the 'id'
    // value is repeatedly added and then removed from the atom table.
    if ((aValue.Length() <= kMaxAtomValueLength) || forceAtom)
    {
        newAtom = getter_AddRefs( NS_NewAtom(aValue) );
    }

        // Release the old value
    if (mValue)
        ReleaseValue();
    
    // ...and set the new value
    if (newAtom) {
        NS_ADDREF((nsIAtom*)newAtom.get());
        mValue = (void*)(PRWord(newAtom.get()) | kAtomType);
    }
    else {
        PRUnichar* str = ToNewUnicode(aValue);
        if (! str)
            return NS_ERROR_OUT_OF_MEMORY;
  
        mValue = str;
    }

    return NS_OK;
}


void nsXULAttributeValue::ReleaseValue( void ) {
    if (IsStringValue()) 
    {
        nsMemory::Free(mValue);
    }
    else 
    {
        nsIAtom* atom = (nsIAtom*)(PRWord(mValue) & ~PRWord(kTypeMask));
        NS_RELEASE(atom);
    }

    mValue = nsnull;
}

nsresult nsXULAttributeValue::GetValueAsAtom(nsIAtom** aResult)
{
    if (! mValue) {
        *aResult = nsnull;
    }
    else if (IsStringValue()) {
        *aResult = NS_NewAtom((const PRUnichar*) mValue);
    }
    else {
        *aResult = (nsIAtom*)(PRWord(mValue) & ~PRWord(kTypeMask));
        NS_ADDREF(*aResult);
    }
    return NS_OK;
}

