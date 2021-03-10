/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_ipc_SharedPrefMap_h
#define dom_ipc_SharedPrefMap_h

#include "mozilla/AutoMemMap.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Preferences.h"
#include "mozilla/Result.h"
#include "mozilla/dom/ipc/StringTable.h"
#include "nsTHashMap.h"

namespace mozilla {

// The approximate number of preferences expected to be in an ordinary
// preferences database.
//
// This number is used to determine initial allocation sizes for data structures
// when building the shared preference map, and should be slightly higher than
// the expected number of preferences in an ordinary database to avoid
// unnecessary reallocations/rehashes.
constexpr size_t kExpectedPrefCount = 4000;

class SharedPrefMapBuilder;

// This class provides access to a compact, read-only copy of a preference
// database, backed by a shared memory buffer which can be shared between
// processes. All state data for the database is stored in the shared memory
// region, so individual instances require no dynamic memory allocation.
//
// Further, all strings returned from this API are nsLiteralCStrings with
// pointers into the shared memory region, which means that they can be copied
// into new nsCString instances without additional allocations. For instance,
// the following (where `pref` is a Pref object) will not cause any string
// copies, memory allocations, or atomic refcount changes:
//
//   nsCString prefName(pref.NameString());
//
// whereas if we returned a nsDependentCString or a dynamically allocated
// nsCString, it would.
//
// The set of entries is stored in sorted order by preference name, so look-ups
// are done by binary search. This means that look-ups have O(log n) complexity,
// rather than the O(1) complexity of a dynamic hashtable. Consumers should keep
// this in mind when planning their accesses.
//
// Important: The mapped memory created by this class is persistent. Once an
// instance has been initialized, the memory that it allocates can never be
// freed before process shutdown. Do not use it for short-lived mappings.
class SharedPrefMap {
  using FileDescriptor = mozilla::ipc::FileDescriptor;

  friend class SharedPrefMapBuilder;

  // Describes a block of memory within the shared memory region.
  struct DataBlock {
    // The byte offset from the start of the shared memory region to the start
    // of the block.
    size_t mOffset;
    // The size of the block, in bytes. This is typically used only for bounds
    // checking in debug builds.
    size_t mSize;
  };

