/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCOMPtr_h___
#define nsCOMPtr_h___

/*
 * Having problems?
 *
 * See the documentation at:
 *   https://firefox-source-docs.mozilla.org/xpcom/refptr.html
 *
 *
 * nsCOMPtr
 *   better than a raw pointer
 * for owning objects
 *                      -- scc
 */

#include <type_traits>

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/RefPtr.h"
#include "nsCycleCollectionNoteChild.h"
#include "nsDebug.h"  // for |NS_ASSERTION|
#include "nsISupportsUtils.h"  // for |nsresult|, |NS_ADDREF|, |NS_GET_TEMPLATE_IID| et al

/*
 * WARNING: This file defines several macros for internal use only. These
 * macros begin with the prefix |NSCAP_|. Do not use these macros in your own
 * code. They are for internal use only for cross-platform compatibility, and
 * are subject to change without notice.
 */

#ifdef _MSC_VER
// Under VC++, we win by inlining StartAssignment.
#  define NSCAP_FEATURE_INLINE_STARTASSIGNMENT

// Also under VC++, at the highest warning level, we are overwhelmed with
// warnings about (unused) inline functions being removed. This is to be
// expected with templates, so we disable the warning.
#  pragma warning(disable : 4514)
#endif

#ifdef DEBUG
#  define NSCAP_FEATURE_TEST_DONTQUERY_CASES
#endif

#ifdef __GNUC__
// Our use of nsCOMPtr_base::mRawPtr violates the C++ standard's aliasing
// rules. Mark it with the may_alias attribute so that gcc 3.3 and higher
// don't reorder instructions based on aliasing assumptions for
// this variable.  Fortunately, gcc versions < 3.3 do not do any
// optimizations that break nsCOMPtr.

#  define NS_MAY_ALIAS_PTR(t) t* __attribute__((__may_alias__))
#else
#  define NS_MAY_ALIAS_PTR(t) t*
#endif

/*
 * The following three macros (NSCAP_ADDREF, NSCAP_RELEASE, and
 * NSCAP_LOG_ASSIGNMENT) allow external clients the ability to add logging or
 * other interesting debug facilities. In fact, if you want |nsCOMPtr| to
 * participate in the standard logging facility, you provide
 * (e.g., in "nsISupportsImpl.h") suitable definitions
 *
 *   #define NSCAP_ADDREF(this, ptr)         NS_ADDREF(ptr)
 *   #define NSCAP_RELEASE(this, ptr)        NS_RELEASE(ptr)
 */

#ifndef NSCAP_ADDREF
#  define NSCAP_ADDREF(this, ptr) \
    mozilla::RefPtrTraits<        \
        typename std::remove_reference<decltype(*ptr)>::type>::AddRef(ptr)
#endif

#ifndef NSCAP_RELEASE
#  define NSCAP_RELEASE(this, ptr) \
    mozilla::RefPtrTraits<         \
        typename std::remove_reference<decltype(*ptr)>::type>::Release(ptr)
#endif

// Clients can define |NSCAP_LOG_ASSIGNMENT| to perform logging.
#ifdef NSCAP_LOG_ASSIGNMENT
// Remember that |NSCAP_LOG_ASSIGNMENT| was defined by some client so that we
// know to instantiate |~nsGetterAddRefs| in turn to note the external
// assignment into the |nsCOMPtr|.
#  define NSCAP_LOG_EXTERNAL_ASSIGNMENT
#else
// ...otherwise, just strip it out of the code
#  define NSCAP_LOG_ASSIGNMENT(this, ptr)
#endif

#ifndef NSCAP_LOG_RELEASE
#  define NSCAP_LOG_RELEASE(this, ptr)
#endif

namespace mozilla {
template <class T>
class OwningNonNull;
}  // namespace mozilla

template <class T>
inline already_AddRefed<T> dont_AddRef(T* aRawPtr) {
  return already_AddRefed<T>(aRawPtr);
}

template <class T>
inline already_AddRefed<T>&& dont_AddRef(
    already_AddRefed<T>&& aAlreadyAddRefedPtr) {
  return std::move(aAlreadyAddRefedPtr);
}

/*
 * An nsCOMPtr_helper transforms commonly called getters into typesafe forms
 * that are more convenient to call, and more efficient to use with |nsCOMPtr|s.
 * Good candidates for helpers are |QueryInterface()|, |CreateInstance()|, etc.
 *
 * Here are the rules for a helper:
 *   - it implements |operator()| to produce an interface pointer
 *   - (except for its name) |operator()| is a valid [XP]COM `getter'
 *   - the interface pointer that it returns is already |AddRef()|ed (as from
 *     any good getter)
 *   - it matches the type requested with the supplied |nsIID| argument
 *   - its constructor provides an optional |nsresult*| that |operator()| can
 *     fill in with an error when it is executed
 *
 * See |class nsGetInterface| for an example.
 */
class MOZ_STACK_CLASS nsCOMPtr_helper {
 public:
  virtual nsresult NS_FASTCALL operator()(const nsIID&, void**) const = 0;
};

/*
 * nsQueryInterface could have been implemented as an nsCOMPtr_helper to avoid
 * adding specialized machinery in nsCOMPtr, but do_QueryInterface is called
 * often enough that the codesize savings are big enough to warrant the
 * specialcasing.
 */
class MOZ_STACK_CLASS nsQueryInterfaceISupports {
 public:
  explicit nsQueryInterfaceISupports(nsISupports* aRawPtr) : mRawPtr(aRawPtr) {}

