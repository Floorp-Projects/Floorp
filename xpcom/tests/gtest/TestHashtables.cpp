/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/Attributes.h"

#include "gtest/gtest.h"

namespace TestHashtables {

class TestUniChar // for nsClassHashtable
{
public:
  explicit TestUniChar(uint32_t aWord)
  {
    mWord = aWord;
  }

  ~TestUniChar() = default;

  uint32_t GetChar() const { return mWord; }

private:
  uint32_t mWord;
};

struct EntityNode {
  const char*   mStr; // never owns buffer
  uint32_t       mUnicode;
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

  explicit EntityToUnicodeEntry(const char* aKey) { mNode = nullptr; }
  EntityToUnicodeEntry(const EntityToUnicodeEntry& aEntry) { mNode = aEntry.mNode; }
  ~EntityToUnicodeEntry() = default;

  bool KeyEquals(const char* aEntity) const { return !strcmp(mNode->mStr, aEntity); }
  static const char* KeyToPointer(const char* aEntity) { return aEntity; }
  static PLDHashNumber HashKey(const char* aEntity) { return mozilla::HashString(aEntity); }
  enum { ALLOW_MEMMOVE = true };

  const EntityNode* mNode;
};

static uint32_t
nsTIterPrint(nsTHashtable<EntityToUnicodeEntry>& hash)
{
  uint32_t n = 0;
  for (auto iter = hash.Iter(); !iter.Done(); iter.Next()) {
    n++;
  }
  return n;
}

static uint32_t
nsTIterPrintRemove(nsTHashtable<EntityToUnicodeEntry>& hash)
{
  uint32_t n = 0;
  for (auto iter = hash.Iter(); !iter.Done(); iter.Next()) {
    iter.Remove();
    n++;
  }
  return n;
}

void
testTHashtable(nsTHashtable<EntityToUnicodeEntry>& hash, uint32_t numEntries) {
  uint32_t i;
  for (i = 0; i < numEntries; ++i) {
    EntityToUnicodeEntry* entry =
      hash.PutEntry(gEntities[i].mStr);

    EXPECT_TRUE(entry);

    EXPECT_FALSE(entry->mNode);
    entry->mNode = &gEntities[i];
  }

  for (i = 0; i < numEntries; ++i) {
    EntityToUnicodeEntry* entry =
      hash.GetEntry(gEntities[i].mStr);

    EXPECT_TRUE(entry);
  }

  EntityToUnicodeEntry* entry =
    hash.GetEntry("xxxy");

  EXPECT_FALSE(entry);

  uint32_t count = nsTIterPrint(hash);
  EXPECT_EQ(count, numEntries);
}

//
// all this nsIFoo stuff was copied wholesale from TestCOMPtr.cpp
//

#define NS_IFOO_IID \
{ 0x6f7652e0,  0xee43, 0x11d1, \
 { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class IFoo final : public nsISupports
  {
    public:
      NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

      IFoo();

      NS_IMETHOD_(MozExternalRefCountType) AddRef() override;
      NS_IMETHOD_(MozExternalRefCountType) Release() override;
      NS_IMETHOD QueryInterface( const nsIID&, void** ) override;

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
  }

IFoo::IFoo()
    : refcount_(0)
  {
    ++total_constructions_;
  }

IFoo::~IFoo()
  {
    ++total_destructions_;
  }

MozExternalRefCountType
IFoo::AddRef()
  {
    ++refcount_;
    return refcount_;
  }

MozExternalRefCountType
IFoo::Release()
  {
    int newcount = --refcount_;
    if ( newcount == 0 )
      {
        delete this;
      }

    return newcount;
  }

nsresult
IFoo::QueryInterface( const nsIID& aIID, void** aResult )
  {
    nsISupports* rawPtr = 0;
    nsresult status = NS_OK;

    if ( aIID.Equals(NS_GET_IID(IFoo)) )
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
    auto* foop = new IFoo();

    foop->AddRef();
    *result = foop;

    return NS_OK;
  }

} // namespace TestHashtables

using namespace TestHashtables;

TEST(Hashtable, THashtable)
{
  // check an nsTHashtable
  nsTHashtable<EntityToUnicodeEntry> EntityToUnicode(ENTITY_COUNT);

  testTHashtable(EntityToUnicode, 5);

  uint32_t count = nsTIterPrintRemove(EntityToUnicode);
  ASSERT_EQ(count, uint32_t(5));

  count = nsTIterPrint(EntityToUnicode);
  ASSERT_EQ(count, uint32_t(0));

  testTHashtable(EntityToUnicode, ENTITY_COUNT);

  EntityToUnicode.Clear();

  count = nsTIterPrint(EntityToUnicode);
  ASSERT_EQ(count, uint32_t(0));
}

TEST(Hashtables, DataHashtable)
{
  // check a data-hashtable
  nsDataHashtable<nsUint32HashKey,const char*> UniToEntity(ENTITY_COUNT);

  for (auto& entity : gEntities) {
    UniToEntity.Put(entity.mUnicode, entity.mStr);
  }

  const char* str;

  for (auto& entity : gEntities) {
    ASSERT_TRUE(UniToEntity.Get(entity.mUnicode, &str));
  }

  ASSERT_FALSE(UniToEntity.Get(99446, &str));

  uint32_t count = 0;
  for (auto iter = UniToEntity.Iter(); !iter.Done(); iter.Next()) {
    count++;
  }
  ASSERT_EQ(count, ENTITY_COUNT);

  UniToEntity.Clear();

  count = 0;
  for (auto iter = UniToEntity.Iter(); !iter.Done(); iter.Next()) {
    printf("  enumerated %u = \"%s\"\n", iter.Key(), iter.Data());
    count++;
  }
  ASSERT_EQ(count, uint32_t(0));
}

TEST(Hashtables, ClassHashtable)
{
  // check a class-hashtable
  nsClassHashtable<nsCStringHashKey,TestUniChar> EntToUniClass(ENTITY_COUNT);

  for (auto& entity : gEntities) {
    auto* temp = new TestUniChar(entity.mUnicode);
    EntToUniClass.Put(nsDependentCString(entity.mStr), temp);
  }

  TestUniChar* myChar;

  for (auto& entity : gEntities) {
    ASSERT_TRUE(EntToUniClass.Get(nsDependentCString(entity.mStr), &myChar));
  }

  ASSERT_FALSE(EntToUniClass.Get(NS_LITERAL_CSTRING("xxxx"), &myChar));

  uint32_t count = 0;
  for (auto iter = EntToUniClass.Iter(); !iter.Done(); iter.Next()) {
    count++;
  }
  ASSERT_EQ(count, ENTITY_COUNT);

  EntToUniClass.Clear();

  count = 0;
  for (auto iter = EntToUniClass.Iter(); !iter.Done(); iter.Next()) {
    count++;
  }
  ASSERT_EQ(count, uint32_t(0));
}

TEST(Hashtables, DataHashtableWithInterfaceKey)
{
  // check a data-hashtable with an interface key
  nsDataHashtable<nsISupportsHashKey,uint32_t> EntToUniClass2(ENTITY_COUNT);

  nsCOMArray<IFoo> fooArray;

  for (uint32_t i = 0; i < ENTITY_COUNT; ++i) {
    nsCOMPtr<IFoo> foo;
    CreateIFoo(getter_AddRefs(foo));
    foo->SetString(nsDependentCString(gEntities[i].mStr));

    fooArray.InsertObjectAt(foo, i);

    EntToUniClass2.Put(foo, gEntities[i].mUnicode);
  }

  uint32_t myChar2;

  for (uint32_t i = 0; i < ENTITY_COUNT; ++i) {
    ASSERT_TRUE(EntToUniClass2.Get(fooArray[i], &myChar2));
  }

  ASSERT_FALSE(EntToUniClass2.Get((nsISupports*) 0x55443316, &myChar2));

  uint32_t count = 0;
  for (auto iter = EntToUniClass2.Iter(); !iter.Done(); iter.Next()) {
    nsAutoCString s;
    nsCOMPtr<IFoo> foo = do_QueryInterface(iter.Key());
    foo->GetString(s);
    count++;
  }
  ASSERT_EQ(count, ENTITY_COUNT);

  EntToUniClass2.Clear();

  count = 0;
  for (auto iter = EntToUniClass2.Iter(); !iter.Done(); iter.Next()) {
    nsAutoCString s;
    nsCOMPtr<IFoo> foo = do_QueryInterface(iter.Key());
    foo->GetString(s);
    count++;
  }
  ASSERT_EQ(count, uint32_t(0));
}

TEST(Hashtables, InterfaceHashtable)
{
  // check an interface-hashtable with an uint32_t key
  nsInterfaceHashtable<nsUint32HashKey,IFoo> UniToEntClass2(ENTITY_COUNT);

  for (auto& entity : gEntities) {
    nsCOMPtr<IFoo> foo;
    CreateIFoo(getter_AddRefs(foo));
    foo->SetString(nsDependentCString(entity.mStr));

    UniToEntClass2.Put(entity.mUnicode, foo);
  }

  for (auto& entity : gEntities) {
    nsCOMPtr<IFoo> myEnt;
    ASSERT_TRUE(UniToEntClass2.Get(entity.mUnicode, getter_AddRefs(myEnt)));

    nsAutoCString myEntStr;
    myEnt->GetString(myEntStr);
  }

  nsCOMPtr<IFoo> myEnt;
  ASSERT_FALSE(UniToEntClass2.Get(9462, getter_AddRefs(myEnt)));

  uint32_t count = 0;
  for (auto iter = UniToEntClass2.Iter(); !iter.Done(); iter.Next()) {
    nsAutoCString s;
    iter.UserData()->GetString(s);
    count++;
  }
  ASSERT_EQ(count, ENTITY_COUNT);

  UniToEntClass2.Clear();

  count = 0;
  for (auto iter = UniToEntClass2.Iter(); !iter.Done(); iter.Next()) {
    nsAutoCString s;
    iter.Data()->GetString(s);
    count++;
  }
  ASSERT_EQ(count, uint32_t(0));
}

TEST(Hashtables, DataHashtable_LookupForAdd)
{
  // check LookupForAdd/OrInsert
  nsDataHashtable<nsUint32HashKey,const char*> UniToEntity(ENTITY_COUNT);

  for (auto& entity : gEntities) {
    auto entry = UniToEntity.LookupForAdd(entity.mUnicode);
    const char* val = entry.OrInsert([&entity] () { return entity.mStr; });
    ASSERT_FALSE(entry);
    ASSERT_TRUE(val == entity.mStr);
    ASSERT_TRUE(entry.Data() == entity.mStr);
  }

  for (auto& entity : gEntities) {
    ASSERT_TRUE(UniToEntity.LookupForAdd(entity.mUnicode));
  }

  // 0 should not be found
  size_t count = UniToEntity.Count();
  UniToEntity.Lookup(0U).Remove();
  ASSERT_TRUE(count == UniToEntity.Count());

  // Lookup should find all entries
  count = 0;
  for (auto& entity : gEntities) {
    if (UniToEntity.Lookup(entity.mUnicode)) {
      count++;
    }
  }
  ASSERT_TRUE(count == UniToEntity.Count());

  for (auto& entity : gEntities) {
    ASSERT_TRUE(UniToEntity.LookupForAdd(entity.mUnicode));
  }

  // Lookup().Remove() should remove all entries.
  for (auto& entity : gEntities) {
    if (auto entry = UniToEntity.Lookup(entity.mUnicode)) {
      entry.Remove();
    }
  }
  ASSERT_TRUE(0 == UniToEntity.Count());
}

TEST(Hashtables, ClassHashtable_LookupForAdd)
{
  // check a class-hashtable LookupForAdd with null values
  nsClassHashtable<nsCStringHashKey,TestUniChar> EntToUniClass(ENTITY_COUNT);

  for (auto& entity : gEntities) {
    auto entry = EntToUniClass.LookupForAdd(nsDependentCString(entity.mStr));
    const TestUniChar* val = entry.OrInsert([] () { return nullptr; });
    ASSERT_FALSE(entry);
    ASSERT_TRUE(val == nullptr);
    ASSERT_TRUE(entry.Data() == nullptr);
  }

  for (auto& entity : gEntities) {
    ASSERT_TRUE(EntToUniClass.LookupForAdd(nsDependentCString(entity.mStr)));
    ASSERT_TRUE(EntToUniClass.LookupForAdd(nsDependentCString(entity.mStr)).Data() == nullptr);
  }

  // "" should not be found
  size_t count = EntToUniClass.Count();
  EntToUniClass.Lookup(nsDependentCString("")).Remove();
  ASSERT_TRUE(count == EntToUniClass.Count());

  // Lookup should find all entries.
  count = 0;
  for (auto& entity : gEntities) {
    if (EntToUniClass.Lookup(nsDependentCString(entity.mStr))) {
      count++;
    }
  }
  ASSERT_TRUE(count == EntToUniClass.Count());

  for (auto& entity : gEntities) {
    ASSERT_TRUE(EntToUniClass.LookupForAdd(nsDependentCString(entity.mStr)));
  }

  // Lookup().Remove() should remove all entries.
  for (auto& entity : gEntities) {
    if (auto entry = EntToUniClass.Lookup(nsDependentCString(entity.mStr))) {
      entry.Remove();
    }
  }
  ASSERT_TRUE(0 == EntToUniClass.Count());
}