  // Describes the contents of the shared memory region, which is laid-out as
  // follows:
  //
  // - The Header struct
  //
  // - An array of Entry structs with mEntryCount elements, lexicographically
  //   sorted by preference name.
  //
  // - A set of data blocks, with offsets and sizes described by the DataBlock
  //   entries in the header, described below.
  //
  // Each entry stores its name string and values as indices into these blocks,
  // as documented in the Entry struct, but with some important optimizations:
  //
  // - Boolean values are always stored inline. Both the default and user
  //   values can be retrieved directly from the entry. Other types have only
  //   one value index, and their values appear at the same indices in the
  //   default and user value arrays.
  //
  //   Aside from reducing our memory footprint, this space-efficiency means
  //   that we can fit more entries in the CPU cache at once, and reduces the
  //   number of likely cache misses during lookups.
  //
  // - Key strings are stored in a separate string table from value strings. As
  //   above, this makes it more likely that the strings we need will be
  //   available in the CPU cache during lookups by not interleaving them with
  //   extraneous data.
  //
  // - Default and user values are stored in separate arrays. Entries with user
  //   values always appear before entries with default values in the value
  //   arrays, and entries without user values do not have entries in the user
  //   array at all. Since the same index is used for both arrays, this means
  //   that entries with a default value but no user value do not allocate any
  //   space to store their user value.
  //
  // - For preferences with no user value, the entries in the default value are
  //   de-duplicated. All preferences with the same default value (and no user
  //   value) point to the same index in the default value array.
  //
  //
  // For example, a preference database containing:
  //
  // +---------+-------------------------------+-------------------------------+
  // | Name    | Default Value | User Value    |                               |
  // +---------+---------------+---------------+-------------------------------+
  // | string1 | "meh"         | "hem"         |                               |
  // | string2 |               | "b"           |                               |
  // | string3 | "a"           |               |                               |
  // | string4 | "foo"         |               |                               |
  // | string5 | "foo"         |               |                               |
  // | string6 | "meh"         |               |                               |
  // +---------+---------------+---------------+-------------------------------+
  // | bool1   | false         | true          |                               |
  // | bool2   |               | false         |                               |
  // | bool3   | true          |               |                               |
  // +---------+---------------+---------------+-------------------------------+
  // | int1    | 18            | 16            |                               |
  // | int2    |               | 24            |                               |
  // | int3    | 42            |               |                               |
  // | int4    | 12            |               |                               |
  // | int5    | 12            |               |                               |
  // | int6    | 18            |               |                               |
  // +---------+---------------+---------------+-------------------------------+
  //
  // Results in a database that looks like:
  //
  // +-------------------------------------------------------------------------+
  // | Header:                                                                 |
  // +-------------------------------------------------------------------------+
  // |  mEntryCount = 15                                                       |
  // |  ...                                                                    |
  // +-------------------------------------------------------------------------+
  //
  // +-------------------------------------------------------------------------+
  // | Key strings:                                                            |
  // +--------+----------------------------------------------------------------+
  // | Offset | Value                                                          |
  // +--------+----------------------------------------------------------------+
  // |      0 | string1\0                                                      |
  // |      8 | string2\0                                                      |
  // |     16 | string3\0                                                      |
  // |     24 | string4\0                                                      |
  // |     32 | string5\0                                                      |
  // |     40 | string6\0                                                      |
  // |     48 | bool1\0                                                        |
  // |     54 | bool2\0                                                        |
  // |     60 | bool3\0                                                        |
  // |     66 | int1\0                                                         |
  // |     71 | int2\0                                                         |
  // |     76 | int3\0                                                         |
  // |     81 | int4\0                                                         |
  // |     86 | int6\0                                                         |
  // |     91 | int6\0                                                         |
  // +--------+----------------------------------------------------------------+
  //
  // +-------------------------------------------------------------------------+
  // | Entries:                                                                |
  // +---------------------+------+------------+------------+------------------+
  // | Key[1]              | Type | HasDefault | HasUser    | Value            |
  // +---------------------+------+------------+------------+------------------+
  // | K["bool1", 48, 5]   | 3    | true       | true       | { false, true }  |
  // | K["bool2", 54, 5]   | 3    | false      | true       | { 0,     false } |
  // | K["bool3", 60, 5]   | 3    | true       | false      | { true,  0    }  |
  // | K["int1", 66, 4]    | 2    | true       | true       | 0                |
  // | K["int2", 71, 4]    | 2    | false      | true       | 1                |
  // | K["int3", 76, 4]    | 2    | true       | false      | 2                |
  // | K["int4", 81, 4]    | 2    | true       | false      | 3                |
  // | K["int5", 86, 4]    | 2    | true       | false      | 3                |
  // | K["int6", 91, 4]    | 2    | true       | false      | 4                |
  // | K["string1",  0, 6] | 1    | true       | true       | 0                |
  // | K["string2",  8, 6] | 1    | false      | true       | 1                |
  // | K["string3", 16, 6] | 1    | true       | false      | 2                |
  // | K["string4", 24, 6] | 1    | true       | false      | 3                |
  // | K["string5", 32, 6] | 1    | true       | false      | 3                |
  // | K["string6", 40, 6] | 1    | true       | false      | 4                |
  // +---------------------+------+------------+------------+------------------+
  // | [1]: Encoded as an offset into the key table and a length. Specified    |
  // |      as K[string, offset, length] for clarity.                          |
  // +-------------------------------------------------------------------------+
  //
  // +------------------------------------+------------------------------------+
  // | User integer values                | Default integer values             |
  // +-------+----------------------------+-------+----------------------------+
  // | Index | Contents                   | Index | Contents                   |
  // +-------+----------------------------+-------+----------------------------+
  // |     0 | 16                         |     0 | 18                         |
  // |     1 | 24                         |     1 |                            |
  // |       |                            |     2 | 42                         |
  // |       |                            |     3 | 12                         |
  // |       |                            |     4 | 18                         |
  // +-------+----------------------------+-------+----------------------------+
  // | * Note: Tables are laid out sequentially in memory, but displayed       |
  // |         here side-by-side for clarity.                                  |
  // +-------------------------------------------------------------------------+
  //
  // +------------------------------------+------------------------------------+
  // | User string values                 | Default string values              |
  // +-------+----------------------------+-------+----------------------------+
  // | Index | Contents[1]                | Index | Contents[1]                |
  // +-------+----------------------------+-------+----------------------------+
  // |     0 | V["hem", 0, 3]             |     0 | V["meh", 4, 3]             |
  // |     1 | V["b", 8, 1]               |     1 |                            |
  // |       |                            |     2 | V["a", 10, 1]              |
  // |       |                            |     3 | V["foo", 12, 3]            |
  // |       |                            |     4 | V["meh", 4, 3]             |
  // |-------+----------------------------+-------+----------------------------+
  // | [1]: Encoded as an offset into the value table and a length. Specified  |
  // |      as V[string, offset, length] for clarity.                          |
  // +-------------------------------------------------------------------------+
  // | * Note: Tables are laid out sequentially in memory, but displayed       |
  // |         here side-by-side for clarity.                                  |
  // +-------------------------------------------------------------------------+
  //
  // +-------------------------------------------------------------------------+
  // | Value strings:                                                          |
  // +--------+----------------------------------------------------------------+
  // | Offset | Value                                                          |
  // +--------+----------------------------------------------------------------+
  // |      0 | hem\0                                                          |
  // |      4 | meh\0                                                          |
  // |      8 | b\0                                                            |
  // |     10 | a\0                                                            |
  // |     12 | foo\0                                                          |
  // +--------+----------------------------------------------------------------+
  struct Header {
    // The number of entries in this map.
    uint32_t mEntryCount;

