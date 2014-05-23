/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTHashtable_h__
#define nsTHashtable_h__

#include "nscore.h"
#include "pldhash.h"
#include "nsDebug.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/Move.h"
#include "mozilla/fallible.h"
#include "mozilla/PodOperations.h"

#include <new>

// helper function for nsTHashtable::Clear()
NS_COM_GLUE PLDHashOperator
PL_DHashStubEnumRemove(PLDHashTable    *table,
                       PLDHashEntryHdr *entry,
                       uint32_t         ordinal,
                       void            *userArg);


/**
 * a base class for templated hashtables.
 *
 * Clients will rarely need to use this class directly. Check the derived
 * classes first, to see if they will meet your needs.
 *
 * @param EntryType  the templated entry-type class that is managed by the
 *   hashtable. <code>EntryType</code> must extend the following declaration,
 *   and <strong>must not declare any virtual functions or derive from classes
 *   with virtual functions.</strong>  Any vtable pointer would break the
 *   PLDHashTable code.
 *<pre>   class EntryType : public PLDHashEntryHdr
 *   {
 *   public: or friend nsTHashtable<EntryType>;
 *     // KeyType is what we use when Get()ing or Put()ing this entry
 *     // this should either be a simple datatype (uint32_t, nsISupports*) or
 *     // a const reference (const nsAString&)
 *     typedef something KeyType;
 *     // KeyTypePointer is the pointer-version of KeyType, because pldhash.h
 *     // requires keys to cast to <code>const void*</code>
 *     typedef const something* KeyTypePointer;
 *
 *     EntryType(KeyTypePointer aKey);
 *
 *     // A copy or C++11 Move constructor must be defined, even if
 *     // AllowMemMove() == true, otherwise you will cause link errors.
 *     EntryType(const EntryType& aEnt);  // Either this...
 *     EntryType(EntryType&& aEnt);       // ...or this
 *
 *     // the destructor must be defined... or you will cause link errors!
 *     ~EntryType();
 *
 *     // KeyEquals(): does this entry match this key?
 *     bool KeyEquals(KeyTypePointer aKey) const;
 *
 *     // KeyToPointer(): Convert KeyType to KeyTypePointer
 *     static KeyTypePointer KeyToPointer(KeyType aKey);
 *
 *     // HashKey(): calculate the hash number
 *     static PLDHashNumber HashKey(KeyTypePointer aKey);
 *
 *     // ALLOW_MEMMOVE can we move this class with memmove(), or do we have
 *     // to use the copy constructor?
 *     enum { ALLOW_MEMMOVE = true/false };
 *   }</pre>
 *
 * @see nsInterfaceHashtable
 * @see nsDataHashtable
 * @see nsClassHashtable
 * @author "Benjamin Smedberg <bsmedberg@covad.net>"
 */

template<class EntryType>
class nsTHashtable
{
  typedef mozilla::fallible_t fallible_t;

public:
  // Separate constructors instead of default aInitSize parameter since
  // otherwise the default no-arg constructor isn't found.
  nsTHashtable()
  {
    Init(PL_DHASH_MIN_SIZE);
  }
  explicit nsTHashtable(uint32_t aInitSize)
  {
    Init(aInitSize);
  }

  /**
   * destructor, cleans up and deallocates
   */
  ~nsTHashtable();

  nsTHashtable(nsTHashtable<EntryType>&& aOther);

  /**
   * Return the generation number for the table. This increments whenever
   * the table data items are moved.
   */
  uint32_t GetGeneration() const { return mTable.generation; }

  /**
   * KeyType is typedef'ed for ease of use.
   */
  typedef typename EntryType::KeyType KeyType;

  /**
   * KeyTypePointer is typedef'ed for ease of use.
   */
  typedef typename EntryType::KeyTypePointer KeyTypePointer;

  /**
   * Return the number of entries in the table.
   * @return    number of entries
   */
  uint32_t Count() const { return mTable.entryCount; }

  /**
   * Get the entry associated with a key.
   * @param     aKey the key to retrieve
   * @return    pointer to the entry class, if the key exists; nullptr if the
   *            key doesn't exist
   */
  EntryType* GetEntry(KeyType aKey) const
  {
    NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");
  
    EntryType* entry =
      reinterpret_cast<EntryType*>
                      (PL_DHashTableOperate(
                            const_cast<PLDHashTable*>(&mTable),
                            EntryType::KeyToPointer(aKey),
                            PL_DHASH_LOOKUP));
    return PL_DHASH_ENTRY_IS_BUSY(entry) ? entry : nullptr;
  }

