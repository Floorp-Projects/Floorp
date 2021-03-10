/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseHashtable_h__
#define nsBaseHashtable_h__

#include <functional>
#include <utility>

#include "mozilla/Maybe.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Result.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsHashtablesFwd.h"
#include "nsTHashtable.h"

namespace mozilla::detail {

template <typename SmartPtr>
struct SmartPtrTraits {
  static constexpr bool IsSmartPointer = false;
  static constexpr bool IsRefCounted = false;
};

template <typename Pointee>
struct SmartPtrTraits<UniquePtr<Pointee>> {
  static constexpr bool IsSmartPointer = true;
  static constexpr bool IsRefCounted = false;
  using SmartPointerType = UniquePtr<Pointee>;
  using PointeeType = Pointee;
  using RawPointerType = Pointee*;
  template <typename U>
  using OtherSmartPtrType = UniquePtr<U>;

  template <typename U, typename... Args>
  static SmartPointerType NewObject(Args&&... aConstructionArgs) {
    return mozilla::MakeUnique<U>(std::forward<Args>(aConstructionArgs)...);
  }
};

template <typename Pointee>
struct SmartPtrTraits<RefPtr<Pointee>> {
  static constexpr bool IsSmartPointer = true;
  static constexpr bool IsRefCounted = true;
  using SmartPointerType = RefPtr<Pointee>;
  using PointeeType = Pointee;
  using RawPointerType = Pointee*;
  template <typename U>
  using OtherSmartPtrType = RefPtr<U>;

  template <typename U, typename... Args>
  static SmartPointerType NewObject(Args&&... aConstructionArgs) {
    return MakeRefPtr<U>(std::forward<Args>(aConstructionArgs)...);
  }
};

template <typename Pointee>
struct SmartPtrTraits<nsCOMPtr<Pointee>> {
  static constexpr bool IsSmartPointer = true;
  static constexpr bool IsRefCounted = true;
  using SmartPointerType = nsCOMPtr<Pointee>;
  using PointeeType = Pointee;
  using RawPointerType = Pointee*;
  template <typename U>
  using OtherSmartPtrType = nsCOMPtr<U>;

  template <typename U, typename... Args>
  static SmartPointerType NewObject(Args&&... aConstructionArgs) {
    return MakeRefPtr<U>(std::forward<Args>(aConstructionArgs)...);
  }
};

// XXX Add SafeRefPtr specialization

template <class T>
T* PtrGetWeak(T* aPtr) {
  return aPtr;
}

template <class T>
T* PtrGetWeak(const RefPtr<T>& aPtr) {
  return aPtr.get();
}

template <class T>
T* PtrGetWeak(const nsCOMPtr<T>& aPtr) {
  return aPtr.get();
}

template <class T>
T* PtrGetWeak(const UniquePtr<T>& aPtr) {
  return aPtr.get();
}

}  // namespace mozilla::detail

/**
 * Data type conversion helper that is used to wrap and unwrap the specified
 * DataType.
 */
template <class DataType, class UserDataType>
class nsDefaultConverter {
 public:
  /**
   * Maps the storage DataType to the exposed UserDataType.
   */
  static UserDataType Unwrap(DataType& src) { return UserDataType(src); }

  /**
   * Const ref variant used for example with nsCOMPtr wrappers.
   */
  static DataType Wrap(const UserDataType& src) { return DataType(src); }

  /**
   * Generic conversion, this is useful for things like already_AddRefed.
   */
  template <typename U>
  static DataType Wrap(U&& src) {
    return std::forward<U>(src);
  }

  template <typename U>
  static UserDataType Unwrap(U&& src) {
    return std::forward<U>(src);
  }
};

/**
 * the private nsTHashtable::EntryType class used by nsBaseHashtable
 * @see nsTHashtable for the specification of this class
 * @see nsBaseHashtable for template parameters
 */
template <class KeyClass, class DataType>
class nsBaseHashtableET : public KeyClass {
 public:
  const DataType& GetData() const { return mData; }
  DataType* GetModifiableData() { return &mData; }
  template <typename U>
  void SetData(U&& aData) {
    mData = std::forward<U>(aData);
  }