    // The StringTable data block for preference name strings, which act as keys
    // in the map.
    DataBlock mKeyStrings;

    // The int32_t arrays of user and default int preference values. Entries in
    // the map store their values as indices into these arrays.
    DataBlock mUserIntValues;
    DataBlock mDefaultIntValues;

    // The StringTableEntry arrays of user and default string preference values.
    //
    // Strings are stored as StringTableEntry structs with character offsets
    // into the mValueStrings string table and their corresponding lengths.
    //
    // Entries in the map, likewise, store their string values as indices into
    // these arrays.
    DataBlock mUserStringValues;
    DataBlock mDefaultStringValues;

    // The StringTable data block for string preference values, referenced by
    // the above two data blocks.
    DataBlock mValueStrings;
  };

  using StringTableEntry = mozilla::dom::ipc::StringTableEntry;

  // Represents a preference value, as either a pair of boolean values, or an
  // index into one of the above value arrays.
  union Value {
    Value(bool aDefaultValue, bool aUserValue)
        : mDefaultBool(aDefaultValue), mUserBool(aUserValue) {}

    MOZ_IMPLICIT Value(uint16_t aIndex) : mIndex(aIndex) {}

    // The index of this entry in the value arrays.
    //
    // User and default preference values have the same indices in their
    // respective arrays. However, entries without a user value are not
    // guaranteed to have space allocated for them in the user value array, and
    // likewise for preferences without default values in the default value
    // array. This means that callers must only access value entries for entries
    // which claim to have a value of that type.
    uint16_t mIndex;
    struct {
      bool mDefaultBool;
      bool mUserBool;
    };
  };

  // Represents a preference entry in the map, containing its name, type info,
  // flags, and a reference to its value.
  struct Entry {
    // A pointer to the preference name in the KeyTable string table.
    StringTableEntry mKey;

    // The preference's value, either as a pair of booleans, or an index into
    // the value arrays. Please see the documentation for the Value struct
    // above.
    Value mValue;