  nsresult NS_FASTCALL operator()(const nsIID& aIID, void**) const;

 private:
  nsISupports* MOZ_OWNING_REF mRawPtr;
};

template <typename T>
class MOZ_STACK_CLASS nsQueryInterface final
    : public nsQueryInterfaceISupports {
 public:
  explicit nsQueryInterface(T* aRawPtr)
      : nsQueryInterfaceISupports(ToSupports(aRawPtr)) {}

  nsresult NS_FASTCALL operator()(const nsIID& aIID, void** aAnswer) const {
    return nsQueryInterfaceISupports::operator()(aIID, aAnswer);
  }
};

class MOZ_STACK_CLASS nsQueryInterfaceISupportsWithError {
 public:
  nsQueryInterfaceISupportsWithError(nsISupports* aRawPtr, nsresult* aError)
      : mRawPtr(aRawPtr), mErrorPtr(aError) {}

  nsresult NS_FASTCALL operator()(const nsIID& aIID, void**) const;

 private:
  nsISupports* MOZ_OWNING_REF mRawPtr;
  nsresult* mErrorPtr;
};

template <typename T>
class MOZ_STACK_CLASS nsQueryInterfaceWithError final
    : public nsQueryInterfaceISupportsWithError {
 public:
  explicit nsQueryInterfaceWithError(T* aRawPtr, nsresult* aError)
      : nsQueryInterfaceISupportsWithError(ToSupports(aRawPtr), aError) {}

  nsresult NS_FASTCALL operator()(const nsIID& aIID, void** aAnswer) const {
    return nsQueryInterfaceISupportsWithError::operator()(aIID, aAnswer);
  }
};

namespace mozilla {
// PointedToType<> is needed so that do_QueryInterface() will work with a
// variety of smart pointer types in addition to raw pointers. These types
// include RefPtr<>, nsCOMPtr<>, and OwningNonNull<>.
template <class T>
using PointedToType = std::remove_pointer_t<decltype(&*std::declval<T>())>;
}  // namespace mozilla

template <class T>
inline nsQueryInterface<mozilla::PointedToType<T>> do_QueryInterface(T aPtr) {
  return nsQueryInterface<mozilla::PointedToType<T>>(aPtr);
}

template <class T>
inline nsQueryInterfaceWithError<mozilla::PointedToType<T>> do_QueryInterface(
    T aRawPtr, nsresult* aError) {
  return nsQueryInterfaceWithError<mozilla::PointedToType<T>>(aRawPtr, aError);
}

template <class T>
inline void do_QueryInterface(already_AddRefed<T>&) {
  // This signature exists solely to _stop_ you from doing the bad thing.
  // Saying |do_QueryInterface()| on a pointer that is not otherwise owned by
  // someone else is an automatic leak. See bug 8221.
}

template <class T>
inline void do_QueryInterface(already_AddRefed<T>&, nsresult*) {
  // This signature exists solely to _stop_ you from doing the bad thing.
  // Saying |do_QueryInterface()| on a pointer that is not otherwise owned by
  // someone else is an automatic leak. See bug 8221.
}

////////////////////////////////////////////////////////////////////////////
// Using servicemanager with COMPtrs
class nsGetServiceByCID final {
 public:
  explicit nsGetServiceByCID(const nsCID& aCID) : mCID(aCID) {}

  nsresult NS_FASTCALL operator()(const nsIID&, void**) const;

 private:
  const nsCID& mCID;
};

class nsGetServiceByCIDWithError final {
 public:
  nsGetServiceByCIDWithError(const nsCID& aCID, nsresult* aErrorPtr)
      : mCID(aCID), mErrorPtr(aErrorPtr) {}

  nsresult NS_FASTCALL operator()(const nsIID&, void**) const;

 private:
  const nsCID& mCID;
  nsresult* mErrorPtr;
};

class nsGetServiceByContractID final {
 public:
  explicit nsGetServiceByContractID(const char* aContractID)
      : mContractID(aContractID) {}

  nsresult NS_FASTCALL operator()(const nsIID&, void**) const;

 private:
  const char* mContractID;
};

class nsGetServiceByContractIDWithError final {
 public:
  nsGetServiceByContractIDWithError(const char* aContractID,
                                    nsresult* aErrorPtr)
      : mContractID(aContractID), mErrorPtr(aErrorPtr) {}

  nsresult NS_FASTCALL operator()(const nsIID&, void**) const;

 private:
  const char* mContractID;
  nsresult* mErrorPtr;
};

class nsIWeakReference;

// Weak references
class MOZ_STACK_CLASS nsQueryReferent final {
 public:
  nsQueryReferent(nsIWeakReference* aWeakPtr, nsresult* aError)
      : mWeakPtr(aWeakPtr), mErrorPtr(aError) {}

  nsresult NS_FASTCALL operator()(const nsIID& aIID, void**) const;

 private:
  nsIWeakReference* MOZ_NON_OWNING_REF mWeakPtr;
  nsresult* mErrorPtr;
};

// template<class T> class nsGetterAddRefs;

// Helper for assert_validity method
template <class T>
char (&TestForIID(decltype(&NS_GET_TEMPLATE_IID(T))))[2];
template <class T>
char TestForIID(...);