  decltype(auto) GetWeak() const {
    return mozilla::detail::PtrGetWeak(GetData());
  }

 private:
  DataType mData;
  friend class nsTHashtable<nsBaseHashtableET<KeyClass, DataType>>;
  template <typename KeyClassX, typename DataTypeX, typename UserDataTypeX,
            typename ConverterX>
  friend class nsBaseHashtable;

  typedef typename KeyClass::KeyType KeyType;
  typedef typename KeyClass::KeyTypePointer KeyTypePointer;

  template <typename... Args>
  explicit nsBaseHashtableET(KeyTypePointer aKey, Args&&... aArgs);
  nsBaseHashtableET(nsBaseHashtableET<KeyClass, DataType>&& aToMove) = default;
  ~nsBaseHashtableET() = default;
};

/**
 * Templated hashtable. Usually, this isn't instantiated directly but through
 * its sub-class templates nsInterfaceHashtable, nsClassHashtable,
 * nsRefPtrHashtable and nsTHashMap.
 *
 * Originally, UserDataType used to be the only type exposed to the user in the
 * public member function signatures (hence its name), but this has proven to
 * inadequate over time. Now, UserDataType is only exposed in by-value
 * getter member functions that are called *Get*. Member functions that provide
 * access to the DataType are called Lookup rather than Get. Note that this rule
 * does not apply to nsRefPtrHashtable and nsInterfaceHashtable, as they are
 * provide a similar interface, but are no genuine sub-classes of
 * nsBaseHashtable.
 *
 * @param KeyClass a wrapper-class for the hashtable key, see nsHashKeys.h
 *   for a complete specification.
 * @param DataType the datatype stored in the hashtable,
 *   for example, uint32_t or nsCOMPtr.
 * @param UserDataType the datatype returned from the by-value getter member
 *   functions (named *Get*), for example uint32_t or nsISupports*
 * @param Converter that is used to map from DataType to UserDataType. A
 *   default converter is provided that assumes implicit conversion is an
 *   option.
 */
