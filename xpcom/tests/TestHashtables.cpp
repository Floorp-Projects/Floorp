/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsTHashtable.h"
#include "nsBaseHashtable.h"
#include "nsDataHashtable.h"
#include "nsInterfaceHashtable.h"
#include "nsClassHashtable.h"

#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsCOMArray.h"

#include <stdio.h>

namespace TestHashtables {

class TestUniChar // for nsClassHashtable
{
public:
  TestUniChar(PRUint32 aWord)
  {
    printf("    TestUniChar::TestUniChar() %u\n", aWord);
    mWord = aWord;
  }

  ~TestUniChar()
  {
    printf("    TestUniChar::~TestUniChar() %u\n", mWord);
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

#define ENTITY_COUNT (unsigned(sizeof(gEntities)/sizeof(EntityNode)))

class EntityToUnicodeEntry : public PLDHashEntryHdr
{
public:
  typedef const char* KeyType;
  typedef const char* KeyTypePointer;

  EntityToUnicodeEntry(const char* aKey) { mNode = nsnull; }
  EntityToUnicodeEntry(const EntityToUnicodeEntry& aEntry) { mNode = aEntry.mNode; }
  ~EntityToUnicodeEntry() { };

  bool KeyEquals(const char* aEntity) const { return !strcmp(mNode->mStr, aEntity); }
  static const char* KeyToPointer(const char* aEntity) { return aEntity; }
  static PLDHashNumber HashKey(const char* aEntity) { return mozilla::HashString(aEntity); }
  enum { ALLOW_MEMMOVE = true };

  const EntityNode* mNode;
};

PLDHashOperator
nsTEnumGo(EntityToUnicodeEntry* aEntry, void* userArg) {
  printf("  enumerated \"%s\" = %u\n", 
         aEntry->mNode->mStr, aEntry->mNode->mUnicode);

  return PL_DHASH_NEXT;
}

PLDHashOperator
nsTEnumStop(EntityToUnicodeEntry* aEntry, void* userArg) {
  printf("  enumerated \"%s\" = %u\n",
         aEntry->mNode->mStr, aEntry->mNode->mUnicode);

  return PL_DHASH_REMOVE;
}

void
testTHashtable(nsTHashtable<EntityToUnicodeEntry>& hash, PRUint32 numEntries) {
  printf("Filling hash with %d entries.\n", numEntries);

  PRUint32 i;
  for (i = 0; i < numEntries; ++i) {
    printf("  Putting entry \"%s\"...", gEntities[i].mStr);
    EntityToUnicodeEntry* entry =
      hash.PutEntry(gEntities[i].mStr);

    if (!entry) {
      printf("FAILED\n");
      exit (2);
    }
    printf("OK...");

    if (entry->mNode) {
      printf("entry already exists!\n");
      exit (3);
    }
    printf("\n");

    entry->mNode = &gEntities[i];
  }

  printf("Testing Get:\n");

  for (i = 0; i < numEntries; ++i) {
    printf("  Getting entry \"%s\"...", gEntities[i].mStr);
    EntityToUnicodeEntry* entry =
      hash.GetEntry(gEntities[i].mStr);

    if (!entry) {
      printf("FAILED\n");
      exit (4);
    }

    printf("Found %u\n", entry->mNode->mUnicode);
  }

  printf("Testing nonexistent entries...");

  EntityToUnicodeEntry* entry =
    hash.GetEntry("xxxy");

  if (entry) {
    printf("FOUND! BAD!\n");
    exit (5);
  }

  printf("not found; good.\n");

  printf("Enumerating:\n");
  PRUint32 count = hash.EnumerateEntries(nsTEnumGo, nsnull);
  if (count != numEntries) {
    printf("  Bad count!\n");
    exit (6);
  }
}

PLDHashOperator
nsDEnumRead(const PRUint32& aKey, const char* aData, void* userArg) {
  printf("  enumerated %u = \"%s\"\n", aKey, aData);
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsDEnum(const PRUint32& aKey, const char*& aData, void* userArg) {
  printf("  enumerated %u = \"%s\"\n", aKey, aData);
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsCEnumRead(const nsACString& aKey, TestUniChar* aData, void* userArg) {
  printf("  enumerated \"%s\" = %c\n",
         PromiseFlatCString(aKey).get(), aData->GetChar());
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsCEnum(const nsACString& aKey, nsAutoPtr<TestUniChar>& aData, void* userArg) {
    printf("  enumerated \"%s\" = %c\n", 
           PromiseFlatCString(aKey).get(), aData->GetChar());
  return PL_DHASH_NEXT;
}

//
// all this nsIFoo stuff was copied wholesale from TestCOMPtr.cpp
//

#define NS_IFOO_IID \
{ 0x6f7652e0,  0xee43, 0x11d1, \
 { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class IFoo : public nsISupports
  {
    public:
      NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

      IFoo();

      NS_IMETHOD_(nsrefcnt) AddRef();
      NS_IMETHOD_(nsrefcnt) Release();
      NS_IMETHOD QueryInterface( const nsIID&, void** );

      NS_IMETHOD SetString(const nsACString& /*in*/ aString);
      NS_IMETHOD GetString(nsACString& /*out*/ aString);

      static void print_totals();

    private:
      ~IFoo();

      unsigned int refcount_;

      static unsigned int total_constructions_;
      static unsigned int total_destructions_;
      nsCString mString;
  };

NS_DEFINE_STATIC_IID_ACCESSOR(IFoo, NS_IFOO_IID)

unsigned int IFoo::total_constructions_;
unsigned int IFoo::total_destructions_;

void
IFoo::print_totals()
  {
    printf("total constructions/destructions --> %d/%d\n",
           total_constructions_, total_destructions_);
  }

IFoo::IFoo()
    : refcount_(0)
  {
    ++total_constructions_;
    printf("  new IFoo@%p [#%d]\n",
           static_cast<void*>(this), total_constructions_);
  }

IFoo::~IFoo()
  {
    ++total_destructions_;
    printf("IFoo@%p::~IFoo() [#%d]\n",
           static_cast<void*>(this), total_destructions_);
  }

nsrefcnt
IFoo::AddRef()
  {
    ++refcount_;
    printf("IFoo@%p::AddRef(), refcount --> %d\n", 
           static_cast<void*>(this), refcount_);
    return refcount_;
  }

nsrefcnt
IFoo::Release()
  {
    int newcount = --refcount_;
    if ( newcount == 0 )
      printf(">>");

    printf("IFoo@%p::Release(), refcount --> %d\n",
           static_cast<void*>(this), refcount_);

    if ( newcount == 0 )
      {
        printf("  delete IFoo@%p\n", static_cast<void*>(this));
        printf("<<IFoo@%p::Release()\n", static_cast<void*>(this));
        delete this;
      }

    return newcount;
  }

nsresult
IFoo::QueryInterface( const nsIID& aIID, void** aResult )
  {
    printf("IFoo@%p::QueryInterface()\n", static_cast<void*>(this));
    nsISupports* rawPtr = 0;
    nsresult status = NS_OK;

    if ( aIID.Equals(GetIID()) )
      rawPtr = this;
    else
      {
        nsID iid_of_ISupports = NS_ISUPPORTS_IID;
        if ( aIID.Equals(iid_of_ISupports) )
          rawPtr = static_cast<nsISupports*>(this);
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
    printf("    >>CreateIFoo() --> ");
    IFoo* foop = new IFoo();
    printf("IFoo@%p\n", static_cast<void*>(foop));

    foop->AddRef();
    *result = foop;

    printf("<<CreateIFoo()\n");
    return 0;
  }

PLDHashOperator
nsIEnumRead(const PRUint32& aKey, IFoo* aFoo, void* userArg) {
  nsCAutoString str;
  aFoo->GetString(str);

  printf("  enumerated %u = \"%s\"\n", aKey, str.get());
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsIEnum(const PRUint32& aKey, nsCOMPtr<IFoo>& aData, void* userArg) {
  nsCAutoString str;
  aData->GetString(str);

  printf("  enumerated %u = \"%s\"\n", aKey, str.get());
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsIEnum2Read(nsISupports* aKey, PRUint32 aData, void* userArg) {
  nsCAutoString str;
  nsCOMPtr<IFoo> foo = do_QueryInterface(aKey);
  foo->GetString(str);


  printf("  enumerated \"%s\" = %u\n", str.get(), aData);
  return PL_DHASH_NEXT;
}

PLDHashOperator
nsIEnum2(nsISupports* aKey, PRUint32& aData, void* userArg) {
  nsCAutoString str;
  nsCOMPtr<IFoo> foo = do_QueryInterface(aKey);
  foo->GetString(str);

  printf("  enumerated \"%s\" = %u\n", str.get(), aData);
  return PL_DHASH_NEXT;
}

}

using namespace TestHashtables;

int
main(void) {
  // check an nsTHashtable
  nsTHashtable<EntityToUnicodeEntry> EntityToUnicode;

  printf("Initializing nsTHashtable...");
  EntityToUnicode.Init(ENTITY_COUNT);
  printf("OK\n");

  printf("Partially filling nsTHashtable:\n");
  testTHashtable(EntityToUnicode, 5);

  printf("Enumerate-removing...\n");
  PRUint32 count = EntityToUnicode.EnumerateEntries(nsTEnumStop, nsnull);
  if (count != 5) {
    printf("wrong count\n");
    exit (7);
  }
  printf("OK\n");

  printf("Check enumeration...");
  count = EntityToUnicode.EnumerateEntries(nsTEnumGo, nsnull);
  if (count) {
    printf("entries remain in table!\n");
    exit (8);
  }
  printf("OK\n");

  printf("Filling nsTHashtable:\n");
  testTHashtable(EntityToUnicode, ENTITY_COUNT);

  printf("Clearing...");
  EntityToUnicode.Clear();
  printf("OK\n");

  printf("Check enumeration...");
  count = EntityToUnicode.EnumerateEntries(nsTEnumGo, nsnull);
  if (count) {
    printf("entries remain in table!\n");
    exit (9);
  }
  printf("OK\n");

  //
  // now check a data-hashtable
  //

  nsDataHashtable<nsUint32HashKey,const char*> UniToEntity;

  printf("Initializing nsDataHashtable...");
  UniToEntity.Init(ENTITY_COUNT);
  printf("OK\n");

  printf("Filling hash with %u entries.\n", ENTITY_COUNT);

  PRUint32 i;
  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Putting entry %u...", gEntities[i].mUnicode);
    UniToEntity.Put(gEntities[i].mUnicode, gEntities[i].mStr);
    printf("OK...\n");
  }

  printf("Testing Get:\n");
  const char* str;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Getting entry %u...", gEntities[i].mUnicode);
    if (!UniToEntity.Get(gEntities[i].mUnicode, &str)) {
      printf("FAILED\n");
      exit (12);
    }

    printf("Found %s\n", str);
  }

  printf("Testing nonexistent entries...");
  if (UniToEntity.Get(99446, &str)) {
    printf("FOUND! BAD!\n");
    exit (13);
  }
      
  printf("not found; good.\n");
      
  printf("Enumerating:\n");
  
  count = UniToEntity.EnumerateRead(nsDEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    printf("  Bad count!\n");
    exit (14);
  }
  
  printf("Clearing...");
  UniToEntity.Clear();
  printf("OK\n");

  printf("Checking count...");
  count = UniToEntity.Enumerate(nsDEnum, nsnull);
  if (count) {
    printf("  Clear did not remove all entries.\n");
    exit (15);
  }

  printf("OK\n");

  //
  // now check a thread-safe data-hashtable
  //

  nsDataHashtableMT<nsUint32HashKey,const char*> UniToEntityL;

  printf("Initializing nsDataHashtableMT...");
  UniToEntityL.Init(ENTITY_COUNT);
  printf("OK\n");

  printf("Filling hash with %u entries.\n", ENTITY_COUNT);

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Putting entry %u...", gEntities[i].mUnicode);
    UniToEntityL.Put(gEntities[i].mUnicode, gEntities[i].mStr);
    printf("OK...\n");
  }

  printf("Testing Get:\n");

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Getting entry %u...", gEntities[i].mUnicode);
    if (!UniToEntityL.Get(gEntities[i].mUnicode, &str)) {
      printf("FAILED\n");
      exit (12);
    }

    printf("Found %s\n", str);
  }

  printf("Testing nonexistent entries...");
  if (UniToEntityL.Get(99446, &str)) {
    printf("FOUND! BAD!\n");
    exit (13);
  }
      
  printf("not found; good.\n");
      
  printf("Enumerating:\n");
  
  count = UniToEntityL.EnumerateRead(nsDEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    printf("  Bad count!\n");
    exit (14);
  }
  
  printf("Clearing...");
  UniToEntityL.Clear();
  printf("OK\n");

  printf("Checking count...");
  count = UniToEntityL.Enumerate(nsDEnum, nsnull);
  if (count) {
    printf("  Clear did not remove all entries.\n");
    exit (15);
  }

  printf("OK\n");

  //
  // now check a class-hashtable
  //

  nsClassHashtable<nsCStringHashKey,TestUniChar> EntToUniClass;

  printf("Initializing nsClassHashtable...");
  EntToUniClass.Init(ENTITY_COUNT);
  printf("OK\n");

  printf("Filling hash with %u entries.\n", ENTITY_COUNT);

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Putting entry %u...", gEntities[i].mUnicode);
    TestUniChar* temp = new TestUniChar(gEntities[i].mUnicode);

    EntToUniClass.Put(nsDependentCString(gEntities[i].mStr), temp);
    printf("OK...\n");
  }

  printf("Testing Get:\n");
  TestUniChar* myChar;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Getting entry %s...", gEntities[i].mStr);
    if (!EntToUniClass.Get(nsDependentCString(gEntities[i].mStr), &myChar)) {
      printf("FAILED\n");
      exit (18);
    }

    printf("Found %c\n", myChar->GetChar());
  }

  printf("Testing nonexistent entries...");
  if (EntToUniClass.Get(NS_LITERAL_CSTRING("xxxx"), &myChar)) {
    printf("FOUND! BAD!\n");
    exit (19);
  }
      
  printf("not found; good.\n");
      
  printf("Enumerating:\n");
  
  count = EntToUniClass.EnumerateRead(nsCEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    printf("  Bad count!\n");
    exit (20);
  }
  
  printf("Clearing...\n");
  EntToUniClass.Clear();
  printf("  Clearing OK\n");

  printf("Checking count...");
  count = EntToUniClass.Enumerate(nsCEnum, nsnull);
  if (count) {
    printf("  Clear did not remove all entries.\n");
    exit (21);
  }

  printf("OK\n");

  //
  // now check a thread-safe class-hashtable
  //

  nsClassHashtableMT<nsCStringHashKey,TestUniChar> EntToUniClassL;

  printf("Initializing nsClassHashtableMT...");
  EntToUniClassL.Init(ENTITY_COUNT);
  printf("OK\n");

  printf("Filling hash with %u entries.\n", ENTITY_COUNT);

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Putting entry %u...", gEntities[i].mUnicode);
    TestUniChar* temp = new TestUniChar(gEntities[i].mUnicode);

    EntToUniClassL.Put(nsDependentCString(gEntities[i].mStr), temp);
    printf("OK...\n");
  }

  printf("Testing Get:\n");

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Getting entry %s...", gEntities[i].mStr);
    if (!EntToUniClassL.Get(nsDependentCString(gEntities[i].mStr), &myChar)) {
      printf("FAILED\n");
      exit (18);
    }

    printf("Found %c\n", myChar->GetChar());
  }

  printf("Testing nonexistent entries...");
  if (EntToUniClassL.Get(NS_LITERAL_CSTRING("xxxx"), &myChar)) {
    printf("FOUND! BAD!\n");
    exit (19);
  }
      
  printf("not found; good.\n");
      
  printf("Enumerating:\n");
  
  count = EntToUniClassL.EnumerateRead(nsCEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    printf("  Bad count!\n");
    exit (20);
  }
  
  printf("Clearing...\n");
  EntToUniClassL.Clear();
  printf("  Clearing OK\n");

  printf("Checking count...");
  count = EntToUniClassL.Enumerate(nsCEnum, nsnull);
  if (count) {
    printf("  Clear did not remove all entries.\n");
    exit (21);
  }

  printf("OK\n");

  //
  // now check a data-hashtable with an interface key
  //

  nsDataHashtable<nsISupportsHashKey,PRUint32> EntToUniClass2;

  printf("Initializing nsDataHashtable with interface key...");
  EntToUniClass2.Init(ENTITY_COUNT);
  printf("OK\n");

  printf("Filling hash with %u entries.\n", ENTITY_COUNT);

  nsCOMArray<IFoo> fooArray;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Putting entry %u...", gEntities[i].mUnicode);
    nsCOMPtr<IFoo> foo;
    CreateIFoo(getter_AddRefs(foo));
    foo->SetString(nsDependentCString(gEntities[i].mStr));
    
    
    fooArray.InsertObjectAt(foo, i);

    EntToUniClass2.Put(foo, gEntities[i].mUnicode);
    printf("OK...\n");
  }