template <class T>
class MOZ_IS_REFPTR nsCOMPtr final {
 private:
  void assign_with_AddRef(T*);
  template <typename U>
  void assign_from_qi(const nsQueryInterface<U>, const nsIID&);
  template <typename U>
  void assign_from_qi_with_error(const nsQueryInterfaceWithError<U>&,
                                 const nsIID&);
  void assign_from_gs_cid(const nsGetServiceByCID, const nsIID&);
  void assign_from_gs_cid_with_error(const nsGetServiceByCIDWithError&,
                                     const nsIID&);
  void assign_from_gs_contractid(const nsGetServiceByContractID, const nsIID&);
  void assign_from_gs_contractid_with_error(
      const nsGetServiceByContractIDWithError&, const nsIID&);
  void assign_from_query_referent(const nsQueryReferent&, const nsIID&);
  void assign_from_helper(const nsCOMPtr_helper&, const nsIID&);
  void** begin_assignment();

  void assign_assuming_AddRef(T* aNewPtr) {
    T* oldPtr = mRawPtr;
    mRawPtr = aNewPtr;
    NSCAP_LOG_ASSIGNMENT(this, aNewPtr);
    NSCAP_LOG_RELEASE(this, oldPtr);
    if (oldPtr) {
      NSCAP_RELEASE(this, oldPtr);
    }
  }

 private:
  T* MOZ_OWNING_REF mRawPtr;

  void assert_validity() {
    static_assert(1 < sizeof(TestForIID<T>(nullptr)),
                  "nsCOMPtr only works "
                  "for types with IIDs.  Either use RefPtr; add an IID to "
                  "your type with NS_DECLARE_STATIC_IID_ACCESSOR/"
                  "NS_DEFINE_STATIC_IID_ACCESSOR; or make the nsCOMPtr point "
                  "to a base class with an IID.");
  }

 public:
  typedef T element_type;

  ~nsCOMPtr() {
    NSCAP_LOG_RELEASE(this, mRawPtr);
    if (mRawPtr) {
      NSCAP_RELEASE(this, mRawPtr);
    }
  }

#ifdef NSCAP_FEATURE_TEST_DONTQUERY_CASES
  void Assert_NoQueryNeeded() {
    if (!mRawPtr) {
      return;
    }
    if constexpr (std::is_same_v<T, nsISupports>) {
      // FIXME: nsCOMPtr<nsISupports> never asserted this, and it currently
      // fails...
      return;
    }
    // This can't be defined in terms of do_QueryInterface because
    // that bans casts from a class to itself.
    void* out = nullptr;
    mRawPtr->QueryInterface(NS_GET_TEMPLATE_IID(T), &out);
    T* query_result = static_cast<T*>(out);
    MOZ_ASSERT(query_result == mRawPtr, "QueryInterface needed");
    NS_RELEASE(query_result);
  }

#  define NSCAP_ASSERT_NO_QUERY_NEEDED() Assert_NoQueryNeeded();
#else
#  define NSCAP_ASSERT_NO_QUERY_NEEDED()
#endif

  // Constructors

