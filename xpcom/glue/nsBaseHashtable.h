/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseHashtable_h__
#define nsBaseHashtable_h__

#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "nsTHashtable.h"
#include "prlock.h"
#include "nsDebug.h"

template<class KeyClass, class DataType, class UserDataType>
class nsBaseHashtable; // forward declaration

/**
 * the private nsTHashtable::EntryType class used by nsBaseHashtable
 * @see nsTHashtable for the specification of this class
 * @see nsBaseHashtable for template parameters
 */
template<class KeyClass, class DataType>
class nsBaseHashtableET : public KeyClass
{
public:
  DataType mData;
  friend class nsTHashtable<nsBaseHashtableET<KeyClass, DataType>>;

private:
  typedef typename KeyClass::KeyType KeyType;
  typedef typename KeyClass::KeyTypePointer KeyTypePointer;

  explicit nsBaseHashtableET(KeyTypePointer aKey);
  nsBaseHashtableET(nsBaseHashtableET<KeyClass, DataType>&& aToMove);
  ~nsBaseHashtableET();
};

/**
 * templated hashtable for simple data types
 * This class manages simple data types that do not need construction or
 * destruction.
 *
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param DataType the datatype stored in the hashtable,
 *   for example, uint32_t or nsCOMPtr.  If UserDataType is not the same,
 *   DataType must implicitly cast to UserDataType
 * @param UserDataType the user sees, for example uint32_t or nsISupports*
 */
