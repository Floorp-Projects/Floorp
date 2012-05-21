/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTHashtable_h__
#define nsTHashtable_h__

#include "nscore.h"
#include "pldhash.h"
#include "nsDebug.h"
#include NEW_H
#include "mozilla/fallible.h"

// helper function for nsTHashtable::Clear()
NS_COM_GLUE PLDHashOperator
PL_DHashStubEnumRemove(PLDHashTable    *table,
                       PLDHashEntryHdr *entry,
                       PRUint32         ordinal,
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
 *     // this should either be a simple datatype (PRUint32, nsISupports*) or
 *     // a const reference (const nsAString&)
 *     typedef something KeyType;
 *     // KeyTypePointer is the pointer-version of KeyType, because pldhash.h
 *     // requires keys to cast to <code>const void*</code>
 *     typedef const something* KeyTypePointer;
 *
 *     EntryType(KeyTypePointer aKey);
 *
 *     // the copy constructor must be defined, even if AllowMemMove() == true
 *     // or you will cause link errors!
 *     EntryType(const EntryType& aEnt);
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
 *     enum { ALLOW_MEMMOVE = PR_(TRUE or FALSE) };
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
  /**
   * A dummy constructor; you must call Init() before using this class.
   */
  nsTHashtable();

  /**
   * destructor, cleans up and deallocates
   */
  ~nsTHashtable();

  /**
   * Initialize the table.  This function must be called before any other
   * class operations.  This can fail due to OOM conditions.
   * @param initSize the initial number of buckets in the hashtable, default 16
   * @return true if the class was initialized properly.
   */
  void Init(PRUint32 initSize = PL_DHASH_MIN_SIZE)
  {
    if (!Init(initSize, fallible_t()))
      NS_RUNTIMEABORT("OOM");
  }
  bool Init(const fallible_t&) NS_WARN_UNUSED_RESULT
  { return Init(PL_DHASH_MIN_SIZE, fallible_t()); }
  bool Init(PRUint32 initSize, const fallible_t&) NS_WARN_UNUSED_RESULT;

  /**
   * Check whether the table has been initialized. This can be useful for static hashtables.
   * @return the initialization state of the class.
   */
  bool IsInitialized() const { return !!mTable.entrySize; }

  /**
   * Return the generation number for the table. This increments whenever
   * the table data items are moved.
   */
  PRUint32 GetGeneration() const { return mTable.generation; }

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
  PRUint32 Count() const { return mTable.entryCount; }

  /**
   * Get the entry associated with a key.
   * @param     aKey the key to retrieve
   * @return    pointer to the entry class, if the key exists; nsnull if the
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
    return PL_DHASH_ENTRY_IS_BUSY(entry) ? entry : nsnull;
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
   * @return    pointer to the entry class retreived; nsnull only if memory
                can't be allocated
   */
  EntryType* PutEntry(KeyType aKey)
  {
    EntryType* e = PutEntry(aKey, fallible_t());
    if (!e)
      NS_RUNTIMEABORT("OOM");
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
  PRUint32 EnumerateEntries(Enumerator enumFunc, void* userArg)
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

    PL_DHashTableEnumerate(&mTable, PL_DHashStubEnumRemove, nsnull);
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
                                                 nsMallocSizeOfFun mallocSizeOf,
                                                 void *arg);

  /**
   * Measure the size of the table's entry storage, and if
   * |sizeOfEntryExcludingThis| is non-NULL, measure the size of things pointed
   * to by entries.
   * 
   * @param     sizeOfEntryExcludingThis the
   *            <code>SizeOfEntryExcludingThisFun</code> function to call
   * @param     mallocSizeOf the function used to measure heap-allocated blocks
   * @param     userArg a pointer to pass to the
   *            <code>SizeOfEntryExcludingThisFun</code> function
   * @return    the summed size of all the entries
   */
  size_t SizeOfExcludingThis(SizeOfEntryExcludingThisFun sizeOfEntryExcludingThis,
                             nsMallocSizeOfFun mallocSizeOf, void *userArg = NULL) const
  {
    if (!IsInitialized()) {
      return 0;
    }
    if (sizeOfEntryExcludingThis) {
      s_SizeOfArgs args = { sizeOfEntryExcludingThis, userArg };
      return PL_DHashTableSizeOfExcludingThis(&mTable, s_SizeOfStub, mallocSizeOf, &args);
    }
    return PL_DHashTableSizeOfExcludingThis(&mTable, NULL, mallocSizeOf);
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
                                    PRUint32         number,
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
                             nsMallocSizeOfFun mallocSizeOf,
                             void *arg);

private:
  // copy constructor, not implemented
  nsTHashtable(nsTHashtable<EntryType>& toCopy);

  // assignment operator, not implemented
  nsTHashtable<EntryType>& operator= (nsTHashtable<EntryType>& toEqual);
};

//
// template definitions
//

template<class EntryType>
nsTHashtable<EntryType>::nsTHashtable()
{
  // entrySize is our "I'm initialized" indicator
  mTable.entrySize = 0;
}

template<class EntryType>
nsTHashtable<EntryType>::~nsTHashtable()
{
  if (mTable.entrySize)
    PL_DHashTableFinish(&mTable);
}

template<class EntryType>
bool
nsTHashtable<EntryType>::Init(PRUint32 initSize, const fallible_t&)
{
  if (mTable.entrySize)
  {
    NS_ERROR("nsTHashtable::Init() should not be called twice.");
    return true;
  }

  static PLDHashTableOps sOps = 
  {
    ::PL_DHashAllocTable,
    ::PL_DHashFreeTable,
    s_HashKey,
    s_MatchEntry,
    ::PL_DHashMoveEntryStub,
    s_ClearEntry,
    ::PL_DHashFinalizeStub,
    s_InitEntry
  };

  if (!EntryType::ALLOW_MEMMOVE)
  {
    sOps.moveEntry = s_CopyEntry;
  }
  
  if (!PL_DHashTableInit(&mTable, &sOps, nsnull, sizeof(EntryType), initSize))
  {
    // if failed, reset "flag"
    mTable.entrySize = 0;
    return false;
  }

  return true;
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

  new(to) EntryType(*fromEntry);

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
                                    PRUint32         number,
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
                                      nsMallocSizeOfFun mallocSizeOf,
                                      void *arg)
{
  // dereferences the function-pointer to the user's enumeration function
  return (* reinterpret_cast<s_SizeOfArgs*>(arg)->userFunc)(
    reinterpret_cast<EntryType*>(entry),
    mallocSizeOf,
    reinterpret_cast<s_SizeOfArgs*>(arg)->userArg);
}

#endif // nsTHashtable_h__