  printf("Testing Get:\n");
  PRUint32 myChar2;

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Getting entry %s...", gEntities[i].mStr);
    
    if (!EntToUniClass2.Get(fooArray[i], &myChar2)) {
      printf("FAILED\n");
      exit (24);
    }

    printf("Found %c\n", myChar2);
  }

  printf("Testing nonexistent entries...");
  if (EntToUniClass2.Get((nsISupports*) 0x55443316, &myChar2)) {
    printf("FOUND! BAD!\n");
    exit (25);
  }
      
  printf("not found; good.\n");
      
  printf("Enumerating:\n");
  
  count = EntToUniClass2.EnumerateRead(nsIEnum2Read, nsnull);
  if (count != ENTITY_COUNT) {
    printf("  Bad count!\n");
    exit (26);
  }
  
  printf("Clearing...\n");
  EntToUniClass2.Clear();
  printf("  Clearing OK\n");

  printf("Checking count...");
  count = EntToUniClass2.Enumerate(nsIEnum2, nsnull);
  if (count) {
    printf("  Clear did not remove all entries.\n");
    exit (27);
  }

  printf("OK\n");

  //
  // now check an interface-hashtable with an PRUint32 key
  //

  nsInterfaceHashtable<nsUint32HashKey,IFoo> UniToEntClass2;

