/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */

#define NS_IMPL_IDS

#include "nsProperties.h"

#include <iostream.h>

////////////////////////////////////////////////////////////////////////////////

nsProperties::nsProperties(nsISupports* outer)
{
    NS_INIT_AGGREGATED(outer);
}

NS_METHOD
nsProperties::Create(nsISupports *outer, REFNSIID aIID, void **aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
	 NS_ENSURE_PROPER_AGGREGATION(outer, aIID);

    nsProperties* props = new nsProperties(outer);
    if (props == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = props->AggregatedQueryInterface(aIID, aResult);
	 if (NS_FAILED(rv))
	     delete props;
    return rv;
}

PRBool
nsProperties::ReleaseValues(nsHashKey* key, void* data, void* closure)
{
    nsISupports* value = (nsISupports*)data;
    NS_IF_RELEASE(value);
    return PR_TRUE;
}

nsProperties::~nsProperties()
{
    Enumerate(ReleaseValues);
}

NS_IMPL_AGGREGATED(nsProperties);

NS_METHOD
nsProperties::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);

	 if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()))
	     *aInstancePtr = GetInner();
	 else if (aIID.Equals(nsIProperties::GetIID()))
	     *aInstancePtr = NS_STATIC_CAST(nsIProperties*, this);
	 else {
	     *aInstancePtr = nsnull;
		  return NS_NOINTERFACE;
    } 

	 NS_ADDREF((nsISupports*)*aInstancePtr);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::DefineProperty(const char* prop, nsISupports* initialValue)
{
    nsStringKey key(prop);
    if (Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Put(&key, initialValue);
    NS_ASSERTION(prevValue == NULL, "hashtable error");
    NS_IF_ADDREF(initialValue);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::UndefineProperty(const char* prop)
{
    nsStringKey key(prop);
    if (!Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Remove(&key);
    NS_IF_RELEASE(prevValue);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::GetProperty(const char* prop, nsISupports* *result)
{
    nsStringKey key(prop);
    if (!Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* value = (nsISupports*)Get(&key);
    NS_IF_ADDREF(value);
    *result = value;
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::SetProperty(const char* prop, nsISupports* value)
{
    nsStringKey key(prop);
    if (!Exists(&key))
        return NS_ERROR_FAILURE;

    nsISupports* prevValue = (nsISupports*)Put(&key, value);
    NS_IF_RELEASE(prevValue);
    NS_IF_ADDREF(value);
    return NS_OK;
}

NS_IMETHODIMP
nsProperties::HasProperty(const char* prop, nsISupports* expectedValue)
{
    nsISupports* value;
    nsresult rv = GetProperty(prop, &value);
    if (NS_FAILED(rv)) return NS_COMFALSE;
    rv = (value == expectedValue) ? NS_OK : NS_COMFALSE;
    NS_IF_RELEASE(value);
    return rv;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
NS_NewIProperties(nsIProperties* *result)
{
    return nsProperties::Create(NULL, nsIProperties::GetIID(), (void**)result);
}

////////////////////////////////////////////////////////////////////////////////
// Persistent Properties (should go in a separate file)
////////////////////////////////////////////////////////////////////////////////

#include "nsID.h"
#include "nsCRT.h"
#include "nsIInputStream.h"
#include "nsIProperties.h"
#include "nsIUnicharInputStream.h"
#include "nsProperties.h"
#include "pratom.h"

static PLHashNumber
HashKey(const PRUnichar *aString)
{
  return (PLHashNumber) nsCRT::HashValue(aString);
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
  mTable = PL_NewHashTable(8, (PLHashFunction) HashKey,
    (PLHashComparator) CompareKeys,
    (PLHashComparator) nsnull, nsnull, nsnull);
}

PR_STATIC_CALLBACK(PRIntn)
FreeHashEntries(PLHashEntry* he, PRIntn i, void* arg)
{
  delete[] (PRUnichar*)he->key;
  delete[] (PRUnichar*)he->value;
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

NS_IMPL_ISUPPORTS2(nsPersistentProperties, nsIPersistentProperties, nsIProperties)

NS_IMETHODIMP
nsPersistentProperties::Load(nsIInputStream *aIn)
{
  PRInt32  c;
  nsresult ret;

  nsString uesc("x-u-escaped");
  ret = NS_NewConverterStream(&mIn, nsnull, aIn, 0, &uesc);
  if (ret != NS_OK) {
#ifdef NS_DEBUG
    cout << "NS_NewConverterStream failed" << endl;
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
      nsAutoString key("");
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
      nsAutoString value("");
      while ((c >= 0) && (c != '\r') && (c != '\n')) {
        if (c == '\\') {
          c = Read();
          if ((c == '\r') || (c == '\n')) {
            c = SkipWhiteSpace(c);
          }
          else {
            value.Append('\\');
          }
        }
        value.Append((PRUnichar) c);
        c = Read();
      }
      value.Trim(trimThese, PR_TRUE, PR_TRUE);
      nsAutoString oldValue("");
      mSubclass->SetProperty(key, value, oldValue);
    }
  }
  mIn->Close();
  NS_RELEASE(mIn);
  NS_ASSERTION(!mIn, "unexpected remaining reference");

  return NS_OK;
}

NS_IMETHODIMP
nsPersistentProperties::SetProperty(const nsString& aKey, nsString& aNewValue,
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

  const PRUnichar *key = aKey.GetUnicode();  // returns internal pointer (not a copy)
  PRUint32 len;
  PRUint32 hashValue = nsCRT::HashValue(key, &len);
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
nsPersistentProperties::GetProperty(const nsString& aKey, nsString& aValue)
{
  if (!mTable)
     return NS_ERROR_FAILURE;

  const PRUnichar *key = aKey.GetUnicode();

  if (!mTable) {
    return NS_ERROR_FAILURE;
  }

  PRUint32 len;
  PRUint32 hashValue = nsCRT::HashValue(key, &len);
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

PRInt32
nsPersistentProperties::Read()
{
  PRUnichar  c;
  PRUint32  nRead;
  nsresult  ret;

  ret = mIn->Read(&c, 0, 1, &nRead);
  if (ret == NS_OK) {
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

NS_IMETHODIMP 
nsPersistentProperties::DefineProperty(const char* prop, nsISupports* initialValue)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::UndefineProperty(const char* prop)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::GetProperty(const char* prop, nsISupports* *result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::SetProperty(const char* prop, nsISupports* value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP 
nsPersistentProperties::HasProperty(const char* prop, nsISupports* value)
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
nsPropertyElement::GetKey(nsString** aReturnKey)
{
	if (aReturnKey)
	{
		*aReturnKey = mKey;
		return NS_OK;
	}

	return NS_ERROR_INVALID_POINTER;
}

NS_IMETHODIMP
nsPropertyElement::GetValue(nsString** aReturnValue)
{
	if (aReturnValue)
	{
		*aReturnValue = mValue;
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