  /**
   * Return true if an entry for the given key exists, false otherwise.
   * @param     aKey the key to retrieve
   * @return    true if the key exists, false if the key doesn't exist
   */
  bool Contains(KeyType aKey) const
  {
    return !!GetEntry(aKey);
  }

  /**
   * Get the entry associated with a key, or create a new entry,
   * @param     aKey the key to retrieve
   * @return    pointer to the entry class retreived; nullptr only if memory
                can't be allocated
   */
  EntryType* PutEntry(KeyType aKey)
  {
    EntryType* e = PutEntry(aKey, fallible_t());
    if (!e)
      NS_ABORT_OOM(mTable.entrySize * mTable.entryCount);
    return e;
  }

  EntryType* PutEntry(KeyType aKey, const fallible_t&) NS_WARN_UNUSED_RESULT
  {
    NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");

    return static_cast<EntryType*>
                      (PL_DHashTableOperate(
                            &mTable,
                            EntryType::KeyToPointer(aKey),
                            PL_DHASH_ADD));
  }

  /**
   * Remove the entry associated with a key.
   * @param     aKey of the entry to remove
   */
  void RemoveEntry(KeyType aKey)
  {
    NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");

    PL_DHashTableOperate(&mTable,
                         EntryType::KeyToPointer(aKey),
                         PL_DHASH_REMOVE);
  }

  /**
   * Remove the entry associated with a key, but don't resize the hashtable.
   * This is a low-level method, and is not recommended unless you know what
   * you're doing and you need the extra performance. This method can be used
   * during enumeration, while RemoveEntry() cannot.
   * @param aEntry   the entry-pointer to remove (obtained from GetEntry or
   *                 the enumerator
   */
  void RawRemoveEntry(EntryType* aEntry)
  {
    PL_DHashTableRawRemove(&mTable, aEntry);
  }

  /**
   * client must provide an <code>Enumerator</code> function for
   *   EnumerateEntries
   * @param     aEntry the entry being enumerated
   * @param     userArg passed unchanged from <code>EnumerateEntries</code>
   * @return    combination of flags
   *            @link PLDHashOperator::PL_DHASH_NEXT PL_DHASH_NEXT @endlink ,
   *            @link PLDHashOperator::PL_DHASH_STOP PL_DHASH_STOP @endlink ,
   *            @link PLDHashOperator::PL_DHASH_REMOVE PL_DHASH_REMOVE @endlink
   */
  typedef PLDHashOperator (* Enumerator)(EntryType* aEntry, void* userArg);

  /**
   * Enumerate all the entries of the function.
   * @param     enumFunc the <code>Enumerator</code> function to call
   * @param     userArg a pointer to pass to the
   *            <code>Enumerator</code> function
   * @return    the number of entries actually enumerated
   */
  uint32_t EnumerateEntries(Enumerator enumFunc, void* userArg)
  {
    NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");
    
    s_EnumArgs args = { enumFunc, userArg };
    return PL_DHashTableEnumerate(&mTable, s_EnumStub, &args);
  }

  /**
   * remove all entries, return hashtable to "pristine" state ;)
   */
  void Clear()
  {
    NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");

    PL_DHashTableEnumerate(&mTable, PL_DHashStubEnumRemove, nullptr);
  }

  /**
   * client must provide a <code>SizeOfEntryExcludingThisFun</code> function for
   *   SizeOfExcludingThis.
   * @param     aEntry the entry being enumerated
   * @param     mallocSizeOf the function used to measure heap-allocated blocks
   * @param     arg passed unchanged from <code>SizeOf{In,Ex}cludingThis</code>
   * @return    summed size of the things pointed to by the entries
   */
  typedef size_t (* SizeOfEntryExcludingThisFun)(EntryType* aEntry,
                                                 mozilla::MallocSizeOf mallocSizeOf,
                                                 void *arg);

  /**
   * Measure the size of the table's entry storage, and if
   * |sizeOfEntryExcludingThis| is non-nullptr, measure the size of things
   * pointed to by entries.
   * 
   * @param     sizeOfEntryExcludingThis the
   *            <code>SizeOfEntryExcludingThisFun</code> function to call
   * @param     mallocSizeOf the function used to measure heap-allocated blocks
   * @param     userArg a pointer to pass to the
   *            <code>SizeOfEntryExcludingThisFun</code> function
   * @return    the summed size of all the entries
   */
  size_t SizeOfExcludingThis(SizeOfEntryExcludingThisFun sizeOfEntryExcludingThis,
                             mozilla::MallocSizeOf mallocSizeOf,
                             void *userArg = nullptr) const
  {
    if (sizeOfEntryExcludingThis) {
      s_SizeOfArgs args = { sizeOfEntryExcludingThis, userArg };
      return PL_DHashTableSizeOfExcludingThis(&mTable, s_SizeOfStub, mallocSizeOf, &args);
    }
    return PL_DHashTableSizeOfExcludingThis(&mTable, nullptr, mallocSizeOf);
  }