    // The preference's type, as a PrefType enum value. This must *never* be
    // PrefType::None for values in a shared array.
    uint8_t mType : 2;
    // True if the preference has a default value. Callers must not attempt to
    // access the entry's default value if this is false.
    uint8_t mHasDefaultValue : 1;
    // True if the preference has a user value. Callers must not attempt to
    // access the entry's user value if this is false.
    uint8_t mHasUserValue : 1;
    // True if the preference is sticky, as defined by the preference service.
    uint8_t mIsSticky : 1;
    // True if the preference is locked, as defined by the preference service.
    uint8_t mIsLocked : 1;
    // True if the preference should be skipped while iterating over the
    // SharedPrefMap. This is used to internally store Once StaticPrefs.
    // This property is not visible to users the way sticky and locked are.
    uint8_t mIsSkippedByIteration : 1;
  };

 public:
  NS_INLINE_DECL_REFCOUNTING(SharedPrefMap)

  // A temporary wrapper class for accessing entries in the array. Instances of
  // this class are valid as long as SharedPrefMap instance is alive, but
  // generally should not be stored long term, or allocated on the heap.
  //
  // The class is implemented as two pointers, one to the SharedPrefMap
  // instance, and one to the Entry that corresponds to the preference, and is
  // meant to be cheaply returned by value from preference lookups and
  // iterators. All property accessors lazily fetch the appropriate values from
  // the shared memory region.
  class MOZ_STACK_CLASS Pref final {
   public:
    const char* Name() const { return mMap->KeyTable().GetBare(mEntry->mKey); }

    nsCString NameString() const { return mMap->KeyTable().Get(mEntry->mKey); }

    PrefType Type() const {
      MOZ_ASSERT(PrefType(mEntry->mType) != PrefType::None);
      return PrefType(mEntry->mType);
    }

    bool HasDefaultValue() const { return mEntry->mHasDefaultValue; }
    bool HasUserValue() const { return mEntry->mHasUserValue; }
    bool IsLocked() const { return mEntry->mIsLocked; }
    bool IsSticky() const { return mEntry->mIsSticky; }
    bool IsSkippedByIteration() const { return mEntry->mIsSkippedByIteration; }

    bool GetBoolValue(PrefValueKind aKind = PrefValueKind::User) const {
      MOZ_ASSERT(Type() == PrefType::Bool);
      MOZ_ASSERT(aKind == PrefValueKind::Default ? HasDefaultValue()
                                                 : HasUserValue());

      return aKind == PrefValueKind::Default ? mEntry->mValue.mDefaultBool
                                             : mEntry->mValue.mUserBool;
    }

    int32_t GetIntValue(PrefValueKind aKind = PrefValueKind::User) const {
      MOZ_ASSERT(Type() == PrefType::Int);
      MOZ_ASSERT(aKind == PrefValueKind::Default ? HasDefaultValue()
                                                 : HasUserValue());

      return aKind == PrefValueKind::Default
                 ? mMap->DefaultIntValues()[mEntry->mValue.mIndex]
                 : mMap->UserIntValues()[mEntry->mValue.mIndex];
    }

   private:
    const StringTableEntry& GetStringEntry(PrefValueKind aKind) const {
      MOZ_ASSERT(Type() == PrefType::String);
      MOZ_ASSERT(aKind == PrefValueKind::Default ? HasDefaultValue()
                                                 : HasUserValue());

      return aKind == PrefValueKind::Default
                 ? mMap->DefaultStringValues()[mEntry->mValue.mIndex]
                 : mMap->UserStringValues()[mEntry->mValue.mIndex];
    }

   public:
    nsCString GetStringValue(PrefValueKind aKind = PrefValueKind::User) const {
      return mMap->ValueTable().Get(GetStringEntry(aKind));
    }

    const char* GetBareStringValue(
        PrefValueKind aKind = PrefValueKind::User) const {
      return mMap->ValueTable().GetBare(GetStringEntry(aKind));
    }

    // Returns the entry's index in the map, as understood by GetKeyAt() and
    // GetValueAt().
    size_t Index() const { return mEntry - mMap->Entries().get(); }

    bool operator==(const Pref& aPref) const { return mEntry == aPref.mEntry; }
    bool operator!=(const Pref& aPref) const { return !(*this == aPref); }

    // This is odd, but necessary in order for the C++ range iterator protocol
    // to work here.
    Pref& operator*() { return *this; }