  nsCOMPtr() : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
  }

  MOZ_IMPLICIT nsCOMPtr(decltype(nullptr)) : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
  }

  nsCOMPtr(const nsCOMPtr<T>& aSmartPtr) : mRawPtr(aSmartPtr.mRawPtr) {
    assert_validity();
    if (mRawPtr) {
      NSCAP_ADDREF(this, mRawPtr);
    }
    NSCAP_LOG_ASSIGNMENT(this, aSmartPtr.mRawPtr);
  }

  template <class U>
  MOZ_IMPLICIT nsCOMPtr(const nsCOMPtr<U>& aSmartPtr)
      : mRawPtr(aSmartPtr.get()) {
    // Make sure that U actually inherits from T
    static_assert(std::is_base_of<T, U>::value, "U should be a subclass of T");
    assert_validity();
    if (mRawPtr) {
      NSCAP_ADDREF(this, mRawPtr);
    }
    NSCAP_LOG_ASSIGNMENT(this, aSmartPtr.get());
  }

  nsCOMPtr(nsCOMPtr<T>&& aSmartPtr) : mRawPtr(aSmartPtr.mRawPtr) {
    assert_validity();
    aSmartPtr.mRawPtr = nullptr;
    NSCAP_LOG_ASSIGNMENT(this, mRawPtr);
  }

  template <class U>
  MOZ_IMPLICIT nsCOMPtr(nsCOMPtr<U>&& aSmartPtr)
      : mRawPtr(aSmartPtr.forget().template downcast<T>().take()) {
    // Make sure that U actually inherits from T
    static_assert(std::is_base_of<T, U>::value, "U should be a subclass of T");
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, mRawPtr);
    NSCAP_ASSERT_NO_QUERY_NEEDED();
  }

  MOZ_IMPLICIT nsCOMPtr(T* aRawPtr) : mRawPtr(aRawPtr) {
    assert_validity();
    if (mRawPtr) {
      NSCAP_ADDREF(this, mRawPtr);
    }
    NSCAP_LOG_ASSIGNMENT(this, aRawPtr);
    NSCAP_ASSERT_NO_QUERY_NEEDED();
  }

  MOZ_IMPLICIT nsCOMPtr(already_AddRefed<T>& aSmartPtr)
      : mRawPtr(aSmartPtr.take()) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, mRawPtr);
    NSCAP_ASSERT_NO_QUERY_NEEDED();
  }

  // Construct from |otherComPtr.forget()|.
  MOZ_IMPLICIT nsCOMPtr(already_AddRefed<T>&& aSmartPtr)
      : mRawPtr(aSmartPtr.take()) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, mRawPtr);
    NSCAP_ASSERT_NO_QUERY_NEEDED();
  }

  // Construct from |std::move(otherRefPtr)|.
  template <typename U>
  MOZ_IMPLICIT nsCOMPtr(RefPtr<U>&& aSmartPtr)
      : mRawPtr(static_cast<already_AddRefed<T>>(aSmartPtr.forget()).take()) {
    assert_validity();
    // Make sure that U actually inherits from T
    static_assert(std::is_base_of<T, U>::value, "U is not a subclass of T");
    NSCAP_LOG_ASSIGNMENT(this, mRawPtr);
    NSCAP_ASSERT_NO_QUERY_NEEDED();
  }

  // Construct from |already_AddRefed|.
  template <typename U>
  MOZ_IMPLICIT nsCOMPtr(already_AddRefed<U>& aSmartPtr)
      : mRawPtr(static_cast<T*>(aSmartPtr.take())) {
    assert_validity();
    // But make sure that U actually inherits from T.
    static_assert(std::is_base_of<T, U>::value, "U is not a subclass of T");
    NSCAP_LOG_ASSIGNMENT(this, static_cast<T*>(mRawPtr));
    NSCAP_ASSERT_NO_QUERY_NEEDED();
  }

  // Construct from |otherComPtr.forget()|.
  template <typename U>
  MOZ_IMPLICIT nsCOMPtr(already_AddRefed<U>&& aSmartPtr)
      : mRawPtr(static_cast<T*>(aSmartPtr.take())) {
    assert_validity();
    // But make sure that U actually inherits from T.
    static_assert(std::is_base_of<T, U>::value, "U is not a subclass of T");
    NSCAP_LOG_ASSIGNMENT(this, static_cast<T*>(mRawPtr));
    NSCAP_ASSERT_NO_QUERY_NEEDED();
  }

  // Construct from |do_QueryInterface(expr)|.
  template <typename U>
  MOZ_IMPLICIT nsCOMPtr(const nsQueryInterface<U> aQI) : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
    assign_from_qi(aQI, NS_GET_TEMPLATE_IID(T));
  }

  // Construct from |do_QueryInterface(expr, &rv)|.
  template <typename U>
  MOZ_IMPLICIT nsCOMPtr(const nsQueryInterfaceWithError<U>& aQI)
      : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
    assign_from_qi_with_error(aQI, NS_GET_TEMPLATE_IID(T));
  }

  // Construct from |do_GetService(cid_expr)|.
  MOZ_IMPLICIT nsCOMPtr(const nsGetServiceByCID aGS) : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
    assign_from_gs_cid(aGS, NS_GET_TEMPLATE_IID(T));
  }

  // Construct from |do_GetService(cid_expr, &rv)|.
  MOZ_IMPLICIT nsCOMPtr(const nsGetServiceByCIDWithError& aGS)
      : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
    assign_from_gs_cid_with_error(aGS, NS_GET_TEMPLATE_IID(T));
  }

  // Construct from |do_GetService(contractid_expr)|.
  MOZ_IMPLICIT nsCOMPtr(const nsGetServiceByContractID aGS) : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
    assign_from_gs_contractid(aGS, NS_GET_TEMPLATE_IID(T));
  }

  // Construct from |do_GetService(contractid_expr, &rv)|.
  MOZ_IMPLICIT nsCOMPtr(const nsGetServiceByContractIDWithError& aGS)
      : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
    assign_from_gs_contractid_with_error(aGS, NS_GET_TEMPLATE_IID(T));
  }

  // Construct from |do_QueryReferent(ptr)|
  MOZ_IMPLICIT nsCOMPtr(const nsQueryReferent& aQueryReferent)
      : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
    assign_from_query_referent(aQueryReferent, NS_GET_TEMPLATE_IID(T));
  }

  // And finally, anything else we might need to construct from can exploit the
  // nsCOMPtr_helper facility.
  MOZ_IMPLICIT nsCOMPtr(const nsCOMPtr_helper& aHelper) : mRawPtr(nullptr) {
    assert_validity();
    NSCAP_LOG_ASSIGNMENT(this, nullptr);
    assign_from_helper(aHelper, NS_GET_TEMPLATE_IID(T));
    NSCAP_ASSERT_NO_QUERY_NEEDED();
  }

  // construct from |mozilla::NotNull|.
  template <typename I,
            typename = std::enable_if_t<!std::is_same_v<I, nsCOMPtr<T>> &&
                                        std::is_convertible_v<I, nsCOMPtr<T>>>>
  MOZ_IMPLICIT nsCOMPtr(const mozilla::NotNull<I>& aSmartPtr)
      : mRawPtr(nsCOMPtr<T>(aSmartPtr.get()).forget().take()) {}

  // construct from |mozilla::MovingNotNull|.
  template <typename I,
            typename = std::enable_if_t<!std::is_same_v<I, nsCOMPtr<T>> &&
                                        std::is_convertible_v<I, nsCOMPtr<T>>>>
  MOZ_IMPLICIT nsCOMPtr(mozilla::MovingNotNull<I>&& aSmartPtr)
      : mRawPtr(
            nsCOMPtr<T>(std::move(aSmartPtr).unwrapBasePtr()).forget().take()) {
  }

  // Defined in OwningNonNull.h
  template <class U>
  MOZ_IMPLICIT nsCOMPtr(const mozilla::OwningNonNull<U>& aOther);

  // Assignment operators

  nsCOMPtr<T>& operator=(const nsCOMPtr<T>& aRhs) {
    assign_with_AddRef(aRhs.mRawPtr);
    return *this;
  }

  template <class U>
  nsCOMPtr<T>& operator=(const nsCOMPtr<U>& aRhs) {
    // Make sure that U actually inherits from T
    static_assert(std::is_base_of<T, U>::value, "U should be a subclass of T");
    assign_with_AddRef(aRhs.get());
    return *this;
  }

  nsCOMPtr<T>& operator=(nsCOMPtr<T>&& aRhs) {
    assign_assuming_AddRef(aRhs.forget().take());
    return *this;
  }

  template <class U>
  nsCOMPtr<T>& operator=(nsCOMPtr<U>&& aRhs) {
    // Make sure that U actually inherits from T
    static_assert(std::is_base_of<T, U>::value, "U should be a subclass of T");
    assign_assuming_AddRef(aRhs.forget().template downcast<T>().take());
    NSCAP_ASSERT_NO_QUERY_NEEDED();
    return *this;
  }

  nsCOMPtr<T>& operator=(T* aRhs) {
    assign_with_AddRef(aRhs);
    NSCAP_ASSERT_NO_QUERY_NEEDED();
    return *this;
  }

  nsCOMPtr<T>& operator=(decltype(nullptr)) {
    assign_assuming_AddRef(nullptr);
    return *this;
  }

  // Assign from |already_AddRefed|.
  template <typename U>
  nsCOMPtr<T>& operator=(already_AddRefed<U>& aRhs) {
    // Make sure that U actually inherits from T
    static_assert(std::is_base_of<T, U>::value, "U is not a subclass of T");
    assign_assuming_AddRef(static_cast<T*>(aRhs.take()));
    NSCAP_ASSERT_NO_QUERY_NEEDED();
    return *this;
  }

  // Assign from |otherComPtr.forget()|.
  template <typename U>
  nsCOMPtr<T>& operator=(already_AddRefed<U>&& aRhs) {
    // Make sure that U actually inherits from T
    static_assert(std::is_base_of<T, U>::value, "U is not a subclass of T");
    assign_assuming_AddRef(static_cast<T*>(aRhs.take()));
    NSCAP_ASSERT_NO_QUERY_NEEDED();
    return *this;
  }

  // Assign from |std::move(otherRefPtr)|.
  template <typename U>
  nsCOMPtr<T>& operator=(RefPtr<U>&& aRhs) {
    // Make sure that U actually inherits from T
    static_assert(std::is_base_of<T, U>::value, "U is not a subclass of T");
    assign_assuming_AddRef(static_cast<T*>(aRhs.forget().take()));
    NSCAP_ASSERT_NO_QUERY_NEEDED();
    return *this;
  }

  // Assign from |do_QueryInterface(expr)|.
  template <typename U>
  nsCOMPtr<T>& operator=(const nsQueryInterface<U> aRhs) {
    assign_from_qi(aRhs, NS_GET_TEMPLATE_IID(T));
    return *this;
  }

  // Assign from |do_QueryInterface(expr, &rv)|.
  template <typename U>
  nsCOMPtr<T>& operator=(const nsQueryInterfaceWithError<U>& aRhs) {
    assign_from_qi_with_error(aRhs, NS_GET_TEMPLATE_IID(T));
    return *this;
  }

  // Assign from |do_GetService(cid_expr)|.
  nsCOMPtr<T>& operator=(const nsGetServiceByCID aRhs) {
    assign_from_gs_cid(aRhs, NS_GET_TEMPLATE_IID(T));
    return *this;
  }

  // Assign from |do_GetService(cid_expr, &rv)|.
  nsCOMPtr<T>& operator=(const nsGetServiceByCIDWithError& aRhs) {
    assign_from_gs_cid_with_error(aRhs, NS_GET_TEMPLATE_IID(T));
    return *this;
  }

  // Assign from |do_GetService(contractid_expr)|.
  nsCOMPtr<T>& operator=(const nsGetServiceByContractID aRhs) {
    assign_from_gs_contractid(aRhs, NS_GET_TEMPLATE_IID(T));
    return *this;
  }

  // Assign from |do_GetService(contractid_expr, &rv)|.
  nsCOMPtr<T>& operator=(const nsGetServiceByContractIDWithError& aRhs) {
    assign_from_gs_contractid_with_error(aRhs, NS_GET_TEMPLATE_IID(T));
    return *this;
  }

  // Assign from |do_QueryReferent(ptr)|.
  nsCOMPtr<T>& operator=(const nsQueryReferent& aRhs) {
    assign_from_query_referent(aRhs, NS_GET_TEMPLATE_IID(T));
    return *this;
  }

  // And finally, anything else we might need to assign from can exploit the
  // nsCOMPtr_helper facility.
  nsCOMPtr<T>& operator=(const nsCOMPtr_helper& aRhs) {
    assign_from_helper(aRhs, NS_GET_TEMPLATE_IID(T));
    NSCAP_ASSERT_NO_QUERY_NEEDED();
    return *this;
  }

  // Assign from |mozilla::NotNull|.
  template <typename I,
            typename = std::enable_if_t<std::is_convertible_v<I, nsCOMPtr<T>>>>
  nsCOMPtr<T>& operator=(const mozilla::NotNull<I>& aSmartPtr) {
    assign_assuming_AddRef(nsCOMPtr<T>(aSmartPtr.get()).forget().take());
    return *this;
  }

  // Assign from |mozilla::MovingNotNull|.
  template <typename I,
            typename = std::enable_if_t<std::is_convertible_v<I, nsCOMPtr<T>>>>
  nsCOMPtr<T>& operator=(mozilla::MovingNotNull<I>&& aSmartPtr) {
    assign_assuming_AddRef(
        nsCOMPtr<T>(std::move(aSmartPtr).unwrapBasePtr()).forget().take());
    return *this;
  }

  // Defined in OwningNonNull.h
  template <class U>
  nsCOMPtr<T>& operator=(const mozilla::OwningNonNull<U>& aOther);

  // Exchange ownership with |aRhs|; can save a pair of refcount operations.
  void swap(nsCOMPtr<T>& aRhs) {
    T* temp = aRhs.mRawPtr;
    NSCAP_LOG_ASSIGNMENT(&aRhs, mRawPtr);
    NSCAP_LOG_ASSIGNMENT(this, temp);
    NSCAP_LOG_RELEASE(this, mRawPtr);
    NSCAP_LOG_RELEASE(&aRhs, temp);
    aRhs.mRawPtr = mRawPtr;
    mRawPtr = temp;
    // |aRhs| maintains the same invariants, so we don't need to
    // |NSCAP_ASSERT_NO_QUERY_NEEDED|
  }

  // Exchange ownership with |aRhs|; can save a pair of refcount operations.
  void swap(T*& aRhs) {
    T* temp = aRhs;
    NSCAP_LOG_ASSIGNMENT(this, temp);
    NSCAP_LOG_RELEASE(this, mRawPtr);
    aRhs = reinterpret_cast<T*>(mRawPtr);
    mRawPtr = temp;
    NSCAP_ASSERT_NO_QUERY_NEEDED();
  }

  // Other pointer operators

  // Return the value of mRawPtr and null out mRawPtr. Useful for
  // already_AddRefed return values.
  already_AddRefed<T> MOZ_MAY_CALL_AFTER_MUST_RETURN forget() {
    T* temp = nullptr;
    swap(temp);
    return already_AddRefed<T>(temp);
  }

  // Set the target of aRhs to the value of mRawPtr and null out mRawPtr.
  // Useful to avoid unnecessary AddRef/Release pairs with "out" parameters
  // where aRhs bay be a T** or an I** where I is a base class of T.
  template <typename I>
  void forget(I** aRhs) {
    NS_ASSERTION(aRhs, "Null pointer passed to forget!");
    NSCAP_LOG_RELEASE(this, mRawPtr);
    *aRhs = get();
    mRawPtr = nullptr;
  }

  // Prefer the implicit conversion provided automatically by
  // |operator T*() const|. Use |get()| to resolve ambiguity or to get a
  // castable pointer.
  T* get() const { return reinterpret_cast<T*>(mRawPtr); }

  // Makes an nsCOMPtr act like its underlying raw pointer type whenever it is
  // used in a context where a raw pointer is expected. It is this operator
  // that makes an nsCOMPtr substitutable for a raw pointer.
  //
  // Prefer the implicit use of this operator to calling |get()|, except where
  // necessary to resolve ambiguity.
  operator T*() const& { return get(); }

  // Don't allow implicit conversion of temporary nsCOMPtr to raw pointer,
  // because the refcount might be one and the pointer will immediately become
  // invalid.
  operator T*() const&& = delete;

  // Needed to avoid the deleted operator above
  explicit operator bool() const { return !!mRawPtr; }

  T* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN {
    MOZ_ASSERT(mRawPtr != nullptr,
               "You can't dereference a NULL nsCOMPtr with operator->().");
    return get();
  }

  // These are not intended to be used by clients. See |address_of| below.
  nsCOMPtr<T>* get_address() { return this; }
  const nsCOMPtr<T>* get_address() const { return this; }

 public:
  T& operator*() const {
    MOZ_ASSERT(mRawPtr != nullptr,
               "You can't dereference a NULL nsCOMPtr with operator*().");
    return *get();
  }

  T** StartAssignment() {
#ifndef NSCAP_FEATURE_INLINE_STARTASSIGNMENT
    return reinterpret_cast<T**>(begin_assignment());
#else
    assign_assuming_AddRef(nullptr);
    return reinterpret_cast<T**>(&mRawPtr);
#endif
  }
};