  /**
   * Like SizeOfExcludingThis, but includes sizeof(*this).
   */
  size_t SizeOfIncludingThis(SizeOfEntryExcludingThisFun sizeOfEntryExcludingThis,
                             mozilla::MallocSizeOf mallocSizeOf,
                             void *userArg = nullptr) const
  {
    return mallocSizeOf(this) +
        SizeOfExcludingThis(sizeOfEntryExcludingThis, mallocSizeOf, userArg);
  }

#ifdef DEBUG
  /**
   * Mark the table as constant after initialization.
   *
   * This will prevent assertions when a read-only hash is accessed on multiple
   * threads without synchronization.
   */
  void MarkImmutable()
  {
    NS_ASSERTION(mTable.entrySize, "nsTHashtable was not initialized properly.");

    PL_DHashMarkTableImmutable(&mTable);
  }
#endif

protected:
  PLDHashTable mTable;

  static const void* s_GetKey(PLDHashTable    *table,
                              PLDHashEntryHdr *entry);

  static PLDHashNumber s_HashKey(PLDHashTable *table,
                                 const void   *key);

  static bool s_MatchEntry(PLDHashTable           *table,
                             const PLDHashEntryHdr  *entry,
                             const void             *key);
  
  static void s_CopyEntry(PLDHashTable          *table,
                          const PLDHashEntryHdr *from,
                          PLDHashEntryHdr       *to);
  
  static void s_ClearEntry(PLDHashTable *table,
                           PLDHashEntryHdr *entry);

  static bool s_InitEntry(PLDHashTable     *table,
                            PLDHashEntryHdr  *entry,
                            const void       *key);

  /**
   * passed internally during enumeration.  Allocated on the stack.
   *
   * @param userFunc the Enumerator function passed to
   *   EnumerateEntries by the client
   * @param userArg the userArg passed unaltered
   */
  struct s_EnumArgs
  {
    Enumerator userFunc;
    void* userArg;
  };
  
  static PLDHashOperator s_EnumStub(PLDHashTable    *table,
                                    PLDHashEntryHdr *entry,
                                    uint32_t         number,
                                    void            *arg);

  /**
   * passed internally during sizeOf counting.  Allocated on the stack.
   *
   * @param userFunc the SizeOfEntryExcludingThisFun passed to
   *                 SizeOf{In,Ex}cludingThis by the client
   * @param userArg the userArg passed unaltered
   */
  struct s_SizeOfArgs
  {
    SizeOfEntryExcludingThisFun userFunc;
    void* userArg;
  };
  
  static size_t s_SizeOfStub(PLDHashEntryHdr *entry,
                             mozilla::MallocSizeOf mallocSizeOf,
                             void *arg);

private:
  // copy constructor, not implemented
  nsTHashtable(nsTHashtable<EntryType>& toCopy) MOZ_DELETE;

  /**
   * Initialize the table.
   * @param initSize the initial number of buckets in the hashtable
   */
  void Init(uint32_t aInitSize);

  // assignment operator, not implemented
  nsTHashtable<EntryType>& operator= (nsTHashtable<EntryType>& toEqual) MOZ_DELETE;
};

//
// template definitions
//

template<class EntryType>
nsTHashtable<EntryType>::nsTHashtable(
  nsTHashtable<EntryType>&& aOther)
  : mTable(mozilla::Move(aOther.mTable))
{
  // aOther shouldn't touch mTable after this, because we've stolen the table's
  // pointers but not overwitten them.
  MOZ_MAKE_MEM_UNDEFINED(&aOther.mTable, sizeof(aOther.mTable));

  // Indicate that aOther is not initialized.  This will make its destructor a
  // nop, which is what we want.
  aOther.mTable.entrySize = 0;
}

template<class EntryType>
nsTHashtable<EntryType>::~nsTHashtable()
{
  if (mTable.entrySize)
    PL_DHashTableFinish(&mTable);
}