    // Updates this wrapper to point to the next visible entry in the map. This
    // should not be attempted unless Index() is less than the map's Count().
    Pref& operator++() {
      do {
        mEntry++;
      } while (mEntry->mIsSkippedByIteration && Index() < mMap->Count());
      return *this;
    }

    Pref(const Pref& aPref) = default;

   protected:
    friend class SharedPrefMap;

    Pref(const SharedPrefMap* aPrefMap, const Entry* aEntry)
        : mMap(aPrefMap), mEntry(aEntry) {}

   private:
    const SharedPrefMap* const mMap;
    const Entry* mEntry;
  };

  // Note: These constructors are infallible, because the preference database is
  // critical to platform functionality, and we cannot operate without it.
  SharedPrefMap(const FileDescriptor&, size_t);
  explicit SharedPrefMap(SharedPrefMapBuilder&&);

  // Searches for the given preference in the map, and returns true if it
  // exists.
  bool Has(const char* aKey) const;

  bool Has(const nsCString& aKey) const { return Has(aKey.get()); }

  // Searches for the given preference in the map, and if it exists, returns
  // a Some<Pref> containing its details.
  Maybe<const Pref> Get(const char* aKey) const;

  Maybe<const Pref> Get(const nsCString& aKey) const { return Get(aKey.get()); }

 private:
  // Searches for an entry for the given key. If found, returns true, and
  // places its index in the entry array in aIndex.
  bool Find(const char* aKey, size_t* aIndex) const;

 public:
  // Returns the number of entries in the map.
  uint32_t Count() const { return EntryCount(); }

  // Returns the string entry at the given index. Keys are guaranteed to be
  // sorted lexicographically.
  //
  // The given index *must* be less than the value returned by Count().
  //
  // The returned value is a literal string which references the mapped memory
  // region.
  nsCString GetKeyAt(uint32_t aIndex) const {
    MOZ_ASSERT(aIndex < Count());
    return KeyTable().Get(Entries()[aIndex].mKey);
  }

  // Returns the value for the entry at the given index.
  //
  // The given index *must* be less than the value returned by Count().
  //
  // The returned value is valid for the lifetime of this map instance.
  const Pref GetValueAt(uint32_t aIndex) const {
    MOZ_ASSERT(aIndex < Count());
    return UncheckedGetValueAt(aIndex);
  }

 private:
  // Returns a wrapper with a pointer to an entry without checking its bounds.
  // This should only be used by range iterators, to check their end positions.
  //
  // Note: In debug builds, the RangePtr returned by entries will still assert
  // that aIndex is no more than 1 past the last element in the array, since it
  // also takes into account the ranged iteration use case.
  Pref UncheckedGetValueAt(uint32_t aIndex) const {
    return {this, (Entries() + aIndex).get()};
  }

 public:
  // C++ range iterator protocol. begin() and end() return references to the
  // first (non-skippable) and last entries in the array. The begin wrapper
  // can be incremented until it matches the last element in the array, at which
  // point it becomes invalid and the iteration is over.
  Pref begin() const {
    for (uint32_t aIndex = 0; aIndex < Count(); aIndex++) {
      Pref pref = UncheckedGetValueAt(aIndex);
      if (!pref.IsSkippedByIteration()) {
        return pref;
      }
    }
    return end();
  }
  Pref end() const { return UncheckedGetValueAt(Count()); }

  // A cosmetic helper for range iteration. Returns a reference value from a
  // pointer to this instance so that its .begin() and .end() methods can be
  // accessed in a ranged for loop. `map->Iter()` is equivalent to `*map`, but
  // makes its purpose slightly clearer.
  const SharedPrefMap& Iter() const { return *this; }

  // Returns a copy of the read-only file descriptor which backs the shared
  // memory region for this map. The file descriptor may be passed between
  // processes, and used to construct new instances of SharedPrefMap with
  // the same data as this instance.
  FileDescriptor CloneFileDescriptor() const;

  // Returns the size of the mapped memory region. This size must be passed to
  // the constructor when mapping the shared region in another process.
  size_t MapSize() const { return mMap.size(); }