template <typename T>
inline void ImplCycleCollectionUnlink(nsCOMPtr<T>& aField) {
  aField = nullptr;
}

template <typename T>
inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, nsCOMPtr<T>& aField,
    const char* aName, uint32_t aFlags = 0) {
  CycleCollectionNoteChild(aCallback, aField.get(), aName, aFlags);
}

template <class T>
void nsCOMPtr<T>::assign_with_AddRef(T* aRawPtr) {
  if (aRawPtr) {
    NSCAP_ADDREF(this, aRawPtr);
  }
  assign_assuming_AddRef(aRawPtr);
}

template <class T>
template <typename U>
void nsCOMPtr<T>::assign_from_qi(const nsQueryInterface<U> aQI,
                                 const nsIID& aIID) {
  // Allow QIing to nsISupports from nsISupports as a special-case, since
  // SameCOMIdentity uses it.
  static_assert(
      std::is_same_v<T, nsISupports> ||
          !(std::is_same_v<T, U> || std::is_base_of<T, U>::value),
      "don't use do_QueryInterface for compile-time-determinable casts");
  void* newRawPtr;
  if (NS_FAILED(aQI(aIID, &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
}

template <class T>
template <typename U>
void nsCOMPtr<T>::assign_from_qi_with_error(
    const nsQueryInterfaceWithError<U>& aQI, const nsIID& aIID) {
  static_assert(
      !(std::is_same_v<T, U> || std::is_base_of<T, U>::value),
      "don't use do_QueryInterface for compile-time-determinable casts");
  void* newRawPtr;
  if (NS_FAILED(aQI(aIID, &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
}

template <class T>
void nsCOMPtr<T>::assign_from_gs_cid(const nsGetServiceByCID aGS,
                                     const nsIID& aIID) {
  void* newRawPtr;
  if (NS_FAILED(aGS(aIID, &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
}

template <class T>
void nsCOMPtr<T>::assign_from_gs_cid_with_error(
    const nsGetServiceByCIDWithError& aGS, const nsIID& aIID) {
  void* newRawPtr;
  if (NS_FAILED(aGS(aIID, &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
}

template <class T>
void nsCOMPtr<T>::assign_from_gs_contractid(const nsGetServiceByContractID aGS,
                                            const nsIID& aIID) {
  void* newRawPtr;
  if (NS_FAILED(aGS(aIID, &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
}

template <class T>
void nsCOMPtr<T>::assign_from_gs_contractid_with_error(
    const nsGetServiceByContractIDWithError& aGS, const nsIID& aIID) {
  void* newRawPtr;
  if (NS_FAILED(aGS(aIID, &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
}

template <class T>
void nsCOMPtr<T>::assign_from_query_referent(
    const nsQueryReferent& aQueryReferent, const nsIID& aIID) {
  void* newRawPtr;
  if (NS_FAILED(aQueryReferent(aIID, &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
}

template <class T>
void nsCOMPtr<T>::assign_from_helper(const nsCOMPtr_helper& helper,
                                     const nsIID& aIID) {
  void* newRawPtr;
  if (NS_FAILED(helper(aIID, &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
}

template <class T>
void** nsCOMPtr<T>::begin_assignment() {
  assign_assuming_AddRef(nullptr);
  union {
    T** mT;
    void** mVoid;
  } result;
  result.mT = &mRawPtr;
  return result.mVoid;
}

template <class T>
inline nsCOMPtr<T>* address_of(nsCOMPtr<T>& aPtr) {
  return aPtr.get_address();
}

template <class T>
inline const nsCOMPtr<T>* address_of(const nsCOMPtr<T>& aPtr) {
  return aPtr.get_address();
}

/**
 * This class is designed to be used for anonymous temporary objects in the
 * argument list of calls that return COM interface pointers, e.g.,
 *
 *   nsCOMPtr<IFoo> fooP;
 *   ...->QueryInterface(iid, getter_AddRefs(fooP))
 *
 * DO NOT USE THIS TYPE DIRECTLY IN YOUR CODE. Use |getter_AddRefs()| instead.
 *
 * When initialized with a |nsCOMPtr|, as in the example above, it returns
 * a |void**|, a |T**|, or an |nsISupports**| as needed, that the outer call
 * (|QueryInterface| in this case) can fill in.
 *
 * This type should be a nested class inside |nsCOMPtr<T>|.
 */
template <class T>
class nsGetterAddRefs {
 public:
  explicit nsGetterAddRefs(nsCOMPtr<T>& aSmartPtr)
      : mTargetSmartPtr(aSmartPtr) {}

#if defined(NSCAP_FEATURE_TEST_DONTQUERY_CASES) || \
    defined(NSCAP_LOG_EXTERNAL_ASSIGNMENT)
  ~nsGetterAddRefs() {
#  ifdef NSCAP_LOG_EXTERNAL_ASSIGNMENT
    NSCAP_LOG_ASSIGNMENT(reinterpret_cast<void*>(address_of(mTargetSmartPtr)),
                         mTargetSmartPtr.get());
#  endif

#  ifdef NSCAP_FEATURE_TEST_DONTQUERY_CASES
    mTargetSmartPtr.Assert_NoQueryNeeded();
#  endif
  }
#endif

  operator void**() {
    return reinterpret_cast<void**>(mTargetSmartPtr.StartAssignment());
  }

  operator T**() { return mTargetSmartPtr.StartAssignment(); }
  T*& operator*() { return *(mTargetSmartPtr.StartAssignment()); }

 private:
  nsCOMPtr<T>& mTargetSmartPtr;
};

template <>
class nsGetterAddRefs<nsISupports> {
 public:
  explicit nsGetterAddRefs(nsCOMPtr<nsISupports>& aSmartPtr)
      : mTargetSmartPtr(aSmartPtr) {}

#ifdef NSCAP_LOG_EXTERNAL_ASSIGNMENT
  ~nsGetterAddRefs() {
    NSCAP_LOG_ASSIGNMENT(reinterpret_cast<void*>(address_of(mTargetSmartPtr)),
                         mTargetSmartPtr.get());
  }
#endif

  operator void**() {
    return reinterpret_cast<void**>(mTargetSmartPtr.StartAssignment());
  }

  operator nsISupports**() { return mTargetSmartPtr.StartAssignment(); }
  nsISupports*& operator*() { return *(mTargetSmartPtr.StartAssignment()); }

 private:
  nsCOMPtr<nsISupports>& mTargetSmartPtr;
};

template <class T>
inline nsGetterAddRefs<T> getter_AddRefs(nsCOMPtr<T>& aSmartPtr) {
  return nsGetterAddRefs<T>(aSmartPtr);
}

template <class T, class DestinationType>
inline nsresult CallQueryInterface(
    T* aSource, nsGetterAddRefs<DestinationType> aDestination) {
  return CallQueryInterface(aSource,
                            static_cast<DestinationType**>(aDestination));
}

// Comparing two |nsCOMPtr|s

template <class T, class U>
inline bool operator==(const nsCOMPtr<T>& aLhs, const nsCOMPtr<U>& aRhs) {
  return static_cast<const T*>(aLhs.get()) == static_cast<const U*>(aRhs.get());
}

template <class T, class U>
inline bool operator!=(const nsCOMPtr<T>& aLhs, const nsCOMPtr<U>& aRhs) {
  return static_cast<const T*>(aLhs.get()) != static_cast<const U*>(aRhs.get());
}

// Comparing an |nsCOMPtr| to a raw pointer

template <class T, class U>
inline bool operator==(const nsCOMPtr<T>& aLhs, const U* aRhs) {
  return static_cast<const T*>(aLhs.get()) == aRhs;
}

template <class T, class U>
inline bool operator==(const U* aLhs, const nsCOMPtr<T>& aRhs) {
  return aLhs == static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool operator!=(const nsCOMPtr<T>& aLhs, const U* aRhs) {
  return static_cast<const T*>(aLhs.get()) != aRhs;
}

template <class T, class U>
inline bool operator!=(const U* aLhs, const nsCOMPtr<T>& aRhs) {
  return aLhs != static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool operator==(const nsCOMPtr<T>& aLhs, U* aRhs) {
  return static_cast<const T*>(aLhs.get()) == const_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool operator==(U* aLhs, const nsCOMPtr<T>& aRhs) {
  return const_cast<const U*>(aLhs) == static_cast<const T*>(aRhs.get());
}

template <class T, class U>
inline bool operator!=(const nsCOMPtr<T>& aLhs, U* aRhs) {
  return static_cast<const T*>(aLhs.get()) != const_cast<const U*>(aRhs);
}

template <class T, class U>
inline bool operator!=(U* aLhs, const nsCOMPtr<T>& aRhs) {
  return const_cast<const U*>(aLhs) != static_cast<const T*>(aRhs.get());
}

// Comparing an |nsCOMPtr| to |nullptr|

template <class T>
inline bool operator==(const nsCOMPtr<T>& aLhs, decltype(nullptr)) {
  return aLhs.get() == nullptr;
}

template <class T>
inline bool operator==(decltype(nullptr), const nsCOMPtr<T>& aRhs) {
  return nullptr == aRhs.get();
}

template <class T>
inline bool operator!=(const nsCOMPtr<T>& aLhs, decltype(nullptr)) {
  return aLhs.get() != nullptr;
}

template <class T>
inline bool operator!=(decltype(nullptr), const nsCOMPtr<T>& aRhs) {
  return nullptr != aRhs.get();
}

// Comparing any two [XP]COM objects for identity

inline bool SameCOMIdentity(nsISupports* aLhs, nsISupports* aRhs) {
  return nsCOMPtr<nsISupports>(do_QueryInterface(aLhs)) ==
         nsCOMPtr<nsISupports>(do_QueryInterface(aRhs));
}

template <class SourceType, class DestinationType>
inline nsresult CallQueryInterface(nsCOMPtr<SourceType>& aSourcePtr,
                                   DestinationType** aDestPtr) {
  return CallQueryInterface(aSourcePtr.get(), aDestPtr);
}

template <class T>
RefPtr<T>::RefPtr(const nsQueryReferent& aQueryReferent) {
  void* newRawPtr;
  if (NS_FAILED(aQueryReferent(NS_GET_TEMPLATE_IID(T), &newRawPtr))) {
    newRawPtr = nullptr;
  }
  mRawPtr = static_cast<T*>(newRawPtr);
}

template <class T>
RefPtr<T>::RefPtr(const nsCOMPtr_helper& aHelper) {
  void* newRawPtr;
  if (NS_FAILED(aHelper(NS_GET_TEMPLATE_IID(T), &newRawPtr))) {
    newRawPtr = nullptr;
  }
  mRawPtr = static_cast<T*>(newRawPtr);
}

template <class T>
RefPtr<T>& RefPtr<T>::operator=(const nsQueryReferent& aQueryReferent) {
  void* newRawPtr;
  if (NS_FAILED(aQueryReferent(NS_GET_TEMPLATE_IID(T), &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
  return *this;
}

template <class T>
RefPtr<T>& RefPtr<T>::operator=(const nsCOMPtr_helper& aHelper) {
  void* newRawPtr;
  if (NS_FAILED(aHelper(NS_GET_TEMPLATE_IID(T), &newRawPtr))) {
    newRawPtr = nullptr;
  }
  assign_assuming_AddRef(static_cast<T*>(newRawPtr));
  return *this;
}

template <class T>
inline already_AddRefed<T> do_AddRef(const nsCOMPtr<T>& aObj) {
  nsCOMPtr<T> ref(aObj);
  return ref.forget();
}

// MOZ_DBG support

template <class T>
std::ostream& operator<<(std::ostream& aOut, const nsCOMPtr<T>& aObj) {
  return mozilla::DebugValue(aOut, aObj.get());
}

// ToRefPtr allows to move an nsCOMPtr<T> into a RefPtr<T>. Be mindful when
// using this, because usually RefPtr<T> should only be used with concrete T and
// nsCOMPtr<T> should only be used with XPCOM interface T.
template <class T>
RefPtr<T> ToRefPtr(nsCOMPtr<T>&& aObj) {
  return aObj.forget();
}

// Integration with ResultExtensions.h
template <typename R>
auto ResultRefAsParam(nsCOMPtr<R>& aResult) {
  return getter_AddRefs(aResult);
}

namespace mozilla::detail {
template <typename T>
struct outparam_as_pointer;

template <typename T>
struct outparam_as_pointer<nsGetterAddRefs<T>> {
  using type = T**;
};
}  // namespace mozilla::detail

#endif  // !defined(nsCOMPtr_h___)