template<class KeyClass, class DataType, class UserDataType>
class nsBaseHashtable
  : protected nsTHashtable<nsBaseHashtableET<KeyClass, DataType>>
{
  typedef mozilla::fallible_t fallible_t;

public:
  typedef typename KeyClass::KeyType KeyType;
  typedef nsBaseHashtableET<KeyClass, DataType> EntryType;

  using nsTHashtable<EntryType>::Contains;

  nsBaseHashtable() {}
  explicit nsBaseHashtable(uint32_t aInitLength)
    : nsTHashtable<EntryType>(aInitLength)
  {
  }

  /**
   * Return the number of entries in the table.
   * @return    number of entries
   */
  uint32_t Count() const { return nsTHashtable<EntryType>::Count(); }

  /**
   * retrieve the value for a key.
   * @param aKey the key to retreive
   * @param aData data associated with this key will be placed at this
   *   pointer.  If you only need to check if the key exists, aData
   *   may be null.
   * @return true if the key exists. If key does not exist, aData is not
   *   modified.
   */
  bool Get(KeyType aKey, UserDataType* aData) const
  {
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return false;
    }

    if (aData) {
      *aData = ent->mData;
    }

    return true;
  }

  /**
   * For pointer types, get the value, returning nullptr if the entry is not
   * present in the table.
   *
   * @param aKey the key to retrieve
   * @return The found value, or nullptr if no entry was found with the given key.
   * @note If nullptr values are stored in the table, it is not possible to
   *       distinguish between a nullptr value and a missing entry.
   */
  UserDataType Get(KeyType aKey) const
  {
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return 0;
    }

    return ent->mData;
  }

  /**
   * put a new value for the associated key
   * @param aKey the key to put
   * @param aData the new data
   * @return always true, unless memory allocation failed
   */
  void Put(KeyType aKey, const UserDataType& aData)
  {
    if (!Put(aKey, aData, fallible_t())) {
      NS_ABORT_OOM(this->mTable.entrySize * this->mTable.entryCount);
    }
  }

  NS_WARN_UNUSED_RESULT bool Put(KeyType aKey, const UserDataType& aData,
                                 const fallible_t&) {
    EntryType* ent = this->PutEntry(aKey);
    if (!ent) {
      return false;
    }

    ent->mData = aData;

    return true;
  }

  /**
   * remove the data for the associated key
   * @param aKey the key to remove from the hashtable
   */
  void Remove(KeyType aKey) { this->RemoveEntry(aKey); }

  /**
   * function type provided by the application for enumeration.
   * @param aKey the key being enumerated
   * @param aData data being enumerated
   * @param aUserArg passed unchanged from Enumerate
   * @return either
   *   @link PLDHashOperator::PL_DHASH_NEXT PL_DHASH_NEXT @endlink or
   *   @link PLDHashOperator::PL_DHASH_STOP PL_DHASH_STOP @endlink
   */
  typedef PLDHashOperator (*EnumReadFunction)(KeyType aKey,
                                              UserDataType aData,
                                              void* aUserArg);

  /**
   * enumerate entries in the hashtable, without allowing changes
   * @param aEnumFunc enumeration callback
   * @param aUserArg passed unchanged to the EnumReadFunction
   */
  uint32_t EnumerateRead(EnumReadFunction aEnumFunc, void* aUserArg) const
  {
    NS_ASSERTION(this->mTable.entrySize,
                 "nsBaseHashtable was not initialized properly.");

    s_EnumReadArgs enumData = { aEnumFunc, aUserArg };
    return PL_DHashTableEnumerate(const_cast<PLDHashTable*>(&this->mTable),
                                  s_EnumReadStub,
                                  &enumData);
  }

  /**
   * function type provided by the application for enumeration.
   * @param aKey the key being enumerated
   * @param aData Reference to data being enumerated, may be altered. e.g. for
   *        nsInterfaceHashtable this is an nsCOMPtr reference...
   * @parm aUserArg passed unchanged from Enumerate
   * @return bitflag combination of
   *   @link PLDHashOperator::PL_DHASH_REMOVE @endlink,
   *   @link PLDHashOperator::PL_DHASH_NEXT PL_DHASH_NEXT @endlink, or
   *   @link PLDHashOperator::PL_DHASH_STOP PL_DHASH_STOP @endlink
   */
  typedef PLDHashOperator (*EnumFunction)(KeyType aKey,
                                          DataType& aData,
                                          void* aUserArg);

  /**
   * enumerate entries in the hashtable, allowing changes. This
   * functions write-locks the hashtable.
   * @param aEnumFunc enumeration callback
   * @param aUserArg passed unchanged to the EnumFunction
   */
  uint32_t Enumerate(EnumFunction aEnumFunc, void* aUserArg)
  {
    NS_ASSERTION(this->mTable.entrySize,
                 "nsBaseHashtable was not initialized properly.");

    s_EnumArgs enumData = { aEnumFunc, aUserArg };
    return PL_DHashTableEnumerate(&this->mTable,
                                  s_EnumStub,
                                  &enumData);
  }

  /**
   * reset the hashtable, removing all entries
   */
  void Clear() { nsTHashtable<EntryType>::Clear(); }

  /**
   * client must provide a SizeOfEntryExcludingThisFun function for
   *   SizeOfExcludingThis.
   * @param     aKey the key being enumerated
   * @param     aData Reference to data being enumerated.
   * @param     aMallocSizeOf the function used to measure heap-allocated blocks
   * @param     aUserArg passed unchanged from SizeOf{In,Ex}cludingThis
   * @return    summed size of the things pointed to by the entries
   */
  typedef size_t
    (*SizeOfEntryExcludingThisFun)(KeyType aKey,
                                   const DataType &aData,
                                   mozilla::MallocSizeOf aMallocSizeOf,
                                   void* aUserArg);

  /**
   * Measure the size of the table's entry storage and the table itself.
   * If |aSizeOfEntryExcludingThis| is non-nullptr, measure the size of things
   * pointed to by entries.
   *
   * @param    aSizeOfEntryExcludingThis
   *           the <code>SizeOfEntryExcludingThisFun</code> function to call
   * @param    aMallocSizeOf the function used to meeasure heap-allocated blocks
   * @param    aUserArg a point to pass to the
   *           <code>SizeOfEntryExcludingThisFun</code> function
   * @return   the summed size of the entries, the table, and the table's storage
   */
  size_t SizeOfIncludingThis(SizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
                             mozilla::MallocSizeOf aMallocSizeOf,
                             void* aUserArg = nullptr)
  {
    return aMallocSizeOf(this) +
      this->SizeOfExcludingThis(aSizeOfEntryExcludingThis, aMallocSizeOf,
                                aUserArg);
  }

  /**
   * Measure the size of the table's entry storage, and if
   * |aSizeOfEntryExcludingThis| is non-nullptr, measure the size of things
   * pointed to by entries.
   *
   * @param     aSizeOfEntryExcludingThis the
   *            <code>SizeOfEntryExcludingThisFun</code> function to call
   * @param     aMallocSizeOf the function used to measure heap-allocated blocks
   * @param     aUserArg a pointer to pass to the
   *            <code>SizeOfEntryExcludingThisFun</code> function
   * @return    the summed size of all the entries
   */
  size_t SizeOfExcludingThis(SizeOfEntryExcludingThisFun aSizeOfEntryExcludingThis,
                             mozilla::MallocSizeOf aMallocSizeOf,
                             void* aUserArg = nullptr) const
  {
    if (aSizeOfEntryExcludingThis) {
      s_SizeOfArgs args = { aSizeOfEntryExcludingThis, aUserArg };
      return PL_DHashTableSizeOfExcludingThis(&this->mTable, s_SizeOfStub,
                                              aMallocSizeOf, &args);
    }
    return PL_DHashTableSizeOfExcludingThis(&this->mTable, nullptr,
                                            aMallocSizeOf);
  }

#ifdef DEBUG
  using nsTHashtable<EntryType>::MarkImmutable;
#endif

