/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is C++ hashtable templates.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg.
 * Portions created by the Initial Developer are Copyright (C) 2002
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <iostream.h>
#include "nsTHashtable.h"
#include "nsBaseHashtable.h"
#include "nsDataHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsClassHashtable.h"

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsCRT.h"
#include "nsCOMArray.h"

class TestUniChar // for nsClassHashtable
{
public:
  TestUniChar(PRUint32 aWord)
  {
    cout << "    TestUniChar::TestUniChar() " << aWord << endl;
    mWord = aWord;
  }

  ~TestUniChar()
  {
    cout << "    TestUniChar::~TestUniChar() " << mWord << endl;
  }

  PRUint32 GetChar() const { return mWord; }

private:
  PRUint32 mWord;
};

struct EntityNode {
  const char*   mStr; // never owns buffer
  PRUint32       mUnicode;
};

EntityNode gEntities[] = {
  {"nbsp",160},
  {"iexcl",161},
  {"cent",162},
  {"pound",163},
  {"curren",164},
  {"yen",165},
  {"brvbar",166},
  {"sect",167},
  {"uml",168},
  {"copy",169},
  {"ordf",170},
  {"laquo",171},
  {"not",172},
  {"shy",173},
  {"reg",174},
  {"macr",175}
};

#define ENTITY_COUNT (sizeof(gEntities)/sizeof(EntityNode))

class EntityToUnicodeEntry : public PLDHashEntryHdr
{
public:
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  EntityToUnicodeEntry(const char* aKey) { mNode = nsnull; }
  EntityToUnicodeEntry(const EntityToUnicodeEntry& aEntry) { mNode = aEntry.mNode; }
  ~EntityToUnicodeEntry() { };

  const char* GetKeyPointer() const { return mNode->mStr; }
  PRBool KeyEquals(const char* aEntity) const { return !strcmp(mNode->mStr, aEntity); }
  static const char* KeyToPointer(const char* aEntity) { return aEntity; }
  static PLDHashNumber HashKey(const char* aEntity) { return nsCRT::HashCode(aEntity); }
  enum { ALLOW_MEMMOVE = PR_TRUE };

  const EntityNode* mNode;
};

PLDHashOperator
nsTEnumGo(EntityToUnicodeEntry* aEntry, void* userArg) {
  cout << "  enumerated \"" << aEntry->mNode->mStr << "\" = " <<
    aEntry->mNode->mUnicode << endl;

  return PL_DHASH_NEXT;
}

PLDHashOperator
nsTEnumStop(EntityToUnicodeEntry* aEntry, void* userArg) {
  cout << "  enumerated \"" << aEntry->mNode->mStr << "\" = " <<
    aEntry->mNode->mUnicode << endl;

  return PL_DHASH_REMOVE;
}

void
testTHashtable(nsTHashtable<EntityToUnicodeEntry>& hash, PRUint32 numEntries) {
  cout << "Filling hash with " << numEntries << " entries." << endl;

  PRUint32 i;
  for (i = 0; i < numEntries; ++i) {
    cout << "  Putting entry \"" << gEntities[i].mStr << "\"...";
    EntityToUnicodeEntry* entry =
      hash.PutEntry(gEntities[i].mStr);

    if (!entry) {
      cout << "FAILED" << endl;
      exit (2);
    }
    cout << "OK...";

    if (entry->mNode) {
      cout << "entry already exists!" << endl;
      exit (3);
    }
    cout << endl;

    entry->mNode = &gEntities[i];
  }

  cout << "Testing Get:" << endl;

  for (i = 0; i < numEntries; ++i) {
    cout << "  Getting entry \"" << gEntities[i].mStr << "\"...";
    EntityToUnicodeEntry* entry =
      hash.GetEntry(gEntities[i].mStr);

    if (!entry) {
      cout << "FAILED" << endl;
      exit (4);
    }

    cout << "Found " << entry->mNode->mUnicode << endl;
  }

  cout << "Testing non-existent entries...";

  EntityToUnicodeEntry* entry =
    hash.GetEntry("xxxy");

  if (entry) {
    cout << "FOUND! BAD!" << endl;
    exit (5);
  }

  cout << "not found; good." << endl;

  cout << "Enumerating:" << endl;
  PRUint32 count = hash.EnumerateEntries(nsTEnumGo, nsnull);
  if (count != numEntries) {
    cout << "  Bad count!" << endl;
    exit (6);
  }
}