  printf("Initializing nsInterfaceHashtable...");
  UniToEntClass2.Init(ENTITY_COUNT);
  printf("OK\n");

  printf("Filling hash with %u entries.\n", ENTITY_COUNT);

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Putting entry %u...", gEntities[i].mUnicode);
    nsCOMPtr<IFoo> foo;
    CreateIFoo(getter_AddRefs(foo));
    foo->SetString(nsDependentCString(gEntities[i].mStr));
    
    UniToEntClass2.Put(gEntities[i].mUnicode, foo);
    printf("OK...\n");
  }

  printf("Testing Get:\n");

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Getting entry %s...", gEntities[i].mStr);
    
    nsCOMPtr<IFoo> myEnt;
    if (!UniToEntClass2.Get(gEntities[i].mUnicode, getter_AddRefs(myEnt))) {
      printf("FAILED\n");
      exit (30);
    }
    
    nsCAutoString str;
    myEnt->GetString(str);
    printf("Found %s\n", str.get());
  }

  printf("Testing nonexistent entries...");
  nsCOMPtr<IFoo> myEnt;
  if (UniToEntClass2.Get(9462, getter_AddRefs(myEnt))) {
    printf("FOUND! BAD!\n");
    exit (31);
  }
      
  printf("not found; good.\n");
      
  printf("Enumerating:\n");
  
  count = UniToEntClass2.EnumerateRead(nsIEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    printf("  Bad count!\n");
    exit (32);
  }
  
  printf("Clearing...\n");
  UniToEntClass2.Clear();
  printf("  Clearing OK\n");

  printf("Checking count...");
  count = UniToEntClass2.Enumerate(nsIEnum, nsnull);
  if (count) {
    printf("  Clear did not remove all entries.\n");
    exit (33);
  }

  printf("OK\n");

  //
  // now check a thread-safe interface hashtable
  //

  nsInterfaceHashtableMT<nsUint32HashKey,IFoo> UniToEntClass2L;

  printf("Initializing nsInterfaceHashtableMT...");
  UniToEntClass2L.Init(ENTITY_COUNT);
  printf("OK\n");

  printf("Filling hash with %u entries.\n", ENTITY_COUNT);

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Putting entry %u...", gEntities[i].mUnicode);
    nsCOMPtr<IFoo> foo;
    CreateIFoo(getter_AddRefs(foo));
    foo->SetString(nsDependentCString(gEntities[i].mStr));
    
    UniToEntClass2L.Put(gEntities[i].mUnicode, foo);
    printf("OK...\n");
  }

  printf("Testing Get:\n");

  for (i = 0; i < ENTITY_COUNT; ++i) {
    printf("  Getting entry %s...", gEntities[i].mStr);
    
    nsCOMPtr<IFoo> myEnt;
    if (!UniToEntClass2L.Get(gEntities[i].mUnicode, getter_AddRefs(myEnt))) {
      printf("FAILED\n");
      exit (30);
    }
    
    nsCAutoString str;
    myEnt->GetString(str);
    printf("Found %s\n", str.get());
  }

  printf("Testing nonexistent entries...");
  if (UniToEntClass2L.Get(9462, getter_AddRefs(myEnt))) {
    printf("FOUND! BAD!\n");
    exit (31);
  }
      
  printf("not found; good.\n");
      
  printf("Enumerating:\n");
  
  count = UniToEntClass2L.EnumerateRead(nsIEnumRead, nsnull);
  if (count != ENTITY_COUNT) {
    printf("  Bad count!\n");
    exit (32);
  }
  
  printf("Clearing...\n");
  UniToEntClass2L.Clear();
  printf("  Clearing OK\n");

  printf("Checking count...");
  count = UniToEntClass2L.Enumerate(nsIEnum, nsnull);
  if (count) {
    printf("  Clear did not remove all entries.\n");
    exit (33);
  }

  printf("OK\n");

  return 0;
}