protected:
  /**
   * used internally during EnumerateRead.  Allocated on the stack.
   * @param func the enumerator passed to EnumerateRead
   * @param userArg the userArg passed to EnumerateRead
   */
  struct s_EnumReadArgs
  {
    EnumReadFunction func;
    void* userArg;
  };

  static PLDHashOperator s_EnumReadStub(PLDHashTable* aTable,
                                        PLDHashEntryHdr* aHdr,
                                        uint32_t aNumber,
                                        void* aArg);

  struct s_EnumArgs
  {
    EnumFunction func;
    void* userArg;
  };

  static PLDHashOperator s_EnumStub(PLDHashTable* aTable,
                                    PLDHashEntryHdr* aHdr,
                                    uint32_t aNumber,
                                    void* aArg);

  struct s_SizeOfArgs
  {
    SizeOfEntryExcludingThisFun func;
    void* userArg;
  };

  static size_t s_SizeOfStub(PLDHashEntryHdr* aEntry,
                             mozilla::MallocSizeOf aMallocSizeOf,
                             void* aArg);
};

class nsCycleCollectionTraversalCallback;

struct MOZ_STACK_CLASS nsBaseHashtableCCTraversalData
{
  nsBaseHashtableCCTraversalData(nsCycleCollectionTraversalCallback& aCallback,
                                 const char* aName,
                                 uint32_t aFlags)
    : mCallback(aCallback)
    , mName(aName)
    , mFlags(aFlags)
  {
  }

  nsCycleCollectionTraversalCallback& mCallback;
  const char* mName;
  uint32_t mFlags;

};

template<typename K, typename T>
PLDHashOperator
ImplCycleCollectionTraverse_EnumFunc(K aKey,
                                     T aData,
                                     void* aUserData)
{
  nsBaseHashtableCCTraversalData* userData =
    static_cast<nsBaseHashtableCCTraversalData*>(aUserData);

  CycleCollectionNoteChild(userData->mCallback,
                           aData,
                           userData->mName,
                           userData->mFlags);
  return PL_DHASH_NEXT;
}

//
// nsBaseHashtableET definitions
//

template<class KeyClass, class DataType>
nsBaseHashtableET<KeyClass, DataType>::nsBaseHashtableET(KeyTypePointer aKey)
  : KeyClass(aKey)
{
}

template<class KeyClass, class DataType>
nsBaseHashtableET<KeyClass, DataType>::nsBaseHashtableET(
      nsBaseHashtableET<KeyClass, DataType>&& aToMove)
  : KeyClass(mozilla::Move(aToMove))
  , mData(mozilla::Move(aToMove.mData))
{
}

template<class KeyClass, class DataType>
nsBaseHashtableET<KeyClass, DataType>::~nsBaseHashtableET()
{
}


//
// nsBaseHashtable definitions
//

template<class KeyClass, class DataType, class UserDataType>
PLDHashOperator
nsBaseHashtable<KeyClass, DataType, UserDataType>::s_EnumReadStub(
    PLDHashTable* aTable, PLDHashEntryHdr* aHdr, uint32_t aNumber, void* aArg)
{
  EntryType* ent = static_cast<EntryType*>(aHdr);
  s_EnumReadArgs* eargs = (s_EnumReadArgs*)aArg;

  PLDHashOperator res = (eargs->func)(ent->GetKey(), ent->mData, eargs->userArg);

  NS_ASSERTION(!(res & PL_DHASH_REMOVE),
               "PL_DHASH_REMOVE return during const enumeration; ignoring.");

  if (res & PL_DHASH_STOP) {
    return PL_DHASH_STOP;
  }

  return PL_DHASH_NEXT;
}

template<class KeyClass, class DataType, class UserDataType>
PLDHashOperator
nsBaseHashtable<KeyClass, DataType, UserDataType>::s_EnumStub(
    PLDHashTable* aTable, PLDHashEntryHdr* aHdr, uint32_t aNumber, void* aArg)
{
  EntryType* ent = static_cast<EntryType*>(aHdr);
  s_EnumArgs* eargs = (s_EnumArgs*)aArg;

  return (eargs->func)(ent->GetKey(), ent->mData, eargs->userArg);
}

template<class KeyClass, class DataType, class UserDataType>
size_t
nsBaseHashtable<KeyClass, DataType, UserDataType>::s_SizeOfStub(
    PLDHashEntryHdr* aHdr, mozilla::MallocSizeOf aMallocSizeOf, void* aArg)
{
  EntryType* ent = static_cast<EntryType*>(aHdr);
  s_SizeOfArgs* eargs = static_cast<s_SizeOfArgs*>(aArg);

  return (eargs->func)(ent->GetKey(), ent->mData, aMallocSizeOf, eargs->userArg);
}

#endif // nsBaseHashtable_h__
