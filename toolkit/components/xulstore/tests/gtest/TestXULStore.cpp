#include <stdint.h>
#include "gtest/gtest.h"
#include "mozilla/XULStore.h"
#include "nsCOMPtr.h"
#include "nsString.h"

using mozilla::XULStoreIterator;
using mozilla::XULStore::GetAttrs;
using mozilla::XULStore::GetIDs;
using mozilla::XULStore::GetValue;
using mozilla::XULStore::HasValue;
using mozilla::XULStore::RemoveValue;
using mozilla::XULStore::SetValue;

TEST(XULStore, SetGetValue)
{
  nsAutoString doc(NS_LITERAL_STRING("SetGetValue"));
  nsAutoString id(NS_LITERAL_STRING("foo"));
  nsAutoString attr(NS_LITERAL_STRING("bar"));
  nsAutoString value;

  EXPECT_EQ(GetValue(doc, id, attr, value), NS_OK);
  EXPECT_TRUE(value.EqualsASCII(""));

  {
    nsAutoString value(NS_LITERAL_STRING("baz"));
    EXPECT_EQ(SetValue(doc, id, attr, value), NS_OK);
  }

  EXPECT_EQ(GetValue(doc, id, attr, value), NS_OK);
  EXPECT_TRUE(value.EqualsASCII("baz"));
}

TEST(XULStore, HasValue)
{
  nsAutoString doc(NS_LITERAL_STRING("HasValue"));
  nsAutoString id(NS_LITERAL_STRING("foo"));
  nsAutoString attr(NS_LITERAL_STRING("bar"));
  bool hasValue = true;
  EXPECT_EQ(HasValue(doc, id, attr, hasValue), NS_OK);
  EXPECT_FALSE(hasValue);
  nsAutoString value(NS_LITERAL_STRING("baz"));
  EXPECT_EQ(SetValue(doc, id, attr, value), NS_OK);
  EXPECT_EQ(HasValue(doc, id, attr, hasValue), NS_OK);
  EXPECT_TRUE(hasValue);
}

TEST(XULStore, RemoveValue)
{
  nsAutoString doc(NS_LITERAL_STRING("RemoveValue"));
  nsAutoString id(NS_LITERAL_STRING("foo"));
  nsAutoString attr(NS_LITERAL_STRING("bar"));
  nsAutoString value(NS_LITERAL_STRING("baz"));
  EXPECT_EQ(SetValue(doc, id, attr, value), NS_OK);
  EXPECT_EQ(GetValue(doc, id, attr, value), NS_OK);
  EXPECT_TRUE(value.EqualsASCII("baz"));
  EXPECT_EQ(RemoveValue(doc, id, attr), NS_OK);
  EXPECT_EQ(GetValue(doc, id, attr, value), NS_OK);
  EXPECT_TRUE(value.EqualsASCII(""));
}

TEST(XULStore, GetIDsIterator)
{
  nsAutoString doc(NS_LITERAL_STRING("idIterDoc"));
  nsAutoString id1(NS_LITERAL_STRING("id1"));
  nsAutoString id2(NS_LITERAL_STRING("id2"));
  nsAutoString id3(NS_LITERAL_STRING("id3"));
  nsAutoString attr(NS_LITERAL_STRING("attr"));
  nsAutoString value(NS_LITERAL_STRING("value"));
  nsAutoString id;

  // Confirm that the store doesn't have any IDs yet.
  mozilla::UniquePtr<XULStoreIterator> iter;
  EXPECT_EQ(GetIDs(doc, iter), NS_OK);
  EXPECT_FALSE(iter->HasMore());
  // EXPECT_EQ(iter->GetNext(&id), NS_ERROR_FAILURE);

  // Insert with IDs in non-alphanumeric order to confirm
  // that store will order them when iterating them.
  EXPECT_EQ(SetValue(doc, id3, attr, value), NS_OK);
  EXPECT_EQ(SetValue(doc, id1, attr, value), NS_OK);
  EXPECT_EQ(SetValue(doc, id2, attr, value), NS_OK);

  // Insert different ID for another doc to confirm that store
  // won't return it when iterating IDs for our doc.
  nsAutoString otherDoc(NS_LITERAL_STRING("otherDoc"));
  nsAutoString otherID(NS_LITERAL_STRING("otherID"));
  EXPECT_EQ(SetValue(otherDoc, otherID, attr, value), NS_OK);

  EXPECT_EQ(GetIDs(doc, iter), NS_OK);
  EXPECT_TRUE(iter->HasMore());
  EXPECT_EQ(iter->GetNext(&id), NS_OK);
  EXPECT_TRUE(id.EqualsASCII("id1"));
  EXPECT_TRUE(iter->HasMore());
  EXPECT_EQ(iter->GetNext(&id), NS_OK);
  EXPECT_TRUE(id.EqualsASCII("id2"));
  EXPECT_TRUE(iter->HasMore());
  EXPECT_EQ(iter->GetNext(&id), NS_OK);
  EXPECT_TRUE(id.EqualsASCII("id3"));
  EXPECT_FALSE(iter->HasMore());
}

TEST(XULStore, GetAttributeIterator)
{
  nsAutoString doc(NS_LITERAL_STRING("attrIterDoc"));
  nsAutoString id(NS_LITERAL_STRING("id"));
  nsAutoString attr1(NS_LITERAL_STRING("attr1"));
  nsAutoString attr2(NS_LITERAL_STRING("attr2"));
  nsAutoString attr3(NS_LITERAL_STRING("attr3"));
  nsAutoString value(NS_LITERAL_STRING("value"));
  nsAutoString attr;

  mozilla::UniquePtr<XULStoreIterator> iter;
  EXPECT_EQ(GetAttrs(doc, id, iter), NS_OK);
  EXPECT_FALSE(iter->HasMore());
  // EXPECT_EQ(iter->GetNext(&attr), NS_ERROR_FAILURE);

  // Insert with attributes in non-alphanumeric order to confirm
  // that store will order them when iterating them.
  EXPECT_EQ(SetValue(doc, id, attr3, value), NS_OK);
  EXPECT_EQ(SetValue(doc, id, attr1, value), NS_OK);
  EXPECT_EQ(SetValue(doc, id, attr2, value), NS_OK);

  // Insert different attribute for another ID to confirm that store
  // won't return it when iterating attributes for our ID.
  nsAutoString otherID(NS_LITERAL_STRING("otherID"));
  nsAutoString otherAttr(NS_LITERAL_STRING("otherAttr"));
  EXPECT_EQ(SetValue(doc, otherID, otherAttr, value), NS_OK);

  EXPECT_EQ(GetAttrs(doc, id, iter), NS_OK);
  EXPECT_TRUE(iter->HasMore());
  EXPECT_EQ(iter->GetNext(&attr), NS_OK);
  EXPECT_TRUE(attr.EqualsASCII("attr1"));
  EXPECT_TRUE(iter->HasMore());
  EXPECT_EQ(iter->GetNext(&attr), NS_OK);
  EXPECT_TRUE(attr.EqualsASCII("attr2"));
  EXPECT_TRUE(iter->HasMore());
  EXPECT_EQ(iter->GetNext(&attr), NS_OK);
  EXPECT_TRUE(attr.EqualsASCII("attr3"));
  EXPECT_FALSE(iter->HasMore());
}