template <class KeyClass, class DataType, class UserDataType, class Converter>
class nsBaseHashtable
    : protected nsTHashtable<nsBaseHashtableET<KeyClass, DataType>> {
  using Base = nsTHashtable<nsBaseHashtableET<KeyClass, DataType>>;
  typedef mozilla::fallible_t fallible_t;

 public:
  typedef typename KeyClass::KeyType KeyType;
  typedef nsBaseHashtableET<KeyClass, DataType> EntryType;

  using nsTHashtable<EntryType>::Contains;
  using nsTHashtable<EntryType>::GetGeneration;
  using nsTHashtable<EntryType>::SizeOfExcludingThis;
  using nsTHashtable<EntryType>::SizeOfIncludingThis;

  nsBaseHashtable() = default;
  explicit nsBaseHashtable(uint32_t aInitLength)
      : nsTHashtable<EntryType>(aInitLength) {}

  /**
   * Return the number of entries in the table.
   * @return    number of entries
   */
  [[nodiscard]] uint32_t Count() const {
    return nsTHashtable<EntryType>::Count();
  }

  /**
   * Return whether the table is empty.
   * @return    whether empty
   */
  [[nodiscard]] bool IsEmpty() const {
    return nsTHashtable<EntryType>::IsEmpty();
  }

  /**
   * Get the value, returning a flag indicating the presence of the entry in
   * the table.
   *
   * @param aKey the key to retrieve
   * @param aData data associated with this key will be placed at this pointer.
   *        If you only need to check if the key exists, aData may be null.
   * @return true if the key exists. If key does not exist, aData is not
   *   modified.
   *
   * @attention As opposed to Remove, this does not assign a value to *aData if
   * no entry is present! (And also as opposed to the member function Get with
   * the same signature that nsClassHashtable defines and hides this one.)
   */
  [[nodiscard]] bool Get(KeyType aKey, UserDataType* aData) const {
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return false;
    }

    if (aData) {
      *aData = Converter::Unwrap(ent->mData);
    }

    return true;
  }

  /**
   * Get the value, returning a zero-initialized POD or a default-initialized
   * object if the entry is not present in the table.
   *
   * This overload can only be used if UserDataType is default-constructible.
   * Use the double-argument Get or MaybeGet with non-default-constructible
   * UserDataType.
   *
   * @param aKey the key to retrieve
   * @return The found value, or UserDataType{} if no entry was found with the
   *         given key.
   * @note If zero/default-initialized values are stored in the table, it is
   *       not possible to distinguish between such a value and a missing entry.
   */
  [[nodiscard]] UserDataType Get(KeyType aKey) const {
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return UserDataType{};
    }

    return Converter::Unwrap(ent->mData);
  }

  /**
   * Get the value, returning Nothing if the entry is not present in the table.
   *
   * @param aKey the key to retrieve
   * @return The found value wrapped in a Maybe, or Nothing if no entry was
   *         found with the given key.
   */
  [[nodiscard]] mozilla::Maybe<UserDataType> MaybeGet(KeyType aKey) const {
    EntryType* ent = this->GetEntry(aKey);
    if (!ent) {
      return mozilla::Nothing();
    }

    return mozilla::Some(Converter::Unwrap(ent->mData));
  }

  using SmartPtrTraits = mozilla::detail::SmartPtrTraits<DataType>;

  /**
   * Looks up aKey in the hash table. If it doesn't exist a new object of
   * SmartPtrTraits::PointeeType will be created (using the arguments provided)
   * and then returned.
   *
   * \note This can only be instantiated if DataType is a smart pointer.
   */
  template <typename... Args>
  auto GetOrInsertNew(KeyType aKey, Args&&... aConstructionArgs) {
    static_assert(
        SmartPtrTraits::IsSmartPointer,
        "GetOrInsertNew can only be used with smart pointer data types");
    return LookupOrInsertWith(std::move(aKey),
                              [&] {
                                return SmartPtrTraits::template NewObject<
                                    typename SmartPtrTraits::PointeeType>(
                                    std::forward<Args>(aConstructionArgs)...);
                              })
        .get();
  }

  /**
   * Add aKey to the table if not already present, and return a reference to its
   * value.  If aKey is not already in the table then the a default-constructed
   * or the provided value aData is used.
   *
   * If the arguments are non-trivial to provide, consider using
   * LookupOrInsertWith instead.
   */
  template <typename... Args>
  DataType& LookupOrInsert(const KeyType& aKey, Args&&... aArgs) {
    return WithEntryHandle(aKey, [&](auto entryHandle) -> DataType& {
      return entryHandle.OrInsert(std::forward<Args>(aArgs)...);
    });
  }

  /**
   * Add aKey to the table if not already present, and return a reference to its
   * value.  If aKey is not already in the table then the value is
   * constructed using the given factory.
   */
  template <typename F>
  DataType& LookupOrInsertWith(const KeyType& aKey, F&& aFunc) {
    return WithEntryHandle(aKey, [&aFunc](auto entryHandle) -> DataType& {
      return entryHandle.OrInsertWith(std::forward<F>(aFunc));
    });
  }

  /**
   * Add aKey to the table if not already present, and return a reference to its
   * value.  If aKey is not already in the table then the value is
   * constructed using the given factory.
   */
  template <typename F>
  [[nodiscard]] auto TryLookupOrInsertWith(const KeyType& aKey, F&& aFunc) {
    return WithEntryHandle(
        aKey,
        [&aFunc](auto entryHandle)
            -> mozilla::Result<std::reference_wrapper<DataType>,
                               typename std::invoke_result_t<F>::err_type> {
          if (entryHandle) {
            return std::ref(entryHandle.Data());
          }

          // XXX Use MOZ_TRY after generalizing QM_TRY to mfbt.
          auto res = std::forward<F>(aFunc)();
          if (res.isErr()) {
            return res.propagateErr();
          }
          return std::ref(entryHandle.Insert(res.unwrap()));
        });
  }

  /**
   * If it does not yet, inserts a new entry with the handle's key and the
   * value passed to this function. Otherwise, it updates the entry by the
   * value passed to this function.
   *
   * \tparam U DataType must be implicitly convertible (and assignable) from U
   * \post HasEntry()
   * \param aKey the key to put
   * \param aData the new data
   */
  template <typename U>
  DataType& InsertOrUpdate(KeyType aKey, U&& aData) {
    return WithEntryHandle(aKey, [&aData](auto entryHandle) -> DataType& {
      return entryHandle.InsertOrUpdate(std::forward<U>(aData));
    });
  }

  template <typename U>
  [[nodiscard]] bool InsertOrUpdate(KeyType aKey, U&& aData,
                                    const fallible_t& aFallible) {
    return WithEntryHandle(aKey, aFallible, [&aData](auto maybeEntryHandle) {
      if (!maybeEntryHandle) {
        return false;
      }
      maybeEntryHandle->InsertOrUpdate(std::forward<U>(aData));
      return true;
    });
  }

  /**
   * Remove the entry associated with aKey (if any), _moving_ its current value
   * into *aData.  Return true if found.
   *
   * This overload can only be used if DataType is default-constructible. Use
   * the single-argument Remove or Extract with non-default-constructible
   * DataType.
   *
   * @param aKey the key to remove from the hashtable
   * @param aData where to move the value.  If an entry is not found, *aData
   *              will be assigned a default-constructed value (i.e. reset to
   *              zero or nullptr for primitive types).
   * @return true if an entry for aKey was found (and removed)
   */
  // XXX This should also better be marked nodiscard, but due to
  // nsClassHashtable not guaranteeing non-nullness of entries, it is usually
  // only checked if aData is nullptr in such cases.
  // [[nodiscard]]
  bool Remove(KeyType aKey, DataType* aData) {
    if (auto* ent = this->GetEntry(aKey)) {
      if (aData) {
        *aData = std::move(ent->mData);
      }
      this->RemoveEntry(ent);
      return true;
    }
    if (aData) {
      *aData = std::move(DataType());
    }
    return false;
  }

  /**
   * Remove the entry associated with aKey (if any).  Return true if found.
   *
   * @param aKey the key to remove from the hashtable
   * @return true if an entry for aKey was found (and removed)
   */
  bool Remove(KeyType aKey) {
    if (auto* ent = this->GetEntry(aKey)) {
      this->RemoveEntry(ent);
      return true;
    }

    return false;
  }

  /**
   * Retrieve the value for a key and remove the corresponding entry at
   * the same time.
   *
   * @param aKey the key to retrieve and remove
   * @return the found value, or Nothing if no entry was found with the
   *   given key.
   */
  [[nodiscard]] mozilla::Maybe<DataType> Extract(KeyType aKey) {
    mozilla::Maybe<DataType> value;
    if (EntryType* ent = this->GetEntry(aKey)) {
      value.emplace(std::move(ent->mData));
      this->RemoveEntry(ent);
    }
    return value;
  }

  struct LookupResult {
   private:
    EntryType* mEntry;
    nsBaseHashtable& mTable;
#ifdef DEBUG
    uint32_t mTableGeneration;
#endif

   public:
    LookupResult(EntryType* aEntry, nsBaseHashtable& aTable)
        : mEntry(aEntry),
          mTable(aTable)
#ifdef DEBUG
          ,
          mTableGeneration(aTable.GetGeneration())
#endif
    {
    }

    // Is there something stored in the table?
    explicit operator bool() const {
      MOZ_ASSERT(mTableGeneration == mTable.GetGeneration());
      return mEntry;
    }

    void Remove() {
      if (!*this) {
        return;
      }
      mTable.RemoveEntry(mEntry);
      mEntry = nullptr;
    }

    [[nodiscard]] DataType& Data() {
      MOZ_ASSERT(!!*this, "must have an entry to access its value");
      return mEntry->mData;
    }

    [[nodiscard]] const DataType& Data() const {
      MOZ_ASSERT(!!*this, "must have an entry to access its value");
      return mEntry->mData;
    }

    [[nodiscard]] DataType* DataPtrOrNull() {
      return static_cast<bool>(*this) ? &mEntry->mData : nullptr;
    }

    [[nodiscard]] const DataType* DataPtrOrNull() const {
      return static_cast<bool>(*this) ? &mEntry->mData : nullptr;
    }

    [[nodiscard]] DataType* operator->() { return &Data(); }
    [[nodiscard]] const DataType* operator->() const { return &Data(); }

    [[nodiscard]] DataType& operator*() { return Data(); }
    [[nodiscard]] const DataType& operator*() const { return Data(); }
  };

  /**
   * Removes all entries matching a predicate.
   *
   * The predicate must be compatible with signature bool (const Iterator &).
   */
  template <typename Pred>
  void RemoveIf(Pred&& aPred) {
    for (auto iter = Iter(); !iter.Done(); iter.Next()) {
      if (aPred(const_cast<std::add_const_t<decltype(iter)>&>(iter))) {
        iter.Remove();
      }
    }
  }

  /**
   * Looks up aKey in the hashtable and returns an object that allows you to
   * read/modify the value of the entry, or remove the entry (if found).
   *
   * A typical usage of this API looks like this:
   *
   *   if (auto entry = hashtable.Lookup(key)) {
   *     DoSomething(entry.Data());
   *     if (entry.Data() > 42) {
   *       entry.Remove();
   *     }
   *   } // else - an entry with the given key doesn't exist
   *
   * This is useful for cases where you want to read/write the value of an entry
   * and (optionally) remove the entry without having to do multiple hashtable
   * lookups.  If you want to insert a new entry if one does not exist, then use
   * WithEntryHandle instead, see below.
   */
  [[nodiscard]] LookupResult Lookup(KeyType aKey) {
    return LookupResult(this->GetEntry(aKey), *this);
  }

  /**
   * Used by WithEntryHandle as the argument type to its functor. It is
   * associated with the Key passed to WithEntryHandle and manages only the
   * potential entry with that key. Note that in case no modifying operations
   * are called on the handle, the state of the hashtable remains unchanged,
   * i.e. WithEntryHandle does not modify the hashtable itself.
   *
   * Provides query functions (Key, HasEntry/operator bool, Data) and
   * modifying operations for inserting new entries (Insert), updating existing
   * entries (Update) and removing existing entries (Remove). They have
   * debug-only assertion that fail when the state of the entry doesn't match
   * the expectation. There are variants prefixed with "Or" (OrInsert, OrUpdate,
   * OrRemove) that are a no-op in case the entry does already exist resp. does
   * not exist. There are also variants OrInsertWith and OrUpdateWith that don't
   * accept a value, but a functor, which is only called if the operation takes
   * place, which should be used if the provision of the value is not trivial
   * (e.g. allocates a heap object). Finally, there's InsertOrUpdate that
   * handles both existing and non-existing entries.
   *
   * Note that all functions of EntryHandle only deal with DataType, not with
   * UserDataType.
   */
  class EntryHandle : protected nsTHashtable<EntryType>::EntryHandle {
   public:
    using Base = typename nsTHashtable<EntryType>::EntryHandle;

    EntryHandle(EntryHandle&& aOther) = default;
    ~EntryHandle() = default;

    EntryHandle(const EntryHandle&) = delete;
    EntryHandle& operator=(const EntryHandle&) = delete;
    EntryHandle& operator=(const EntryHandle&&) = delete;

    using Base::Key;

    using Base::HasEntry;

    using Base::operator bool;

    using Base::Entry;

    /**
     * Inserts a new entry with the handle's key and the value passed to this
     * function.
     *
     * \tparam Args DataType must be constructible from Args
     * \pre !HasEntry()
     * \post HasEntry()
     */
    template <typename... Args>
    DataType& Insert(Args&&... aArgs) {
      Base::InsertInternal(std::forward<Args>(aArgs)...);
      return Data();
    }

    /**
     * If it doesn't yet exist, inserts a new entry with the handle's key and
     * the value passed to this function. The value is not consumed if no insert
     * takes place.
     *
     * \tparam Args DataType must be constructible from Args
     * \post HasEntry()
     */
    template <typename... Args>
    DataType& OrInsert(Args&&... aArgs) {
      if (!HasEntry()) {
        return Insert(std::forward<Args>(aArgs)...);
      }
      return Data();
    }

    /**
     * If it doesn't yet exist, inserts a new entry with the handle's key and
     * the result of the functor passed to this function. The functor is not
     * called if no insert takes place.
     *
     * \tparam F must return a value that is implicitly convertible to DataType
     * \post HasEntry()
     */
    template <typename F>
    DataType& OrInsertWith(F&& aFunc) {
      if (!HasEntry()) {
        return Insert(std::forward<F>(aFunc)());
      }
      return Data();
    }

    /**
     * Updates the entry with the handle's key by the value passed to this
     * function.
     *
     * \tparam U DataType must be assignable from U
     * \pre HasEntry()
     */
    template <typename U>
    DataType& Update(U&& aData) {
      MOZ_RELEASE_ASSERT(HasEntry());
      Data() = std::forward<U>(aData);
      return Data();
    }

    /**
     * If an entry with the handle's key already exists, updates its value by
     * the value passed to this function. The value is not consumed if no update
     * takes place.
     *
     * \tparam U DataType must be assignable from U
     */
    template <typename U>
    void OrUpdate(U&& aData) {
      if (HasEntry()) {
        Update(std::forward<U>(aData));
      }
    }

    /**
     * If an entry with the handle's key already exists, updates its value by
     * the the result of the functor passed to this function. The functor is not
     * called if no update takes place.
     *
     * \tparam F must return a value that DataType is assignable from
     */
    template <typename F>
    void OrUpdateWith(F&& aFunc) {
      if (HasEntry()) {
        Update(std::forward<F>(aFunc)());
      }
    }

    /**
     * If it does not yet, inserts a new entry with the handle's key and the
     * value passed to this function. Otherwise, it updates the entry by the
     * value passed to this function.
     *
     * \tparam U DataType must be implicitly convertible (and assignable) from U
     * \post HasEntry()
     */
    template <typename U>
    DataType& InsertOrUpdate(U&& aData) {
      if (!HasEntry()) {
        Insert(std::forward<U>(aData));
      } else {
        Update(std::forward<U>(aData));
      }
      return Data();
    }

    using Base::Remove;

    using Base::OrRemove;

    /**
     * Returns a reference to the value of the entry.
     *
     * \pre HasEntry()
     */
    [[nodiscard]] DataType& Data() { return Entry()->mData; }

    [[nodiscard]] DataType* DataPtrOrNull() {
      return static_cast<bool>(*this) ? &Data() : nullptr;
    }

    [[nodiscard]] DataType* operator->() { return &Data(); }

    [[nodiscard]] DataType& operator*() { return Data(); }

   private:
    friend class nsBaseHashtable;

    explicit EntryHandle(Base&& aBase) : Base(std::move(aBase)) {}
  };

  /**
   * Performs a scoped operation on the entry for aKey, which may or may not
   * exist when the function is called. It calls aFunc with an EntryHandle. The
   * result of aFunc is returned as the result of this function. Its return type
   * may be void. See the documentation of EntryHandle for the query and
   * modifying operations it offers.
   *
   * A simple use of this function is, e.g.,
   *
   *   hashtable.WithEntryHandle(key, [](auto&& entry) { entry.OrInsert(42); });
   *
   * \attention It is not safe to perform modifying operations on the hashtable
   * other than through the EntryHandle within aFunc, and trying to do so will
   * trigger debug assertions, and result in undefined behaviour otherwise.
   */
  template <class F>
  [[nodiscard]] auto WithEntryHandle(KeyType aKey, F&& aFunc)
      -> std::invoke_result_t<F, EntryHandle&&> {
    return Base::WithEntryHandle(
        aKey, [&aFunc](auto entryHandle) -> decltype(auto) {
          return std::forward<F>(aFunc)(EntryHandle{std::move(entryHandle)});
        });
  }

  /**
   * Fallible variant of WithEntryHandle, with the following differences:
   * - The functor aFunc must accept a Maybe<EntryHandle> (instead of an
   *   EntryHandle).
   * - In case allocation of the slot for the entry fails, Nothing is passed to
   *   the functor.
   *
   * For more details, see the explanation on the non-fallible overload above.
   */
  template <class F>
  [[nodiscard]] auto WithEntryHandle(KeyType aKey, const fallible_t& aFallible,
                                     F&& aFunc)
      -> std::invoke_result_t<F, mozilla::Maybe<EntryHandle>&&> {
    return Base::WithEntryHandle(
        aKey, aFallible, [&aFunc](auto maybeEntryHandle) {
          return std::forward<F>(aFunc)(
              maybeEntryHandle
                  ? mozilla::Some(EntryHandle{maybeEntryHandle.extract()})
                  : mozilla::Nothing());
        });
  }

 public:
  class ConstIterator {
   public:
    explicit ConstIterator(nsBaseHashtable* aTable)
        : mBaseIterator(&aTable->mTable) {}
    ~ConstIterator() = default;

    KeyType Key() const {
      return static_cast<EntryType*>(mBaseIterator.Get())->GetKey();
    }
    UserDataType UserData() const {
      return Converter::Unwrap(
          static_cast<EntryType*>(mBaseIterator.Get())->mData);
    }
    const DataType& Data() const {
      return static_cast<EntryType*>(mBaseIterator.Get())->mData;
    }

    bool Done() const { return mBaseIterator.Done(); }
    void Next() { mBaseIterator.Next(); }

    ConstIterator() = delete;
    ConstIterator(const ConstIterator&) = delete;
    ConstIterator(ConstIterator&& aOther) = delete;
    ConstIterator& operator=(const ConstIterator&) = delete;
    ConstIterator& operator=(ConstIterator&&) = delete;

   protected:
    PLDHashTable::Iterator mBaseIterator;
  };

  // This is an iterator that also allows entry removal. Example usage:
  //
  //   for (auto iter = table.Iter(); !iter.Done(); iter.Next()) {
  //     const KeyType key = iter.Key();
  //     const UserDataType data = iter.UserData();
  //     // or
  //     const DataType& data = iter.Data();
  //     // ... do stuff with |key| and/or |data| ...
  //     // ... possibly call iter.Remove() once ...
  //   }
  //
  class Iterator final : public ConstIterator {
   public:
    using ConstIterator::ConstIterator;

    using ConstIterator::Data;
    DataType& Data() {
      return static_cast<EntryType*>(this->mBaseIterator.Get())->mData;
    }

    void Remove() { this->mBaseIterator.Remove(); }
  };

  Iterator Iter() { return Iterator(this); }

  ConstIterator ConstIter() const {
    return ConstIterator(const_cast<nsBaseHashtable*>(this));
  }

  /**
   * Remove the entry associated with aIter.
   *
   * @param aIter the iterator pointing to the entry
   * @pre !aIter.Done()
   */
  void Remove(ConstIterator& aIter) { aIter.mBaseIterator.Remove(); }

  using typename nsTHashtable<EntryType>::iterator;
  using typename nsTHashtable<EntryType>::const_iterator;

  using nsTHashtable<EntryType>::begin;
  using nsTHashtable<EntryType>::end;
  using nsTHashtable<EntryType>::cbegin;
  using nsTHashtable<EntryType>::cend;

  /**
   * reset the hashtable, removing all entries
   */
  void Clear() { nsTHashtable<EntryType>::Clear(); }

  /**
   * Measure the size of the table's entry storage. The size of things pointed
   * to by entries must be measured separately; hence the "Shallow" prefix.
   *
   * @param   aMallocSizeOf the function used to measure heap-allocated blocks
   * @return  the summed size of the table's storage
   */
  size_t ShallowSizeOfExcludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return this->mTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Like ShallowSizeOfExcludingThis, but includes sizeof(*this).
   */
  size_t ShallowSizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const {
    return aMallocSizeOf(this) + ShallowSizeOfExcludingThis(aMallocSizeOf);
  }

  /**
   * Swap the elements in this hashtable with the elements in aOther.
   */
  void SwapElements(nsBaseHashtable& aOther) {
    nsTHashtable<EntryType>::SwapElements(aOther);
  }

  using nsTHashtable<EntryType>::MarkImmutable;
};

//
// nsBaseHashtableET definitions
//

template <class KeyClass, class DataType>
template <typename... Args>
nsBaseHashtableET<KeyClass, DataType>::nsBaseHashtableET(KeyTypePointer aKey,
                                                         Args&&... aArgs)
    : KeyClass(aKey), mData(std::forward<Args>(aArgs)...) {}

#endif  // nsBaseHashtable_h__
