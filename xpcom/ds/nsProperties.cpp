/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsID.h"

#include "nsBaseDLL.h"
#include "nsCRT.h"
#include "nsIInputStream.h"
#include "nsIProperties.h"
#include "nsIUnicharInputStream.h"
#include "nsProperties.h"
#include "plhash.h"
#include "pratom.h"

class nsProperties : public nsIProperties
{
public:
  nsProperties();
  virtual ~nsProperties();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Load(nsIInputStream* aIn);
  NS_IMETHOD GetProperty(const nsString& aKey, nsString& aValue);
  NS_IMETHOD SetProperty(const nsString& aKey, nsString& aNewValue,
                         nsString& aOldValue);
  NS_IMETHOD Save(nsIOutputStream* aOut, const nsString& aHeader);
  NS_IMETHOD Subclass(nsIProperties* aSubclass);

  PRInt32 Read();
  PRInt32 SkipLine(PRInt32 c);
  PRInt32 SkipWhiteSpace(PRInt32 c);

  nsIUnicharInputStream* mIn;
  nsIProperties*         mSubclass;
  struct PLHashTable*    mTable;
};

nsProperties::nsProperties()
{
  NS_INIT_REFCNT();

  mIn = nsnull;
  mSubclass = NS_STATIC_CAST(nsIProperties*, this);
  mTable = nsnull;
}

PR_STATIC_CALLBACK(PRIntn)
FreeHashEntries(PLHashEntry* he, PRIntn i, void* arg)
{
  delete[] (PRUnichar*)he->key;
  delete[] (PRUnichar*)he->value;
  return HT_ENUMERATE_REMOVE;
}

nsProperties::~nsProperties()
{
  if (mTable) {
    // Free the PRUnicode* pointers contained in the hash table entries
    PL_HashTableEnumerateEntries(mTable, FreeHashEntries, 0);
    PL_HashTableDestroy(mTable);
    mTable = nsnull;
  }
}

NS_DEFINE_IID(kIPropertiesIID, NS_IPROPERTIES_IID);

NS_IMPL_ISUPPORTS(nsProperties, kIPropertiesIID)

NS_IMETHODIMP
nsProperties::Load(nsIInputStream *aIn)
{
  PRInt32  c;
  nsresult ret;

  ret = NS_NewConverterStream(&mIn, nsnull, aIn);
  if (ret != NS_OK) {
    cout << "NS_NewConverterStream failed" << endl;
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

NS_IMETHODIMP
nsProperties::SetProperty(const nsString& aKey, nsString& aNewValue,
  nsString& aOldValue)
{
  // XXX The ToNewCString() calls allocate memory using "new" so this code
  // causes a memory leak...
#if 0
  cout << "will add " << aKey.ToNewCString() << "=" << aNewValue.ToNewCString() << endl;
#endif
  if (!mTable) {
    mTable = PL_NewHashTable(8, (PLHashFunction) HashKey,
      (PLHashComparator) CompareKeys,
      (PLHashComparator) nsnull, nsnull, nsnull);
    if (!mTable) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  const PRUnichar *key = aKey.GetUnicode();  // returns internal pointer (not a copy)
  PRUint32 len;
  PRUint32 hashValue = nsCRT::HashValue(key, &len);
  PLHashEntry **hep = PL_HashTableRawLookup(mTable, hashValue, key);
  PLHashEntry *he = *hep;
  if (he && aOldValue.Length()) {
    // XXX fix me
  }
  PL_HashTableRawAdd(mTable, hep, hashValue, aKey.ToNewUnicode(),
                     aNewValue.ToNewUnicode());

  return NS_OK;
}

NS_IMETHODIMP
nsProperties::Save(nsIOutputStream* aOut, const nsString& aHeader)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsProperties::Subclass(nsIProperties* aSubclass)
{
  if (aSubclass) {
    mSubclass = aSubclass;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsProperties::GetProperty(const nsString& aKey, nsString& aValue)
{
  const PRUnichar *key = aKey.GetUnicode();
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

PRInt32
nsProperties::Read()
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
nsProperties::SkipWhiteSpace(PRInt32 c)
{
  while ((c >= 0) && IS_WHITE_SPACE(c)) {
    c = Read();
  }

  return c;
}

PRInt32
nsProperties::SkipLine(PRInt32 c)
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

nsPropertiesFactory::nsPropertiesFactory()
{
  NS_INIT_REFCNT();
}

nsPropertiesFactory::~nsPropertiesFactory()
{
}

NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);

NS_IMPL_ISUPPORTS(nsPropertiesFactory, kIFactoryIID);

NS_IMETHODIMP
nsPropertiesFactory::CreateInstance(nsISupports* aOuter, REFNSIID aIID,
  void** aResult)
{
  if (aOuter) {
    return NS_ERROR_NO_AGGREGATION;
  }
  if (!aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = nsnull;
  nsProperties* props = new nsProperties();
  if (!props) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsresult ret = props->QueryInterface(aIID, aResult);
  if (NS_FAILED(ret)) {
    delete props;
  }

  return ret;
}

NS_IMETHODIMP
nsPropertiesFactory::LockFactory(PRBool aLock)
{
  if (aLock) {
    PR_AtomicIncrement(&gLockCount);
  }
  else {
    PR_AtomicDecrement(&gLockCount);
  }

  return NS_OK;
}
