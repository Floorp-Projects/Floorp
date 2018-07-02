/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SharedPrefMap.h"

#include "mozilla/dom/ipc/MemMapSnapshot.h"

#include "mozilla/BinarySearch.h"
#include "mozilla/ResultExtensions.h"
#include "mozilla/ipc/FileDescriptor.h"

using namespace mozilla::loader;

namespace mozilla {

using namespace ipc;

static inline size_t
GetAlignmentOffset(size_t aOffset, size_t aAlign)
{
  auto mod = aOffset % aAlign;
  return mod ? aAlign - mod : 0;
}

SharedPrefMap::SharedPrefMap(const FileDescriptor& aMapFile, size_t aMapSize)
{
  auto result = mMap.initWithHandle(aMapFile, aMapSize);
  MOZ_RELEASE_ASSERT(result.isOk());
  // We return literal nsCStrings pointing to the mapped data for preference
  // names and string values, which means that we may still have references to
  // the mapped data even after this instance is destroyed. That means that we
  // need to keep the mapping alive until process shutdown, in order to be safe.
  mMap.setPersistent();
}

SharedPrefMap::SharedPrefMap(SharedPrefMapBuilder&& aBuilder)
{
  auto result = aBuilder.Finalize(mMap);
  MOZ_RELEASE_ASSERT(result.isOk());
  mMap.setPersistent();
}

mozilla::ipc::FileDescriptor
SharedPrefMap::CloneFileDescriptor() const
{
  return mMap.cloneHandle();
}

bool
SharedPrefMap::Has(const char* aKey) const
{
  size_t index;
  return Find(aKey, &index);
}

Maybe<const SharedPrefMap::Pref>
SharedPrefMap::Get(const char* aKey) const
{
  Maybe<const Pref> result;

  size_t index;
  if (Find(aKey, &index)) {
    result.emplace(Pref{ this, &Entries()[index] });
  }

  return result;
}

bool
SharedPrefMap::Find(const char* aKey, size_t* aIndex) const
{
  const auto& keys = KeyTable();

  return BinarySearchIf(Entries(),
                        0,
                        EntryCount(),
                        [&](const Entry& aEntry) {
                          return strcmp(aKey, keys.GetBare(aEntry.mKey));
                        },
                        aIndex);
}

void
SharedPrefMapBuilder::Add(const char* aKey,
                          const Flags& aFlags,
                          bool aDefaultValue,
                          bool aUserValue)
{
  mEntries.AppendElement(Entry{
    aKey,
    mKeyTable.Add(aKey),
    { aDefaultValue, aUserValue },
    uint8_t(PrefType::Bool),
    aFlags.mHasDefaultValue,
    aFlags.mHasUserValue,
    aFlags.mIsSticky,
    aFlags.mIsLocked,
  });
}

void
SharedPrefMapBuilder::Add(const char* aKey,
                          const Flags& aFlags,
                          int32_t aDefaultValue,
                          int32_t aUserValue)
{
  ValueIdx index;
  if (aFlags.mHasUserValue) {
    index = mIntValueTable.Add(aDefaultValue, aUserValue);
  } else {
    index = mIntValueTable.Add(aDefaultValue);
  }

  mEntries.AppendElement(Entry{
    aKey,
    mKeyTable.Add(aKey),
    { index },
    uint8_t(PrefType::Int),
    aFlags.mHasDefaultValue,
    aFlags.mHasUserValue,
    aFlags.mIsSticky,
    aFlags.mIsLocked,
  });
}

void
SharedPrefMapBuilder::Add(const char* aKey,
                          const Flags& aFlags,
                          const nsCString& aDefaultValue,
                          const nsCString& aUserValue)
{
  ValueIdx index;
  StringTableEntry defaultVal = mValueStringTable.Add(aDefaultValue);
  if (aFlags.mHasUserValue) {
    StringTableEntry userVal = mValueStringTable.Add(aUserValue);
    index = mStringValueTable.Add(defaultVal, userVal);
  } else {
    index = mStringValueTable.Add(defaultVal);
  }

  mEntries.AppendElement(Entry{
    aKey,
    mKeyTable.Add(aKey),
    { index },
    uint8_t(PrefType::String),
    aFlags.mHasDefaultValue,
    aFlags.mHasUserValue,
    aFlags.mIsSticky,
    aFlags.mIsLocked,
  });
}

Result<Ok, nsresult>
SharedPrefMapBuilder::Finalize(loader::AutoMemMap& aMap)
{
  using Header = SharedPrefMap::Header;

  // Create an array of entry pointers for the entry array, and sort it by
  // preference name prior to serialization, so that entries can be looked up
  // using binary search.
  nsTArray<Entry*> entries(mEntries.Length());
  for (auto& entry : mEntries) {
    entries.AppendElement(&entry);
  }
  entries.Sort([](const Entry* aA, const Entry* aB) {
    return strcmp(aA->mKeyString, aB->mKeyString);
  });

  Header header = { uint32_t(entries.Length()) };

  size_t offset = sizeof(header);
  offset += GetAlignmentOffset(offset, alignof(Header));

  offset += entries.Length() * sizeof(SharedPrefMap::Entry);

  header.mKeyStrings.mOffset = offset;
  header.mKeyStrings.mSize = mKeyTable.Size();
  offset += header.mKeyStrings.mSize;

  offset += GetAlignmentOffset(offset, mIntValueTable.Alignment());
  header.mUserIntValues.mOffset = offset;
  header.mUserIntValues.mSize = mIntValueTable.UserSize();
  offset += header.mUserIntValues.mSize;

  offset += GetAlignmentOffset(offset, mIntValueTable.Alignment());
  header.mDefaultIntValues.mOffset = offset;
  header.mDefaultIntValues.mSize = mIntValueTable.DefaultSize();
  offset += header.mDefaultIntValues.mSize;

  offset += GetAlignmentOffset(offset, mStringValueTable.Alignment());
  header.mUserStringValues.mOffset = offset;
  header.mUserStringValues.mSize = mStringValueTable.UserSize();
  offset += header.mUserStringValues.mSize;

  offset += GetAlignmentOffset(offset, mStringValueTable.Alignment());
  header.mDefaultStringValues.mOffset = offset;
  header.mDefaultStringValues.mSize = mStringValueTable.DefaultSize();
  offset += header.mDefaultStringValues.mSize;

  header.mValueStrings.mOffset = offset;
  header.mValueStrings.mSize = mValueStringTable.Size();
  offset += header.mValueStrings.mSize;

  MemMapSnapshot mem;
  MOZ_TRY(mem.Init(offset));

  auto headerPtr = mem.Get<Header>();
  headerPtr[0] = header;

  auto* entryPtr = reinterpret_cast<SharedPrefMap::Entry*>(&headerPtr[1]);
  for (auto* entry : entries) {
    *entryPtr = {
      entry->mKey,          GetValue(*entry),
      entry->mType,         entry->mHasDefaultValue,
      entry->mHasUserValue, entry->mIsSticky,
      entry->mIsLocked,
    };
    entryPtr++;
  }

  auto ptr = mem.Get<uint8_t>();

  mKeyTable.Write(
    { &ptr[header.mKeyStrings.mOffset], header.mKeyStrings.mSize });

  mValueStringTable.Write(
    { &ptr[header.mValueStrings.mOffset], header.mValueStrings.mSize });

  mIntValueTable.WriteDefaultValues(
    { &ptr[header.mDefaultIntValues.mOffset], header.mDefaultIntValues.mSize });
  mIntValueTable.WriteUserValues(
    { &ptr[header.mUserIntValues.mOffset], header.mUserIntValues.mSize });

  mStringValueTable.WriteDefaultValues(
    { &ptr[header.mDefaultStringValues.mOffset],
      header.mDefaultStringValues.mSize });
  mStringValueTable.WriteUserValues(
    { &ptr[header.mUserStringValues.mOffset], header.mUserStringValues.mSize });

  mKeyTable.Clear();
  mValueStringTable.Clear();
  mIntValueTable.Clear();
  mStringValueTable.Clear();
  mEntries.Clear();

  return mem.Finalize(aMap);
}

} // mozilla