 protected:
  ~SharedPrefMap() = default;

 private:
  template <typename T>
  using StringTable = mozilla::dom::ipc::StringTable<T>;

  // Type-safe getters for values in the shared memory region:
  const Header& GetHeader() const { return mMap.get<Header>()[0]; }

  RangedPtr<const Entry> Entries() const {
    return {reinterpret_cast<const Entry*>(&GetHeader() + 1), EntryCount()};
  }

  uint32_t EntryCount() const { return GetHeader().mEntryCount; }

  template <typename T>
  RangedPtr<const T> GetBlock(const DataBlock& aBlock) const {
    return RangedPtr<uint8_t>(&mMap.get<uint8_t>()[aBlock.mOffset],
                              aBlock.mSize)
        .ReinterpretCast<const T>();
  }

  RangedPtr<const int32_t> DefaultIntValues() const {
    return GetBlock<int32_t>(GetHeader().mDefaultIntValues);
  }
  RangedPtr<const int32_t> UserIntValues() const {
    return GetBlock<int32_t>(GetHeader().mUserIntValues);
  }

  RangedPtr<const StringTableEntry> DefaultStringValues() const {
    return GetBlock<StringTableEntry>(GetHeader().mDefaultStringValues);
  }
  RangedPtr<const StringTableEntry> UserStringValues() const {
    return GetBlock<StringTableEntry>(GetHeader().mUserStringValues);
  }

  StringTable<nsCString> KeyTable() const {
    auto& block = GetHeader().mKeyStrings;
    return {{&mMap.get<uint8_t>()[block.mOffset], block.mSize}};
  }

  StringTable<nsCString> ValueTable() const {
    auto& block = GetHeader().mValueStrings;
    return {{&mMap.get<uint8_t>()[block.mOffset], block.mSize}};
  }

  loader::AutoMemMap mMap;
};

// A helper class which builds the contiguous look-up table used by
// SharedPrefMap. Each preference in the final map is added to the builder,
// before it is finalized and transformed into a read-only snapshot.
class MOZ_RAII SharedPrefMapBuilder {
 public:
  SharedPrefMapBuilder() = default;

  // The set of flags for the preference, as documented in SharedPrefMap::Entry.
  struct Flags {
    uint8_t mHasDefaultValue : 1;
    uint8_t mHasUserValue : 1;
    uint8_t mIsSticky : 1;
    uint8_t mIsLocked : 1;
    uint8_t mIsSkippedByIteration : 1;
  };

  void Add(const nsCString& aKey, const Flags& aFlags, bool aDefaultValue,
           bool aUserValue);

  void Add(const nsCString& aKey, const Flags& aFlags, int32_t aDefaultValue,
           int32_t aUserValue);

  void Add(const nsCString& aKey, const Flags& aFlags,
           const nsCString& aDefaultValue, const nsCString& aUserValue);

  // Finalizes the binary representation of the map, writes it to a shared
  // memory region, and then initializes the given AutoMemMap with a reference
  // to the read-only copy of it.
  //
  // This should generally not be used directly by callers. The
  // SharedPrefMapBuilder instance should instead be passed to the SharedPrefMap
  // constructor as a move reference.
  Result<Ok, nsresult> Finalize(loader::AutoMemMap& aMap);

 private:
  using StringTableEntry = mozilla::dom::ipc::StringTableEntry;
  template <typename T, typename U>
  using StringTableBuilder = mozilla::dom::ipc::StringTableBuilder<T, U>;

  // An opaque descriptor of the index of a preference entry in a value array,
  // which can be converted numeric index after the ValueTableBuilder is
  // finalized.
  struct ValueIdx {
    // The relative index of the entry, based on its class. Entries for
    // preferences with user values appear at the value arrays. Entries with
    // only default values begin after the last entry with a user value.
    uint16_t mIndex;
    bool mHasUserValue;
  };