PLDHashOperator
nsDEnumRead(const PRUint32& aKey, const char* aData, void* userArg) {
  cout << "  enumerated " << aKey << " = \"" << aData << "\"" << endl;
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsDEnum(const PRUint32& aKey, const char*& aData, void* userArg) {
  cout << "  enumerated " << aKey << " = \"" << aData << "\"" << endl;
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsCEnumRead(const nsACString& aKey, TestUniChar* aData, void* userArg) {
  cout << "  enumerated \"" << PromiseFlatCString(aKey).get() << "\" = " <<
    aData->GetChar() << endl;
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsCEnum(const nsACString& aKey, nsAutoPtr<TestUniChar>& aData, void* userArg) {
  cout << "  enumerated \"" << PromiseFlatCString(aKey).get() << "\" = " <<
    aData->GetChar() << endl;
  return PL_DHASH_NEXT;
}

//
// all this nsIFoo stuff was copied wholesale from TestCOMPTr.cpp
//

#define NS_IFOO_IID \
{ 0x6f7652e0,  0xee43, 0x11d1, \
 { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class IFoo : public nsISupports
  {
		public:
			NS_DEFINE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

		public:
      IFoo();
      virtual ~IFoo();

      NS_IMETHOD_(nsrefcnt) AddRef();
      NS_IMETHOD_(nsrefcnt) Release();
      NS_IMETHOD QueryInterface( const nsIID&, void** );

      NS_IMETHOD SetString(const nsACString& /*in*/ aString);
      NS_IMETHOD GetString(nsACString& /*out*/ aString);

      static void print_totals();

    private:
      unsigned int refcount_;

      static unsigned int total_constructions_;
      static unsigned int total_destructions_;
      nsCString mString;
  };

unsigned int IFoo::total_constructions_;
unsigned int IFoo::total_destructions_;

void
IFoo::print_totals()
  {
    cout << "total constructions/destructions --> " << total_constructions_ << "/" << total_destructions_ << endl;
  }

IFoo::IFoo()
    : refcount_(0)
  {
    ++total_constructions_;
    cout << "  new IFoo@" << NS_STATIC_CAST(void*, this) << " [#" << total_constructions_ << "]" << endl;
  }

IFoo::~IFoo()
  {
    ++total_destructions_;
    cout << "IFoo@" << NS_STATIC_CAST(void*, this) << "::~IFoo()" << " [#" << total_destructions_ << "]" << endl;
  }

nsrefcnt
IFoo::AddRef()
  {
    ++refcount_;
    cout << "IFoo@" << NS_STATIC_CAST(void*, this) << "::AddRef(), refcount --> " << refcount_ << endl;
    return refcount_;
  }

nsrefcnt
IFoo::Release()
  {
    int wrap_message = (refcount_ == 1);
    if ( wrap_message )
      cout << ">>";
      
    --refcount_;
    cout << "IFoo@" << NS_STATIC_CAST(void*, this) << "::Release(), refcount --> " << refcount_ << endl;

    if ( !refcount_ )
      {
        cout << "  delete IFoo@" << NS_STATIC_CAST(void*, this) << endl;
        delete this;
      }

    if ( wrap_message )
      cout << "<<IFoo@" << NS_STATIC_CAST(void*, this) << "::Release()" << endl;

    return refcount_;
  }

nsresult
IFoo::QueryInterface( const nsIID& aIID, void** aResult )
	{
    cout << "IFoo@" << NS_STATIC_CAST(void*, this) << "::QueryInterface()" << endl;
		nsISupports* rawPtr = 0;
		nsresult status = NS_OK;

		if ( aIID.Equals(GetIID()) )
			rawPtr = this;
		else
			{
				nsID iid_of_ISupports = NS_ISUPPORTS_IID;
				if ( aIID.Equals(iid_of_ISupports) )
					rawPtr = NS_STATIC_CAST(nsISupports*, this);
				else
					status = NS_ERROR_NO_INTERFACE;
			}

		NS_IF_ADDREF(rawPtr);
		*aResult = rawPtr;

		return status;
	}

nsresult
IFoo::SetString(const nsACString& aString)
{
  mString = aString;
  return NS_OK;
}

nsresult
IFoo::GetString(nsACString& aString)
{
  aString = mString;
  return NS_OK;
}

nsresult
CreateIFoo( IFoo** result )
    // a typical factory function (that calls AddRef)
  {
    cout << "    >>CreateIFoo() --> ";
    IFoo* foop = new IFoo();
    cout << "IFoo@" << NS_STATIC_CAST(void*, foop) << endl;

    foop->AddRef();
    *result = foop;

    cout << "<<CreateIFoo()" << endl;
    return 0;
  }

PLDHashOperator
nsIEnumRead(const PRUint32& aKey, IFoo* aFoo, void* userArg) {
  nsCAutoString str;
  aFoo->GetString(str);

  cout << "  enumerated " << aKey << " = \"" << str.get() << "\"" << endl;
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsIEnum(const PRUint32& aKey, nsCOMPtr<IFoo>& aData, void* userArg) {
  nsCAutoString str;
  aData->GetString(str);

  cout << "  enumerated " << aKey << " = \"" << str.get() << "\"" << endl;
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsIEnum2Read(nsISupports* aKey, PRUint32 aData, void* userArg) {
  nsCAutoString str;
  nsCOMPtr<IFoo> foo = do_QueryInterface(aKey);
  foo->GetString(str);


  cout << "  enumerated \"" << str.get() << "\" = " << aData << endl;
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsIEnum2(nsISupports* aKey, PRUint32& aData, void* userArg) {
  nsCAutoString str;
  nsCOMPtr<IFoo> foo = do_QueryInterface(aKey);
  foo->GetString(str);

  cout << "  enumerated \"" << str.get() << "\" = " << aData << endl;
  return PL_DHASH_NEXT;
}

int
main(void) {
  // check an nsTHashtable
  nsTHashtable<EntityToUnicodeEntry> EntityToUnicode;

  cout << "Initializing nsTHashtable...";
  if (!EntityToUnicode.Init(ENTITY_COUNT)) {
    cout << "FAILED" << endl;
    exit (1);
  }
  cout << "OK" << endl;

  cout << "Partially filling nsTHashtable:" << endl;
  testTHashtable(EntityToUnicode, 5);

  cout << "Enumerate-removing..." << endl;
  PRUint32 count = EntityToUnicode.EnumerateEntries(nsTEnumStop, nsnull);
  if (count != 5) {
    cout << "wrong count" << endl;
    exit (7);
  }
  cout << "OK" << endl;

  cout << "Check enumeration...";
  count = EntityToUnicode.EnumerateEntries(nsTEnumGo, nsnull);
  if (count) {
    cout << "entries remain in table!" << endl;
    exit (8);
  }
  cout << "OK" << endl;

  cout << "Filling nsTHashtable:" << endl;
  testTHashtable(EntityToUnicode, ENTITY_COUNT);

  cout << "Clearing...";
  EntityToUnicode.Clear();
  cout << "OK" << endl;

  cout << "Check enumeration...";
  count = EntityToUnicode.EnumerateEntries(nsTEnumGo, nsnull);
  if (count) {
    cout << "entries remain in table!" << endl;
    exit (9);
  }
  cout << "OK" << endl;

  //
  // now check a data-hashtable
  //

  nsDataHashtable<nsUint32HashKey,const char*> UniToEntity;

  cout << "Initializing nsDataHashtable...";
  if (!UniToEntity.Init(ENTITY_COUNT)) {
    cout << "FAILED" << endl;
    exit (10);
  }
  cout << "OK" << endl;

  cout << "Filling hash with " << ENTITY_COUNT << " entries." << endl;

  PRUint32 i;
  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Putting entry " << gEntities[i].mUnicode << "...";
    if (!UniToEntity.Put(gEntities[i].mUnicode, gEntities[i].mStr)) {
      cout << "FAILED" << endl;
      exit (11);
    }
    cout << "OK..." << endl;
  }

  cout << "Testing Get:" << endl;
  const char* str;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Getting entry " << gEntities[i].mUnicode << "...";
    if (!UniToEntity.Get(gEntities[i].mUnicode, &str)) {
      cout << "FAILED" << endl;
      exit (12);
    }

    cout << "Found " << str << endl;
  }

  cout << "Testing non-existent entries...";
  if (UniToEntity.Get(99446, &str)) {
    cout << "FOUND! BAD!" << endl;
    exit (13);
  }
      
  cout << "not found; good." << endl;
      
  cout << "Enumerating:" << endl;
  
  count = UniToEntity.EnumerateRead(nsDEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    cout << "  Bad count!" << endl;
    exit (14);
  }
  
  cout << "Clearing...";
  UniToEntity.Clear();
  cout << "OK" << endl;

  cout << "Checking count...";
  count = UniToEntity.Enumerate(nsDEnum, nsnull);
  if (count) {
    cout << "  Clear did not remove all entries." << endl;
    exit (15);
  }

  cout << "OK" << endl;

  //
  // now check a thread-safe data-hashtable
  //

  nsDataHashtableMT<nsUint32HashKey,const char*> UniToEntityL;

  cout << "Initializing nsDataHashtableMT...";
  if (!UniToEntityL.Init(ENTITY_COUNT)) {
    cout << "FAILED" << endl;
    exit (10);
  }
  cout << "OK" << endl;

  cout << "Filling hash with " << ENTITY_COUNT << " entries." << endl;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Putting entry " << gEntities[i].mUnicode << "...";
    if (!UniToEntityL.Put(gEntities[i].mUnicode, gEntities[i].mStr)) {
      cout << "FAILED" << endl;
      exit (11);
    }
    cout << "OK..." << endl;
  }

  cout << "Testing Get:" << endl;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Getting entry " << gEntities[i].mUnicode << "...";
    if (!UniToEntityL.Get(gEntities[i].mUnicode, &str)) {
      cout << "FAILED" << endl;
      exit (12);
    }

    cout << "Found " << str << endl;
  }

  cout << "Testing non-existent entries...";
  if (UniToEntityL.Get(99446, &str)) {
    cout << "FOUND! BAD!" << endl;
    exit (13);
  }
      
  cout << "not found; good." << endl;
      
  cout << "Enumerating:" << endl;
  
  count = UniToEntityL.EnumerateRead(nsDEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    cout << "  Bad count!" << endl;
    exit (14);
  }
  
  cout << "Clearing...";
  UniToEntityL.Clear();
  cout << "OK" << endl;

  cout << "Checking count...";
  count = UniToEntityL.Enumerate(nsDEnum, nsnull);
  if (count) {
    cout << "  Clear did not remove all entries." << endl;
    exit (15);
  }

  cout << "OK" << endl;

  //
  // now check a class-hashtable
  //

  nsClassHashtable<nsCStringHashKey,TestUniChar> EntToUniClass;

  cout << "Initializing nsClassHashtable...";
  if (!EntToUniClass.Init(ENTITY_COUNT)) {
    cout << "FAILED" << endl;
    exit (16);
  }
  cout << "OK" << endl;

  cout << "Filling hash with " << ENTITY_COUNT << " entries." << endl;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Putting entry " << gEntities[i].mUnicode << "...";
    TestUniChar* temp = new TestUniChar(gEntities[i].mUnicode);

    if (!EntToUniClass.Put(nsDependentCString(gEntities[i].mStr), temp)) {
      cout << "FAILED" << endl;
      delete temp;
      exit (17);
    }
    cout << "OK..." << endl;
  }

  cout << "Testing Get:" << endl;
  TestUniChar* myChar;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Getting entry " << gEntities[i].mStr << "...";
    if (!EntToUniClass.Get(nsDependentCString(gEntities[i].mStr), &myChar)) {
      cout << "FAILED" << endl;
      exit (18);
    }

    cout << "Found " << myChar->GetChar() << endl;
  }

  cout << "Testing non-existent entries...";
  if (EntToUniClass.Get(NS_LITERAL_CSTRING("xxxx"), &myChar)) {
    cout << "FOUND! BAD!" << endl;
    exit (19);
  }
      
  cout << "not found; good." << endl;
      
  cout << "Enumerating:" << endl;
  
  count = EntToUniClass.EnumerateRead(nsCEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    cout << "  Bad count!" << endl;
    exit (20);
  }
  
  cout << "Clearing..." << endl;
  EntToUniClass.Clear();
  cout << "  Clearing OK" << endl;

  cout << "Checking count...";
  count = EntToUniClass.Enumerate(nsCEnum, nsnull);
  if (count) {
    cout << "  Clear did not remove all entries." << endl;
    exit (21);
  }

  cout << "OK" << endl;

  //
  // now check a thread-safe class-hashtable
  //

  nsClassHashtableMT<nsCStringHashKey,TestUniChar> EntToUniClassL;

  cout << "Initializing nsClassHashtableMT...";
  if (!EntToUniClassL.Init(ENTITY_COUNT)) {
    cout << "FAILED" << endl;
    exit (16);
  }
  cout << "OK" << endl;

  cout << "Filling hash with " << ENTITY_COUNT << " entries." << endl;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Putting entry " << gEntities[i].mUnicode << "...";
    TestUniChar* temp = new TestUniChar(gEntities[i].mUnicode);

    if (!EntToUniClassL.Put(nsDependentCString(gEntities[i].mStr), temp)) {
      cout << "FAILED" << endl;
      delete temp;
      exit (17);
    }
    cout << "OK..." << endl;
  }

  cout << "Testing Get:" << endl;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Getting entry " << gEntities[i].mStr << "...";
    if (!EntToUniClassL.Get(nsDependentCString(gEntities[i].mStr), &myChar)) {
      cout << "FAILED" << endl;
      exit (18);
    }

    cout << "Found " << myChar->GetChar() << endl;
  }

  cout << "Testing non-existent entries...";
  if (EntToUniClassL.Get(NS_LITERAL_CSTRING("xxxx"), &myChar)) {
    cout << "FOUND! BAD!" << endl;
    exit (19);
  }
      
  cout << "not found; good." << endl;
      
  cout << "Enumerating:" << endl;
  
  count = EntToUniClassL.EnumerateRead(nsCEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    cout << "  Bad count!" << endl;
    exit (20);
  }
  
  cout << "Clearing..." << endl;
  EntToUniClassL.Clear();
  cout << "  Clearing OK" << endl;

  cout << "Checking count...";
  count = EntToUniClassL.Enumerate(nsCEnum, nsnull);
  if (count) {
    cout << "  Clear did not remove all entries." << endl;
    exit (21);
  }

  cout << "OK" << endl;

  //
  // now check a data-hashtable with an interface key
  //

  nsDataHashtable<nsISupportsHashKey,PRUint32> EntToUniClass2;

  cout << "Initializing nsDataHashtable with interface key...";
  if (!EntToUniClass2.Init(ENTITY_COUNT)) {
    cout << "FAILED" << endl;
    exit (22);
  }
  cout << "OK" << endl;

  cout << "Filling hash with " << ENTITY_COUNT << " entries." << endl;

  nsCOMArray<IFoo> fooArray;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Putting entry " << gEntities[i].mUnicode << "...";
    nsCOMPtr<IFoo> foo;
    CreateIFoo(getter_AddRefs(foo));
    foo->SetString(nsDependentCString(gEntities[i].mStr));
    
    
    fooArray.InsertObjectAt(foo, i);

    if (!EntToUniClass2.Put(foo, gEntities[i].mUnicode)) {
      cout << "FAILED" << endl;
      exit (23);
    }
    cout << "OK..." << endl;
  }

  cout << "Testing Get:" << endl;
  PRUint32 myChar2;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Getting entry " << gEntities[i].mStr << "...";
    
    if (!EntToUniClass2.Get(fooArray[i], &myChar2)) {
      cout << "FAILED" << endl;
      exit (24);
    }

    cout << "Found " << myChar2 << endl;
  }

  cout << "Testing non-existent entries...";
  if (EntToUniClass2.Get((nsISupports*) 0x55443316, &myChar2)) {
    cout << "FOUND! BAD!" << endl;
    exit (25);
  }
      
  cout << "not found; good." << endl;
      
  cout << "Enumerating:" << endl;
  
  count = EntToUniClass2.EnumerateRead(nsIEnum2Read, nsnull);
  if (count != ENTITY_COUNT) {
    cout << "  Bad count!" << endl;
    exit (26);
  }
  
  cout << "Clearing..." << endl;
  EntToUniClass2.Clear();
  cout << "  Clearing OK" << endl;

  cout << "Checking count...";
  count = EntToUniClass2.Enumerate(nsIEnum2, nsnull);
  if (count) {
    cout << "  Clear did not remove all entries." << endl;
    exit (27);
  }

  cout << "OK" << endl;

  //
  // now check an interface-hashtable with an PRUint32 key
  //

  nsInterfaceHashtable<nsUint32HashKey,IFoo> UniToEntClass2;

  cout << "Initializing nsInterfaceHashtable...";
  if (!UniToEntClass2.Init(ENTITY_COUNT)) {
    cout << "FAILED" << endl;
    exit (28);
  }
  cout << "OK" << endl;

  cout << "Filling hash with " << ENTITY_COUNT << " entries." << endl;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Putting entry " << gEntities[i].mUnicode << "...";
    nsCOMPtr<IFoo> foo;
    CreateIFoo(getter_AddRefs(foo));
    foo->SetString(nsDependentCString(gEntities[i].mStr));
    
    if (!UniToEntClass2.Put(gEntities[i].mUnicode, foo)) {
      cout << "FAILED" << endl;
      exit (29);
    }
    cout << "OK..." << endl;
  }

  cout << "Testing Get:" << endl;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Getting entry " << gEntities[i].mStr << "...";
    
    nsCOMPtr<IFoo> myEnt;
    if (!UniToEntClass2.Get(gEntities[i].mUnicode, getter_AddRefs(myEnt))) {
      cout << "FAILED" << endl;
      exit (30);
    }
    
    nsCAutoString str;
    myEnt->GetString(str);
    cout << "Found " << str.get() << endl;
  }

  cout << "Testing non-existent entries...";
  nsCOMPtr<IFoo> myEnt;
  if (UniToEntClass2.Get(9462, getter_AddRefs(myEnt))) {
    cout << "FOUND! BAD!" << endl;
    exit (31);
  }
      
  cout << "not found; good." << endl;
      
  cout << "Enumerating:" << endl;
  
  count = UniToEntClass2.EnumerateRead(nsIEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    cout << "  Bad count!" << endl;
    exit (32);
  }
  
  cout << "Clearing..." << endl;
  UniToEntClass2.Clear();
  cout << "  Clearing OK" << endl;

  cout << "Checking count...";
  count = UniToEntClass2.Enumerate(nsIEnum, nsnull);
  if (count) {
    cout << "  Clear did not remove all entries." << endl;
    exit (33);
  }

  cout << "OK" << endl;

  //
  // now check a thread-safe interface hashtable
  //

  nsInterfaceHashtableMT<nsUint32HashKey,IFoo> UniToEntClass2L;

  cout << "Initializing nsInterfaceHashtableMT...";
  if (!UniToEntClass2L.Init(ENTITY_COUNT)) {
    cout << "FAILED" << endl;
    exit (28);
  }
  cout << "OK" << endl;

  cout << "Filling hash with " << ENTITY_COUNT << " entries." << endl;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Putting entry " << gEntities[i].mUnicode << "...";
    nsCOMPtr<IFoo> foo;
    CreateIFoo(getter_AddRefs(foo));
    foo->SetString(nsDependentCString(gEntities[i].mStr));
    
    if (!UniToEntClass2L.Put(gEntities[i].mUnicode, foo)) {
      cout << "FAILED" << endl;
      exit (29);
    }
    cout << "OK..." << endl;
  }

  cout << "Testing Get:" << endl;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    cout << "  Getting entry " << gEntities[i].mStr << "...";
    
    nsCOMPtr<IFoo> myEnt;
    if (!UniToEntClass2L.Get(gEntities[i].mUnicode, getter_AddRefs(myEnt))) {
      cout << "FAILED" << endl;
      exit (30);
    }
    
    nsCAutoString str;
    myEnt->GetString(str);
    cout << "Found " << str.get() << endl;
  }

  cout << "Testing non-existent entries...";
  if (UniToEntClass2L.Get(9462, getter_AddRefs(myEnt))) {
    cout << "FOUND! BAD!" << endl;
    exit (31);
  }
      
  cout << "not found; good." << endl;
      
  cout << "Enumerating:" << endl;
  
  count = UniToEntClass2L.EnumerateRead(nsIEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    cout << "  Bad count!" << endl;
    exit (32);
  }
  
  cout << "Clearing..." << endl;
  UniToEntClass2L.Clear();
  cout << "  Clearing OK" << endl;

  cout << "Checking count...";
  count = UniToEntClass2L.Enumerate(nsIEnum, nsnull);
  if (count) {
    cout << "  Clear did not remove all entries." << endl;
    exit (33);
  }

  cout << "OK" << endl;

  return 0;
}
