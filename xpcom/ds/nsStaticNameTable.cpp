/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/* Class to manage lookup of static names in a table. */

#include "nscore.h"
#include "nsString.h"
#include "nsStaticNameTable.h"

nsStaticCaseInsensitiveNameTable::nsStaticCaseInsensitiveNameTable()
    : mNameArray(nsnull), mNameTable(nsnull), mCount(0)
{
    MOZ_COUNT_CTOR(nsStaticCaseInsensitiveNameTable);
}  

nsStaticCaseInsensitiveNameTable::~nsStaticCaseInsensitiveNameTable()
{
    delete [] mNameArray;
    delete mNameTable;
    MOZ_COUNT_DTOR(nsStaticCaseInsensitiveNameTable);
}  

PRBool 
nsStaticCaseInsensitiveNameTable::Init(const char* Names[], PRInt32 Count)
{
    NS_ASSERTION(!mNameArray, "double Init");  
    NS_ASSERTION(!mNameTable, "double Init");  
    NS_ASSERTION(Names, "null name table");
    NS_ASSERTION(Count, "0 count");

    mCount = Count;
    mNameArray = new nsCString[Count];
    // XXX best bucket count heuristic?
    mNameTable = new nsHashtable(Count<16 ? Count : Count<128 ? Count/4 : 128);
    if (!mNameArray || !mNameTable) {
        return PR_FALSE;
    }

    for (PRInt32 index = 0; index < Count; ++index) {
        char*    raw = (char*) Names[index];
        PRUint32 len = nsCRT::strlen(raw);
        nsStr*   str = NS_STATIC_CAST(nsStr*, &mNameArray[index]);
#ifdef DEBUG
       {
       // verify invarients of contents
       nsCAutoString temp1(raw);
       nsCAutoString temp2(raw);
       temp1.ToLowerCase();
       NS_ASSERTION(temp1.Equals(temp2), "upper case char in table");
       }
#endif      
        nsStr::Initialize(*str, raw, len, len, eOneByte, PR_FALSE);
        nsCStringKey key(raw, len, nsCStringKey::NEVER_OWN);
        mNameTable->Put(&key, (void*)(index+1)); // to make 0 != nsnull
    }
    return PR_TRUE;
}  

inline PRInt32
LookupLowercasedKeyword(const nsCString& aLowercasedKeyword, 
                        nsHashtable* aTable)
{
    nsCStringKey key(aLowercasedKeyword);
    void* val = aTable->Get(&key);
    return val ? NS_PTR_TO_INT32(val) - 1 : 
                 nsStaticCaseInsensitiveNameTable::NOT_FOUND;
}  

PRInt32
nsStaticCaseInsensitiveNameTable::Lookup(const nsACString& aName)
{
    NS_ASSERTION(mNameArray, "not inited");  
    NS_ASSERTION(mNameTable, "not inited");  
    NS_ASSERTION(mCount,     "not inited");

    nsCAutoString strLower(aName);
    strLower.ToLowerCase();
    return LookupLowercasedKeyword(strLower, mNameTable);
}  

PRInt32
nsStaticCaseInsensitiveNameTable::Lookup(const nsAString& aName)
{
    NS_ASSERTION(mNameArray, "not inited");  
    NS_ASSERTION(mNameTable, "not inited");  
    NS_ASSERTION(mCount,     "not inited");
   
    nsCAutoString strLower;
    strLower.AssignWithConversion(aName);
    strLower.ToLowerCase();
    return LookupLowercasedKeyword(strLower, mNameTable);
}  

const nsCString& 
nsStaticCaseInsensitiveNameTable::GetStringValue(PRInt32 index)
{
    NS_ASSERTION(mNameArray, "not inited");  
    NS_ASSERTION(mNameTable, "not inited");  
    NS_ASSERTION(mCount,     "not inited");
    
    if ((NOT_FOUND < index) && (index < mCount)) {
        return mNameArray[index];
    } else {
        return mNullStr;
    }
}  

