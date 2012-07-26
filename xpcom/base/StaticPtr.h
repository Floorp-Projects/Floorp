/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StaticPtr_h
#define mozilla_StaticPtr_h

namespace mozilla {

/**
 * StaticAutoPtr and StaticRefPtr are like nsAutoPtr and nsRefPtr, except they
 * are suitable for use as global variables.
 *
 * In particular, a global instance of Static{Auto,Ref}Ptr doesn't cause the
 * compiler to emit  a static initializer (in release builds, anyway).
 *
 * In order to accomplish this, Static{Auto,Ref}Ptr must have a trivial
 * constructor and destructor.  As a consequence, it cannot initialize its raw
 * pointer to 0 on construction, and it cannot delete/release its raw pointer
 * upon destruction.
 *
 * Since the compiler guarantees that all global variables are initialized to
 * 0, these trivial constructors are safe, so long as you use
 * Static{Auto,Ref}Ptr as a global variable.  If you use Static{Auto,Ref}Ptr as
 * a stack variable or as a class instance variable, you will get what you
 * deserve.
 *
 * Static{Auto,Ref}Ptr have a limited interface as compared to ns{Auto,Ref}Ptr;
 * this is intentional, since their range of acceptable uses is smaller.
 */

template<class T>
class StaticAutoPtr
{
  public:
    StaticAutoPtr()
    {
      // In debug builds, check that mRawPtr is initialized for us as we expect
      // by the compiler.
      MOZ_ASSERT(!mRawPtr);
    }

    ~StaticAutoPtr() {}

    StaticAutoPtr<T>& operator=(T* rhs)
    {
      Assign(rhs);
      return *this;
    }

    T* get() const
    {
      return mRawPtr;
    }

    operator T*() const
    {
      return get();
    }

    T* operator->() const
    {
      MOZ_ASSERT(mRawPtr);
      return get();
    }

    T& operator*() const
    {
      return *get();
    }

  private:
    // Disallow copy constructor.
    StaticAutoPtr(StaticAutoPtr<T> &other);

    void Assign(T* newPtr)
    {
      MOZ_ASSERT(!newPtr || mRawPtr != newPtr);
      T* oldPtr = mRawPtr;
      mRawPtr = newPtr;
      delete oldPtr;
    }

    T* mRawPtr;
};

template<class T>
class StaticRefPtr
{
public:
  StaticRefPtr()
  {
    MOZ_ASSERT(!mRawPtr);
  }

  ~StaticRefPtr()
  {
  }

  StaticRefPtr<T>& operator=(T* rhs)
  {
    AssignWithAddref(rhs);
    return *this;
  }

  StaticRefPtr<T>& operator=(const StaticRefPtr<T>& rhs)
  {
    return (this = rhs.mRawPtr);
  }

  T* get() const
  {
    return mRawPtr;
  }

  operator T*() const
  {
    return get();
  }

  T* operator->() const
  {
    MOZ_ASSERT(mRawPtr);
    return get();
  }

  T& operator*() const
  {
    return *get();
  }

private:
  void AssignWithAddref(T* newPtr)
  {
    if (newPtr) {
      newPtr->AddRef();
    }
    AssignAssumingAddRef(newPtr);
  }

  void AssignAssumingAddRef(T* newPtr)
  {
    T* oldPtr = mRawPtr;
    mRawPtr = newPtr;
    if (oldPtr) {
      oldPtr->Release();
    }
  }

  T* mRawPtr;
};

namespace StaticPtr_internal {
class Zero;
} // namespace StaticPtr_internal

#define REFLEXIVE_EQUALITY_OPERATORS(type1, type2, eq_fn, ...) \
  template<__VA_ARGS__>                                        \
  inline bool                                                  \
  operator==(type1 lhs, type2 rhs)                             \
  {                                                            \
    return eq_fn;                                              \
  }                                                            \
                                                               \
  template<__VA_ARGS__>                                        \
  inline bool                                                  \
  operator==(type2 lhs, type1 rhs)                             \
  {                                                            \
    return rhs == lhs;                                         \
  }                                                            \
                                                               \
  template<__VA_ARGS__>                                        \
  inline bool                                                  \
  operator!=(type1 lhs, type2 rhs)                             \
  {                                                            \
    return !(lhs == rhs);                                      \
  }                                                            \
                                                               \
  template<__VA_ARGS__>                                        \
  inline bool                                                  \
  operator!=(type2 lhs, type1 rhs)                             \
  {                                                            \
    return !(lhs == rhs);                                      \
  }

// StaticAutoPtr (in)equality operators

template<class T, class U>
inline bool
operator==(const StaticAutoPtr<T>& lhs, const StaticAutoPtr<U>& rhs)
{
  return lhs.get() == rhs.get();
}

template<class T, class U>
inline bool
operator!=(const StaticAutoPtr<T>& lhs, const StaticAutoPtr<U>& rhs)
{
  return !(lhs == rhs);
}

REFLEXIVE_EQUALITY_OPERATORS(const StaticAutoPtr<T>&, const U*,
                             lhs.get() == rhs, class T, class U)

REFLEXIVE_EQUALITY_OPERATORS(const StaticAutoPtr<T>&, U*,
                             lhs.get() == rhs, class T, class U)

// Let us compare StaticAutoPtr to 0.
REFLEXIVE_EQUALITY_OPERATORS(const StaticAutoPtr<T>&, StaticPtr_internal::Zero*,
                             lhs.get() == NULL, class T)

// StaticRefPtr (in)equality operators

template<class T, class U>
inline bool
operator==(const StaticRefPtr<T>& lhs, const StaticRefPtr<U>& rhs)
{
  return lhs.get() == rhs.get();
}

template<class T, class U>
inline bool
operator!=(const StaticRefPtr<T>& lhs, const StaticRefPtr<U>& rhs)
{
  return !(lhs == rhs);
}

REFLEXIVE_EQUALITY_OPERATORS(const StaticRefPtr<T>&, const U*,
                             lhs.get() == rhs, class T, class U)

REFLEXIVE_EQUALITY_OPERATORS(const StaticRefPtr<T>&, U*,
                             lhs.get() == rhs, class T, class U)

// Let us compare StaticRefPtr to 0.
REFLEXIVE_EQUALITY_OPERATORS(const StaticRefPtr<T>&, StaticPtr_internal::Zero*,
                             lhs.get() == NULL, class T)

#undef REFLEXIVE_EQUALITY_OPERATORS

} // namespace mozilla

#endif
