/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#define NS_IMPL_IDS

#include "nsPersistentProperties.h"
#include "nsID.h"
#include "nsCRT.h"
#include "nsIInputStream.h"
#include "nsIProperties.h"
#include "nsIUnicharInputStream.h"
#include "nsProperties.h"
#include "pratom.h"
#include "nsEnumeratorUtils.h"

static PLHashNumber
HashKey(const PRUnichar *aString)
{
  return (PLHashNumber) nsCRT::HashCode(aString);
}

static PRIntn
CompareKeys(const PRUnichar *aStr1, const PRUnichar *aStr2)
{
  return nsCRT::strcmp(aStr1, aStr2) == 0;
}

nsPersistentProperties::nsPersistentProperties()
{
  NS_INIT_REFCNT();

  mIn = nsnull;
  mSubclass = NS_STATIC_CAST(nsIPersistentProperties*, this);
  mTable = PL_NewHashTable(128, (PLHashFunction) HashKey,
    (PLHashComparator) CompareKeys,
    (PLHashComparator) nsnull, nsnull, nsnull);
}

PR_STATIC_CALLBACK(PRIntn)
FreeHashEntries(PLHashEntry* he, PRIntn i, void* arg)
{
  nsCRT::free((PRUnichar*)he->key);
  nsCRT::free((PRUnichar*)he->value);
  return HT_ENUMERATE_REMOVE;
}

nsPersistentProperties::~nsPersistentProperties()
{
  if (mTable) {
    // Free the PRUnicode* pointers contained in the hash table entries
    PL_HashTableEnumerateEntries(mTable, FreeHashEntries, 0);
    PL_HashTableDestroy(mTable);
    mTable = nsnull;
  }
}

NS_METHOD
nsPersistentProperties::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsPersistentProperties* props = new nsPersistentProperties();
    if (props == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(props);
    nsresult rv = props->QueryInterface(aIID, aResult);
    NS_RELEASE(props);
    return rv;
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsPersistentProperties, nsIPersistentProperties, nsIProperties)