template<class EntryType>
void
nsTHashtable<EntryType>::Init(uint32_t aInitSize)
{
  static const PLDHashTableOps sOps =
  {
    ::PL_DHashAllocTable,
    ::PL_DHashFreeTable,
    s_HashKey,
    s_MatchEntry,
    EntryType::ALLOW_MEMMOVE ? ::PL_DHashMoveEntryStub : s_CopyEntry,
    s_ClearEntry,
    ::PL_DHashFinalizeStub,
    s_InitEntry
  };

  PL_DHashTableInit(&mTable, &sOps, nullptr, sizeof(EntryType), aInitSize);
}

// static definitions

template<class EntryType>
PLDHashNumber
nsTHashtable<EntryType>::s_HashKey(PLDHashTable  *table,
                                   const void    *key)
{
  return EntryType::HashKey(reinterpret_cast<const KeyTypePointer>(key));
}

template<class EntryType>
bool
nsTHashtable<EntryType>::s_MatchEntry(PLDHashTable          *table,
                                      const PLDHashEntryHdr *entry,
                                      const void            *key)
{
  return ((const EntryType*) entry)->KeyEquals(
    reinterpret_cast<const KeyTypePointer>(key));
}

template<class EntryType>
void
nsTHashtable<EntryType>::s_CopyEntry(PLDHashTable          *table,
                                     const PLDHashEntryHdr *from,
                                     PLDHashEntryHdr       *to)
{
  EntryType* fromEntry =
    const_cast<EntryType*>(reinterpret_cast<const EntryType*>(from));

  new(to) EntryType(mozilla::Move(*fromEntry));

  fromEntry->~EntryType();
}

template<class EntryType>
void
nsTHashtable<EntryType>::s_ClearEntry(PLDHashTable    *table,
                                      PLDHashEntryHdr *entry)
{
  reinterpret_cast<EntryType*>(entry)->~EntryType();
}

template<class EntryType>
bool
nsTHashtable<EntryType>::s_InitEntry(PLDHashTable    *table,
                                     PLDHashEntryHdr *entry,
                                     const void      *key)
{
  new(entry) EntryType(reinterpret_cast<KeyTypePointer>(key));
  return true;
}

template<class EntryType>
PLDHashOperator
nsTHashtable<EntryType>::s_EnumStub(PLDHashTable    *table,
                                    PLDHashEntryHdr *entry,
                                    uint32_t         number,
                                    void            *arg)
{
  // dereferences the function-pointer to the user's enumeration function
  return (* reinterpret_cast<s_EnumArgs*>(arg)->userFunc)(
    reinterpret_cast<EntryType*>(entry),
    reinterpret_cast<s_EnumArgs*>(arg)->userArg);
}

template<class EntryType>
size_t
nsTHashtable<EntryType>::s_SizeOfStub(PLDHashEntryHdr *entry,
                                      mozilla::MallocSizeOf mallocSizeOf,
                                      void *arg)
{
  // dereferences the function-pointer to the user's enumeration function
  return (* reinterpret_cast<s_SizeOfArgs*>(arg)->userFunc)(
    reinterpret_cast<EntryType*>(entry),
    mallocSizeOf,
    reinterpret_cast<s_SizeOfArgs*>(arg)->userArg);
}

class nsCycleCollectionTraversalCallback;

struct MOZ_STACK_CLASS nsTHashtableCCTraversalData
{
  nsTHashtableCCTraversalData(nsCycleCollectionTraversalCallback& aCallback,
                              const char* aName,
                              uint32_t aFlags)
  : mCallback(aCallback),
    mName(aName),
    mFlags(aFlags)
  {
  }

  nsCycleCollectionTraversalCallback& mCallback;
  const char* mName;
  uint32_t mFlags;
};

template <class EntryType>
PLDHashOperator
ImplCycleCollectionTraverse_EnumFunc(EntryType *aEntry,
                                     void* aUserData)
{
  auto userData = static_cast<nsTHashtableCCTraversalData*>(aUserData);

  ImplCycleCollectionTraverse(userData->mCallback,
                              *aEntry,
                              userData->mName,
                              userData->mFlags);
  return PL_DHASH_NEXT;
}

template <class EntryType>
inline void
ImplCycleCollectionUnlink(nsTHashtable<EntryType>& aField)
{
  aField.Clear();
}

template <class EntryType>
inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            nsTHashtable<EntryType>& aField,
                            const char* aName,
                            uint32_t aFlags = 0)
{
  nsTHashtableCCTraversalData userData(aCallback, aName, aFlags);

  aField.EnumerateEntries(ImplCycleCollectionTraverse_EnumFunc<EntryType>,
                          &userData);
}

#endif // nsTHashtable_h__