  // A helper class for building default and user value arrays for preferences.
  //
  // As described in the SharedPrefMap class, this helper optimizes the way that
  // it builds its value arrays, in that:
  //
  // - It stores value entries for all preferences with user values before
  //   entries for preferences with only default values, and allocates no
  //   entries for preferences with only default values in the user value array.
  //   Since most preferences have only default values, this dramatically
  //   reduces the space required for value storage.
  //
  // - For preferences with only default values, it de-duplicates value entries,
  //   and returns the same indices for all preferences with the same value.
  //
  // One important complication of this approach is that it means we cannot know
  // the final index of any entry with only a default value until all entries
  // have been added to the builder, since it depends on the final number of
  // user entries in the output.
  //
  // To deal with this, when entries are added, we return an opaque ValueIndex
  // struct, from which we can calculate the final index after the map has been
  // finalized.
  template <typename HashKey, typename ValueType_>
  class ValueTableBuilder {
   public:
    using ValueType = ValueType_;

    // Adds an entry for a preference with only a default value to the array,
    // and returns an opaque descriptor for its index.
    ValueIdx Add(const ValueType& aDefaultValue) {
      auto index = uint16_t(mDefaultEntries.Count());

      return mDefaultEntries.WithEntryHandle(aDefaultValue, [&](auto&& entry) {
        entry.OrInsertWith([&] { return Entry{index, false, aDefaultValue}; });

        return ValueIdx{entry->mIndex, false};
      });
    }

    // Adds an entry for a preference with a user value to the array. Regardless
    // of whether the preference has a default value, space must be allocated
    // for it. For preferences with no default value, the actual value which
    // appears in the array at its value index is ignored.
    ValueIdx Add(const ValueType& aDefaultValue, const ValueType& aUserValue) {
      auto index = uint16_t(mUserEntries.Length());

      mUserEntries.AppendElement(Entry{index, true, aDefaultValue, aUserValue});

      return {index, true};
    }

    // Returns the final index for an entry based on its opaque index
    // descriptor. This must only be called after the caller has finished adding
    // entries to the builder.
    uint16_t GetIndex(const ValueIdx& aIndex) const {
      uint16_t base = aIndex.mHasUserValue ? 0 : UserCount();
      return base + aIndex.mIndex;
    }

    // Writes out the array of default values at the block beginning at the
    // given pointer. The block must be at least as large as the value returned
    // by DefaultSize().
    void WriteDefaultValues(const RangedPtr<uint8_t>& aBuffer) const {
      auto buffer = aBuffer.ReinterpretCast<ValueType>();

      for (const auto& entry : mUserEntries) {
        buffer[entry.mIndex] = entry.mDefaultValue;
      }

      size_t defaultsOffset = UserCount();
      for (auto iter = mDefaultEntries.ConstIter(); !iter.Done(); iter.Next()) {
        const auto& entry = iter.Data();
        buffer[defaultsOffset + entry.mIndex] = entry.mDefaultValue;
      }
    }

    // Writes out the array of user values at the block beginning at the
    // given pointer. The block must be at least as large as the value returned
    // by UserSize().
    void WriteUserValues(const RangedPtr<uint8_t>& aBuffer) const {
      auto buffer = aBuffer.ReinterpretCast<ValueType>();

      for (const auto& entry : mUserEntries) {
        buffer[entry.mIndex] = entry.mUserValue;
      }
    }

    // These return the number of entries in the default and user value arrays,
    // respectively.
    uint32_t DefaultCount() const {
      return UserCount() + mDefaultEntries.Count();
    }
    uint32_t UserCount() const { return mUserEntries.Length(); }

    // These return the byte sizes of the default and user value arrays,
    // respectively.
    uint32_t DefaultSize() const { return DefaultCount() * sizeof(ValueType); }
    uint32_t UserSize() const { return UserCount() * sizeof(ValueType); }

    void Clear() {
      mUserEntries.Clear();
      mDefaultEntries.Clear();
    }

    static constexpr size_t Alignment() { return alignof(ValueType); }

   private:
    struct Entry {
      uint16_t mIndex;
      bool mHasUserValue;
      ValueType mDefaultValue;
      ValueType mUserValue{};
    };

    AutoTArray<Entry, 256> mUserEntries;

    nsTHashMap<HashKey, Entry> mDefaultEntries;
  };

