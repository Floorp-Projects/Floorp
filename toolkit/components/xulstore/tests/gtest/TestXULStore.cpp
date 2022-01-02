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
  nsAutoString doc(u"SetGetValue"_ns);
  nsAutoString id(u"foo"_ns);
  nsAutoString attr(u"bar"_ns);
  nsAutoString value;

  EXPECT_EQ(GetValue(doc, id, attr, value), NS_OK);
  EXPECT_TRUE(value.EqualsASCII(""));

  {
    nsAutoString value(u"baz"_ns);
    EXPECT_EQ(SetValue(doc, id, attr, value), NS_OK);
  }

  EXPECT_EQ(GetValue(doc, id, attr, value), NS_OK);
  EXPECT_TRUE(value.EqualsASCII("baz"));
}

TEST(XULStore, HasValue)
{
  nsAutoString doc(u"HasValue"_ns);
  nsAutoString id(u"foo"_ns);
  nsAutoString attr(u"bar"_ns);
  bool hasValue = true;
  EXPECT_EQ(HasValue(doc, id, attr, hasValue), NS_OK);
  EXPECT_FALSE(hasValue);
  nsAutoString value(u"baz"_ns);
  EXPECT_EQ(SetValue(doc, id, attr, value), NS_OK);
  EXPECT_EQ(HasValue(doc, id, attr, hasValue), NS_OK);
  EXPECT_TRUE(hasValue);
}

TEST(XULStore, RemoveValue)
{
  nsAutoString doc(u"RemoveValue"_ns);
  nsAutoString id(u"foo"_ns);
  nsAutoString attr(u"bar"_ns);
  nsAutoString value(u"baz"_ns);
  EXPECT_EQ(SetValue(doc, id, attr, value), NS_OK);
  EXPECT_EQ(GetValue(doc, id, attr, value), NS_OK);
  EXPECT_TRUE(value.EqualsASCII("baz"));
  EXPECT_EQ(RemoveValue(doc, id, attr), NS_OK);
  EXPECT_EQ(GetValue(doc, id, attr, value), NS_OK);
  EXPECT_TRUE(value.EqualsASCII(""));
}

TEST(XULStore, GetIDsIterator)
{
  nsAutoString doc(u"idIterDoc"_ns);
  nsAutoString id1(u"id1"_ns);
  nsAutoString id2(u"id2"_ns);
  nsAutoString id3(u"id3"_ns);
  nsAutoString attr(u"attr"_ns);
  nsAutoString value(u"value"_ns);
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
  nsAutoString otherDoc(u"otherDoc"_ns);
  nsAutoString otherID(u"otherID"_ns);
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
  nsAutoString doc(u"attrIterDoc"_ns);
  nsAutoString id(u"id"_ns);
  nsAutoString attr1(u"attr1"_ns);
  nsAutoString attr2(u"attr2"_ns);
  nsAutoString attr3(u"attr3"_ns);
  nsAutoString value(u"value"_ns);
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
  nsAutoString otherID(u"otherID"_ns);
  nsAutoString otherAttr(u"otherAttr"_ns);
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
