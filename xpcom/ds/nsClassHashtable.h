/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClassHashtable_h__
#define nsClassHashtable_h__

#include "mozilla/Move.h"
#include "nsBaseHashtable.h"
#include "nsHashKeys.h"
#include "nsAutoPtr.h"

/**
 * templated hashtable class maps keys to C++ object pointers.
 * See nsBaseHashtable for complete declaration.
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param Class the class-type being wrapped
 * @see nsInterfaceHashtable, nsClassHashtable
 */
template<class KeyClass, class T>
class nsClassHashtable
  : public nsBaseHashtable<KeyClass, nsAutoPtr<T>, T*>
{
public:
  typedef typename KeyClass::KeyType KeyType;
  typedef T* UserDataType;
  typedef nsBaseHashtable<KeyClass, nsAutoPtr<T>, T*> base_type;

  using base_type::IsEmpty;

  nsClassHashtable() {}
  explicit nsClassHashtable(uint32_t aInitLength)
    : nsBaseHashtable<KeyClass, nsAutoPtr<T>, T*>(aInitLength)
  {
  }

  /**
   * Looks up aKey in the hash table. If it doesn't exist a new object of
   * KeyClass will be created (using the arguments provided) and then returned.
   */
  template<typename... Args>
  UserDataType LookupOrAdd(KeyType aKey, Args&&... aConstructionArgs);

  /**
   * @copydoc nsBaseHashtable::Get
   * @param aData if the key doesn't exist, pData will be set to nullptr.
   */
  bool Get(KeyType aKey, UserDataType* aData) const;

  /**
   * @copydoc nsBaseHashtable::Get
   * @returns nullptr if the key is not present.
   */
  UserDataType Get(KeyType aKey) const;

  /**
   * Remove the entry for the given key from the hashtable and return it in
   * aOut.  If the key is not in the hashtable, aOut's pointer is set to
   * nullptr.
   *
   * Normally, an entry is deleted when it's removed from an nsClassHashtable,
   * but this function transfers ownership of the entry back to the caller
   * through aOut -- the entry will be deleted when aOut goes out of scope.
   *
   * @param aKey the key to get and remove from the hashtable
   */
  void RemoveAndForget(KeyType aKey, nsAutoPtr<T>& aOut);

  struct EntryPtr {
  private:
    typename base_type::EntryType& mEntry;
    // For debugging purposes
#ifdef DEBUG
    base_type& mTable;
    uint32_t mTableGeneration;
#endif

  public:
    EntryPtr(base_type& aTable, typename base_type::EntryType* aEntry)
      : mEntry(*aEntry)
#ifdef DEBUG
      , mTable(aTable)
      , mTableGeneration(aTable.GetGeneration())
#endif
    {
    }
    ~EntryPtr()
    {
      MOZ_ASSERT(mEntry.mData, "Entry should have been added by now");
    }

    explicit operator bool() const
    {
      // Is there something stored in the table already?
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      return !!mEntry.mData;
    }

    template <class F>
    T* OrInsert(F func)
    {
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      if (!mEntry.mData) {
        mEntry.mData = func();
      }
      return mEntry.mData;
    }
  };

  /**
   * Looks up aKey in the hashtable and returns an object that allows you to
   * insert a new entry into the hashtable for that key if an existing entry
   * isn't found for it.
   *
   * A typical usage of this API looks like this:
   *
   *   auto insertedValue = table.LookupForAdd(key).OrInsert([]() {
   *     return newValue;
   *   });
   *
   *   auto p = table.LookupForAdd(key);
   *   if (p) {
   *     // The entry already existed in the table.
   *     DoSomething();
   *   } else {
   *     // An existing entry wasn't found, store a new entry in the hashtable.
   *     p.OrInsert([]() { return newValue; });
   *   }
   *
   * We ensure that the hashtable isn't modified before OrInsert() is called.
   * This is useful for cases where you want to insert a new entry into the
   * hashtable if one doesn't exist before but would like to avoid two hashtable
   * lookups.
   */
  MOZ_MUST_USE EntryPtr LookupForAdd(KeyType aKey);

  /**
   * Looks up aKey in the hashtable and if found calls the given callback
   * aFunction with the value.  If the callback returns true then the entry
   * is removed.  If aKey doesn't exist nothing happens.
   * The hashtable must not be modified in the callback function.
   *
   * A typical usage of this API looks like this:
   *
   *   table.LookupRemoveIf(key, [](T* aValue) {
   *     // ... do stuff using aValue ...
   *     return aValue->IsEmpty(); // or some other condition to remove it
   *   });
   *
   * This is useful for cases where you want to lookup and possibly modify
   * the value and then maybe remove the entry but would like to avoid two
   * hashtable lookups.
   */
  template<class F>
  void LookupRemoveIf(KeyType aKey, F aFunction)
  {
#ifdef DEBUG
    auto tableGeneration = base_type::GetGeneration();
#endif
    typename base_type::EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return;
    }
    bool shouldRemove = aFunction(ent->mData);
    MOZ_ASSERT(tableGeneration == base_type::GetGeneration(),
               "hashtable was modified by the LookupRemoveIf callback!");
    if (shouldRemove) {
      this->RemoveEntry(ent);
    }
  }
};

//
// nsClassHashtable definitions
//

template<class KeyClass, class T>
template<typename... Args>
T*
nsClassHashtable<KeyClass, T>::LookupOrAdd(KeyType aKey,
                                           Args&&... aConstructionArgs)
{
  typename base_type::EntryType* ent = this->PutEntry(aKey);
  if (!ent->mData) {
    ent->mData = new T(mozilla::Forward<Args>(aConstructionArgs)...);
  }
  return ent->mData;
}

template<class KeyClass, class T>
typename nsClassHashtable<KeyClass, T>::EntryPtr
nsClassHashtable<KeyClass, T>::LookupForAdd(KeyType aKey)
{
  typename base_type::EntryType* ent = this->PutEntry(aKey);
  return EntryPtr(*this, ent);
}

template<class KeyClass, class T>
bool
nsClassHashtable<KeyClass, T>::Get(KeyType aKey, T** aRetVal) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);

  if (ent) {
    if (aRetVal) {
      *aRetVal = ent->mData;
    }

    return true;
  }

  if (aRetVal) {
    *aRetVal = nullptr;
  }

  return false;
}

template<class KeyClass, class T>
T*
nsClassHashtable<KeyClass, T>::Get(KeyType aKey) const
{
  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent) {
    return nullptr;
  }

  return ent->mData;
}

template<class KeyClass, class T>
void
nsClassHashtable<KeyClass, T>::RemoveAndForget(KeyType aKey, nsAutoPtr<T>& aOut)
{
  aOut = nullptr;

  typename base_type::EntryType* ent = this->GetEntry(aKey);
  if (!ent) {
    return;
  }

  // Transfer ownership from ent->mData into aOut.
  aOut = mozilla::Move(ent->mData);

  this->RemoveEntry(ent);
}

#endif // nsClassHashtable_h__