  // A special-purpose string table builder for keys which are already
  // guaranteed to be unique. Duplicate values will not be detected or
  // de-duplicated.
  template <typename CharType>
  class UniqueStringTableBuilder {
   public:
    using ElemType = CharType;

    explicit UniqueStringTableBuilder(size_t aCapacity) : mEntries(aCapacity) {}

    StringTableEntry Add(const nsTString<CharType>& aKey) {
      auto entry =
          mEntries.AppendElement(Entry{mSize, aKey.Length(), aKey.get()});

      mSize += entry->mLength + 1;

      return {entry->mOffset, entry->mLength};
    }

    void Write(const RangedPtr<uint8_t>& aBuffer) {
      auto buffer = aBuffer.ReinterpretCast<ElemType>();

      for (auto& entry : mEntries) {
        memcpy(&buffer[entry.mOffset], entry.mValue,
               sizeof(ElemType) * (entry.mLength + 1));
      }
    }

    uint32_t Count() const { return mEntries.Length(); }

    uint32_t Size() const { return mSize * sizeof(ElemType); }

    void Clear() { mEntries.Clear(); }

    static constexpr size_t Alignment() { return alignof(ElemType); }

   private:
    struct Entry {
      uint32_t mOffset;
      uint32_t mLength;
      const CharType* mValue;
    };

    nsTArray<Entry> mEntries;
    uint32_t mSize = 0;
  };

  // A preference value entry, roughly corresponding to the
  // SharedPrefMap::Value struct, but with a temporary place-holder value rather
  // than a final value index.
  union Value {
    Value(bool aDefaultValue, bool aUserValue)
        : mDefaultBool(aDefaultValue), mUserBool(aUserValue) {}

    MOZ_IMPLICIT Value(const ValueIdx& aIndex) : mIndex(aIndex) {}

    // For Bool preferences, their default and user bool values.
    struct {
      bool mDefaultBool;
      bool mUserBool;
    };
    // For Int and String preferences, an opaque descriptor for their entries in
    // their value arrays. This must be passed to the appropriate
    // ValueTableBuilder to obtain the final index when the entry is serialized.
    ValueIdx mIndex;
  };

  // A preference entry, to be converted to a SharedPrefMap::Entry struct during
  // serialization.
  struct Entry {
    // The entry's preference name, as passed to Add(). The caller is
    // responsible for keeping this pointer alive until the builder is
    // finalized.
    const char* mKeyString;
    // The entry in mKeyTable corresponding to mKeyString.
    StringTableEntry mKey;
    Value mValue;

    uint8_t mType : 2;
    uint8_t mHasDefaultValue : 1;
    uint8_t mHasUserValue : 1;
    uint8_t mIsSticky : 1;
    uint8_t mIsLocked : 1;
    uint8_t mIsSkippedByIteration : 1;
  };

  // Converts a builder Value struct to a SharedPrefMap::Value struct for
  // serialization. This must not be called before callers have finished adding
  // entries to the value array builders.
  SharedPrefMap::Value GetValue(const Entry& aEntry) const {
    switch (PrefType(aEntry.mType)) {
      case PrefType::Bool:
        return {aEntry.mValue.mDefaultBool, aEntry.mValue.mUserBool};
      case PrefType::Int:
        return {mIntValueTable.GetIndex(aEntry.mValue.mIndex)};
      case PrefType::String:
        return {mStringValueTable.GetIndex(aEntry.mValue.mIndex)};
      default:
        MOZ_ASSERT_UNREACHABLE("Invalid pref type");
        return {false, false};
    }
  }

  UniqueStringTableBuilder<char> mKeyTable{kExpectedPrefCount};
  StringTableBuilder<nsCStringHashKey, nsCString> mValueStringTable;

  ValueTableBuilder<nsUint32HashKey, uint32_t> mIntValueTable;
  ValueTableBuilder<nsGenericHashKey<StringTableEntry>, StringTableEntry>
      mStringValueTable;

  nsTArray<Entry> mEntries{kExpectedPrefCount};
};

}  // namespace mozilla

#endif  // dom_ipc_SharedPrefMap_h