NS_IMETHODIMP
nsPersistentProperties::Load(nsIInputStream *aIn)
{
  PRInt32  c;
  nsresult ret = NS_ERROR_FAILURE;

  nsAutoString uesc;
  uesc.AssignWithConversion("UTF-8");

#ifndef XPCOM_STANDALONE
  ret = NS_NewConverterStream(&mIn, nsnull, aIn, 0, &uesc);
#endif /* XPCOM_STANDALONE */
  if (ret != NS_OK) {
#ifdef NS_DEBUG
      printf("NS_NewConverterStream failed\n");
#endif
    return NS_ERROR_FAILURE;
  }
  c = Read();
  while (1) {
    c = SkipWhiteSpace(c);
    if (c < 0) {
      break;
    }
    else if ((c == '#') || (c == '!')) {
      c = SkipLine(c);
      continue;
    }
    else {
      nsAutoString key;
      while ((c >= 0) && (c != '=') && (c != ':')) {
        key.Append((PRUnichar) c);
        c = Read();
      }
      if (c < 0) {
        break;
      }
      char *trimThese = " \t";
      key.Trim(trimThese, PR_FALSE, PR_TRUE);
      c = Read();
      nsAutoString value;
      PRUint32 state  = 0;
      PRUnichar uchar = 0;
      while ((c >= 0) && (c != '\r') && (c != '\n')) {
        switch(state) {
          case 0:
           if (c == '\\') {
             c = Read();
             switch(c) {
               case '\r':
               case '\n':
                 c = SkipWhiteSpace(c);
                 value.Append((PRUnichar) c);
                 break;
               case 'u':
               case 'U':
                 state = 1;
                 uchar=0;
                 break;
               case 't':
                 value.AppendWithConversion('\t');
                 break;
               case 'n':
                 value.AppendWithConversion('\n');
                 break;
               case 'r':
                 value.AppendWithConversion('\r');
                 break;
               default:
                 value.Append((PRUnichar) c);
             } // switch(c)
           } else {
             value.Append((PRUnichar) c);
           }
           c = Read();
           break;
         case 1:
         case 2:
         case 3:
         case 4:
           if(('0' <= c) && (c <= '9')) {
              uchar = (uchar << 4) | (c - '0');
              state++;
              c = Read();
           } else if(('a' <= c) && (c <= 'f')) {
              uchar = (uchar << 4) | (c - 'a' + 0x0a);
              state++;
              c = Read();
           } else if(('A' <= c) && (c <= 'F')) {
              uchar = (uchar << 4) | (c - 'A' + 0x0a);
              state++;
              c = Read();
           } else {
             value.Append((PRUnichar) uchar);
             state = 0;
           }
           break;
         case 5:
           value.Append((PRUnichar) uchar);
           state = 0;
        }
      }
      if(state != 0) {
        value.Append((PRUnichar) uchar);
        state = 0;
      }

      value.Trim(trimThese, PR_TRUE, PR_TRUE);
      nsAutoString oldValue;
      mSubclass->SetStringProperty(key, value, oldValue);
    }
  }
  mIn->Close();
  NS_RELEASE(mIn);
  NS_ASSERTION(!mIn, "unexpected remaining reference");

  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::SetStringProperty(const nsString& aKey, nsString& aNewValue,
  nsString& aOldValue)
{
  // XXX The ToNewCString() calls allocate memory using "new" so this code
  // causes a memory leak...
#if 0
  cout << "will add " << aKey.ToNewCString() << "=" << aNewValue.ToNewCString() << endl;
#endif
  if (!mTable) {
    return NS_ERROR_FAILURE;
  }

  const PRUnichar *key = aKey.get();  // returns internal pointer (not a copy)
  PRUint32 len;
  PRUint32 hashValue = nsCRT::HashCode(key, &len);
  PLHashEntry **hep = PL_HashTableRawLookup(mTable, hashValue, key);
  PLHashEntry *he = *hep;
  if (he) {
    // XXX should we copy the old value to aOldValue, and then remove it?
#ifdef NS_DEBUG
    char buf[128];
    aKey.ToCString(buf, sizeof(buf));
    printf("warning: property %s already exists\n", buf);
#endif
    return NS_OK;
  }
  PL_HashTableRawAdd(mTable, hep, hashValue, aKey.ToNewUnicode(),
                     aNewValue.ToNewUnicode());

  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::Save(nsIOutputStream* aOut, const nsString& aHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPersistentProperties::Subclass(nsIPersistentProperties* aSubclass)
{
  if (aSubclass) {
    mSubclass = aSubclass;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::GetStringProperty(const nsString& aKey, nsString& aValue)
{
  if (!mTable)
     return NS_ERROR_FAILURE;

  const PRUnichar *key = aKey.get();

  PRUint32 len;
  PRUint32 hashValue = nsCRT::HashCode(key, &len);
  PLHashEntry **hep = PL_HashTableRawLookup(mTable, hashValue, key);
  PLHashEntry *he = *hep;
  if (he) {
    aValue = (const PRUnichar*)he->value;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

PR_STATIC_CALLBACK(PRIntn)
AddElemToArray(PLHashEntry* he, PRIntn i, void* arg)
{
  nsISupportsArray	*propArray = (nsISupportsArray *) arg;

  nsString* keyStr = new nsString((PRUnichar*) he->key);
  nsString* valueStr = new nsString((PRUnichar*) he->value);

  nsPropertyElement *element = new nsPropertyElement();
  if (!element)
     return HT_ENUMERATE_STOP;

  NS_ADDREF(element);
  element->SetKey(keyStr);
  element->SetValue(valueStr);
  propArray->InsertElementAt(element, i);

  return HT_ENUMERATE_NEXT;
}

NS_IMETHODIMP
nsPersistentProperties::EnumerateProperties(nsIBidirectionalEnumerator** aResult)
{
  if (!mTable)
    return NS_ERROR_FAILURE;

  nsISupportsArray* propArray;
  nsresult rv = NS_NewISupportsArray(&propArray);
  if (rv != NS_OK)
    return rv;

  // Step through hash entries populating a transient array
   PRIntn n = PL_HashTableEnumerateEntries(mTable, AddElemToArray, (void *)propArray);
   if ( n < (PRIntn) mTable->nentries )
      return NS_ERROR_OUT_OF_MEMORY;

  // Convert array into enumerator
  rv = NS_NewISupportsArrayEnumerator(propArray, aResult);
  if (rv != NS_OK)
    return rv;

  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::SimpleEnumerateProperties(nsISimpleEnumerator** aResult)
{
  nsCOMPtr<nsIBidirectionalEnumerator> iterator;

  if (!mTable)
    return NS_ERROR_FAILURE;

  nsISupportsArray* propArray;
  nsresult rv = NS_NewISupportsArray(&propArray);
  if (rv != NS_OK)
    return rv;

  // Step through hash entries populating a transient array
   PRIntn n = PL_HashTableEnumerateEntries(mTable, AddElemToArray, (void *)propArray);
   if ( n < (PRIntn) mTable->nentries )
      return NS_ERROR_OUT_OF_MEMORY;

  // Convert array into enumerator
  rv = NS_NewISupportsArrayEnumerator(propArray, getter_AddRefs(iterator));
  // Convert nsIEnumerator into nsISimpleEnumerator
  rv = NS_NewAdapterEnumerator(aResult, iterator);

  if (rv != NS_OK)
    return rv;

  return NS_OK;
}


PRInt32
nsPersistentProperties::Read()
{
  PRUnichar  c;
  PRUint32  nRead;
  nsresult  ret;

  ret = mIn->Read(&c, 0, 1, &nRead);
  if (ret == NS_OK && nRead == 1) {
    return c;
  }

  return -1;
}

#define IS_WHITE_SPACE(c) \
  (((c) == ' ') || ((c) == '\t') || ((c) == '\r') || ((c) == '\n'))

PRInt32
nsPersistentProperties::SkipWhiteSpace(PRInt32 c)
{
  while ((c >= 0) && IS_WHITE_SPACE(c)) {
    c = Read();
  }

  return c;
}

PRInt32
nsPersistentProperties::SkipLine(PRInt32 c)
{
  while ((c >= 0) && (c != '\r') && (c != '\n')) {
    c = Read();
  }
  if (c == '\r') {
    c = Read();
  }
  if (c == '\n') {
    c = Read();
  }

  return c;
}

////////////////////////////////////////////////////////////////////////////////
// XXX Some day we'll unify the nsIPersistentProperties interface with 
// nsIProperties, but until now...

NS_IMETHODIMP 
nsPersistentProperties::Define(const char* prop, nsISupports* initialValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::Undefine(const char* prop)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::Get(const char* prop, const nsIID & uuid, void* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::Set(const char* prop, nsISupports* value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::Has(const char* prop, PRBool *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// PropertyElement
////////////////////////////////////////////////////////////////////////////////

nsPropertyElement::nsPropertyElement()
{
    NS_INIT_REFCNT();
    mKey = nsnull;
    mValue = nsnull;
}

nsPropertyElement::~nsPropertyElement()
{
	if (mKey)
		delete mKey;
	if (mValue)
		delete mValue;
}
NS_METHOD
nsPropertyElement::Create(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;
    nsPropertyElement* propElem = new nsPropertyElement();
    if (propElem == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(propElem);
    nsresult rv = propElem->QueryInterface(aIID, aResult);
    NS_RELEASE(propElem);
    return rv;
}

NS_IMPL_ISUPPORTS1(nsPropertyElement, nsIPropertyElement)

NS_IMETHODIMP
nsPropertyElement::GetKey(PRUnichar **aReturnKey)
{
  if (aReturnKey)
  {
    *aReturnKey = (PRUnichar *) mKey->ToNewUnicode();
    return NS_OK;
  }

  return NS_ERROR_INVALID_POINTER;
}

NS_IMETHODIMP
nsPropertyElement::GetValue(PRUnichar **aReturnValue)
{
  if (aReturnValue)
  {
    *aReturnValue = (PRUnichar *) mValue->ToNewUnicode();
    return NS_OK;
  }

  return NS_ERROR_INVALID_POINTER;
}

NS_IMETHODIMP
nsPropertyElement::SetKey(nsString* aKey)
{
  mKey = aKey;

  return NS_OK;
}

NS_IMETHODIMP
nsPropertyElement::SetValue(nsString* aValue)
{
  mValue = aValue;

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

