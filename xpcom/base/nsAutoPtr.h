/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is nsAutoPtr.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by the Initial Developer are Copyright (C)
 * 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@fas.harvard.edu> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCOMPtr.h" // for |already_AddRefed|
#include "nsMemory.h" // for |nsMemory::Free| for |nsMemoryAutoPtr|

template <class T>
class
nsAutoPtr
{
    private:
        inline void enter(T* aPtr) { }
        inline void exit(T* aPtr) { delete aPtr; }

    public:

        nsAutoPtr() : mPtr(0) { }
        // not |explicit| to allow |nsAutoPtr<T> ptr = foo;|
        nsAutoPtr(T* aPtr) : mPtr(aPtr) { enter(aPtr); }

        ~nsAutoPtr() { exit(mPtr); }

        nsAutoPtr<T>&
        operator=(T* aPtr)
        {
            T* temp = mPtr;
            enter(aPtr);
            mPtr = aPtr;
            exit(temp);
            return *this;
        }

        nsAutoPtr<T>&
        operator=(nsAutoPtr<T>& aSmartPtr)
        {
            T* temp = mPtr;
            enter(aPtr);
            mPtr = aPtr.release();
            exit(temp);
            return *this;
        }

        operator T*() const { return mPtr; }
        T* get() const { return mPtr; }
        T* release() { T* temp = mPtr; mPtr = 0; return temp; }

        // To be used only by helpers (such as |getter_Transfers| below)
        T** begin_assignment() { mPtr = 0; return &mPtr; }

    private:
        // Helpers (such as |getter_Transfers| below) should use
        // begin_assignment(), others should not do this.
        nsAutoPtr<T>* operator&() { return this; }

        T* mPtr;
};

template <class T>
class
nsAutoArrayPtr
{
    private:
        inline void enter(T* aPtr) { }
        inline void exit(T* aPtr) { delete [] aPtr; }

    public:

        nsAutoArrayPtr() : mPtr(0) { }
        // not |explicit| to allow |nsAutoArrayPtr<T> ptr = foo;|
        nsAutoArrayPtr(T* aPtr) : mPtr(aPtr) { enter(aPtr); }

        ~nsAutoArrayPtr() { exit(mPtr); }

        nsAutoArrayPtr<T>&
        operator=(T* aPtr)
        {
            T* temp = mPtr;
            enter(aPtr);
            mPtr = aPtr;
            exit(temp);
            return *this;
        }

        nsAutoArrayPtr<T>&
        operator=(nsAutoArrayPtr<T>& aSmartPtr)
        {
            T* temp = mPtr;
            enter(aPtr);
            mPtr = aPtr.release();
            exit(temp);
            return *this;
        }

        operator T*() const { return mPtr; }
        T* get() const { return mPtr; }
        T* release() { T* temp = mPtr; mPtr = 0; return temp; }

        // To be used only by helpers (such as |getter_Transfers| below)
        T** begin_assignment() { mPtr = 0; return &mPtr; }

    private:
        // Helpers (such as |getter_Transfers| below) should use
        // begin_assignment(), others should not do this.
        nsAutoArrayPtr<T>* operator&() { return this; }

        T* mPtr;
};

/*
 * NOTE:  nsMemoryAutoArrayPtr uses nsMemory, not nsIMemory, so using it
 * requires linking against the xpcom library.
 */
template <class T>
class
nsMemoryAutoPtr
{
    private:
        inline void enter(T* aPtr) { }
        inline void exit(T* aPtr) { nsMemory::Free(aPtr); }

    public:

        nsMemoryAutoPtr() : mPtr(0) { }
        // not |explicit| to allow |nsMemoryAutoPtr<T> ptr = foo;|
        nsMemoryAutoPtr(T* aPtr) : mPtr(aPtr) { enter(aPtr); }

        ~nsMemoryAutoPtr() { exit(mPtr); }

        nsMemoryAutoPtr<T>&
        operator=(T* aPtr)
        {
            T* temp = mPtr;
            enter(aPtr);
            mPtr = aPtr;
            exit(temp);
            return *this;
        }

        nsMemoryAutoPtr<T>&
        operator=(nsMemoryAutoPtr<T>& aSmartPtr)
        {
            T* temp = mPtr;
            enter(aPtr);
            mPtr = aPtr.release();
            exit(temp);
            return *this;
        }

        operator T*() const { return mPtr; }
        T* get() const { return mPtr; }
        T* release() { T* temp = mPtr; mPtr = 0; return temp; }

        // To be used only by helpers (such as |getter_Transfers| below)
        T** begin_assignment() { mPtr = 0; return &mPtr; }

    private:
        // Helpers (such as |getter_Transfers| below) should use
        // begin_assignment(), others should not do this.
        nsMemoryAutoPtr<T>* operator&() { return this; }

        T* mPtr;
};

template <class T>
class
nsRefPtr
{
    private:
        inline void enter(T* aPtr) { aPtr->AddRef(); }
        inline void exit(T* aPtr) { aPtr->Release(); }

    public:

        nsRefPtr() : mPtr(0) { }
        // not |explicit| to allow |nsRefPtr<T> ptr = foo;|
        nsRefPtr(T* aPtr) : mPtr(aPtr) { enter(aPtr); }
        nsRefPtr(const already_AddRefed<T>& aPtr) : mPtr(aPtr.get()) { }
          // needed because ctor for already_AddRefed<T> is not explicit
        nsRefPtr(const nsRefPtr<T>& aPtr) : mPtr(aPtr.get()) { enter(mPtr); }

        ~nsRefPtr() { exit(mPtr); }

        nsRefPtr<T>&
        operator=(T* aPtr)
        {
            T* temp = mPtr;
            enter(aPtr);
            mPtr = aPtr;
            exit(temp);
            return *this;
        }

        nsRefPtr<T>&
        operator=(already_AddRefed<T> aPtr)
        {
            T* temp = mPtr;
            mPtr = aPtr.get();
            exit(temp);
            return *this;
        }

        operator T*() const { return mPtr; }
        T* get() const { return mPtr; }

        // To be used only by helpers (such as |getter_AddRefs| below)
        T** begin_assignment_assuming_AddRef() { mPtr = 0 ; return &mPtr; }

    private:
        // Helpers (such as |getter_AddRefs| below) should use
        // begin_assignment_assuming_AddRef(), others should not do this.
        nsRefPtr<T>* operator&() { return this; }

        T* mPtr;
};

template <class T>
class
nsAutoPtrGetterTransfers
{
    public:
        explicit nsAutoPtrGetterTransfers(nsAutoPtr<T>& aSmartPtr)
            : mPtrPtr(aSmartPtr.begin_assignment()) { }

        operator T**() { return mPtrPtr; }
        T& operator*() { return *mPtrPtr; }

    private:
        T** mPtrPtr;
};

template <class T>
inline nsAutoPtrGetterTransfers<T>
getter_Transfers(nsAutoPtr<T>& aSmartPtr)
{
    return nsAutoPtrGetterTransfers<T>(aSmartPtr);
}

template <class T>
class
nsAutoArrayPtrGetterTransfers
{
    public:
        explicit nsAutoArrayPtrGetterTransfers(nsAutoArrayPtr<T>& aSmartPtr)
            : mPtrPtr(aSmartPtr.begin_assignment()) { }

        operator T**() { return mPtrPtr; }
        T& operator*() { return *mPtrPtr; }

    private:
        T** mPtrPtr;
};

template <class T>
inline nsAutoArrayPtrGetterTransfers<T>
getter_Transfers(nsAutoArrayPtr<T>& aSmartPtr)
{
    return nsAutoArrayPtrGetterTransfers<T>(aSmartPtr);
}

template <class T>
class
nsMemoryAutoPtrGetterTransfers
{
    public:
        explicit nsMemoryAutoPtrGetterTransfers(nsMemoryAutoPtr<T>& aSmartPtr)
            : mPtrPtr(aSmartPtr.begin_assignment()) { }

        operator T**() { return mPtrPtr; }
        T& operator*() { return *mPtrPtr; }

    private:
        T** mPtrPtr;
};

template <class T>
inline nsMemoryAutoPtrGetterTransfers<T>
getter_Transfers(nsMemoryAutoPtr<T>& aSmartPtr)
{
    return nsMemoryAutoPtrGetterTransfers<T>(aSmartPtr);
}

template <class T>
class
nsRefPtrGetterAddRefs
{
    public:
        explicit nsRefPtrGetterAddRefs(nsRefPtr<T>& aSmartPtr)
            : mPtrPtr(aSmartPtr.begin_assignment_assuming_AddRef()) { }

        operator T**() { return mPtrPtr; }
        T& operator*() { return *mPtrPtr; }

    private:
        T** mPtrPtr;
};

template <class T>
inline nsRefPtrGetterAddRefs<T>
getter_AddRefs(nsRefPtr<T>& aSmartPtr)
{
    return nsRefPtrGetterAddRefs<T>(aSmartPtr);
}


#define EQ_OP(op_, lhtype_, rhtype_, lhget_, rhget_)                          \
    template <class T, class U>                                               \
    inline PRBool                                                             \
    operator op_( lhtype_ lhs, rhtype_ rhs )                                  \
    {                                                                         \
        return lhs lhget_ op_ rhs rhget_;                                     \
    }

#define SMART_SMART_EQ_OP(op_, type_, lhconst_, rhconst_)                     \
    EQ_OP(op_, const type_<lhconst_ T>&, const type_<rhconst_ U>&,            \
          .get(), .get())

#define SMART_RAW_EQ_OP(op_, type_, lhconst_, rhconst_)                       \
    EQ_OP(op_, const type_<lhconst_ T>&, rhconst_ U *, .get(), )

#define RAW_SMART_EQ_OP(op_, type_, lhconst_, rhconst_)                       \
    EQ_OP(op_, lhconst_ T *, const type_<rhconst_ U>&, , .get())

#define CONST_OPEQ_OPS(type_)                                                 \
    SMART_SMART_EQ_OP(==, type_, , const)                                     \
    SMART_SMART_EQ_OP(!=, type_, , const)                                     \
    SMART_SMART_EQ_OP(==, type_, const, )                                     \
    SMART_SMART_EQ_OP(!=, type_, const, )                                     \
    SMART_SMART_EQ_OP(==, type_, const, const)                                \
    SMART_SMART_EQ_OP(!=, type_, const, const)                                \
    SMART_RAW_EQ_OP(==, type_, , const)                                       \
    SMART_RAW_EQ_OP(!=, type_, , const)                                       \
    SMART_RAW_EQ_OP(==, type_, const, )                                       \
    SMART_RAW_EQ_OP(!=, type_, const, )                                       \
    SMART_RAW_EQ_OP(==, type_, const, const)                                  \
    SMART_RAW_EQ_OP(!=, type_, const, const)                                  \
    RAW_SMART_EQ_OP(==, type_, , const)                                       \
    RAW_SMART_EQ_OP(!=, type_, , const)                                       \
    RAW_SMART_EQ_OP(==, type_, const, )                                       \
    RAW_SMART_EQ_OP(!=, type_, const, )                                       \
    RAW_SMART_EQ_OP(==, type_, const, const)                                  \
    RAW_SMART_EQ_OP(!=, type_, const, const)

#define NON_CONST_OPEQ_OPS(type_)                                             \
    SMART_SMART_EQ_OP(==, type_, , )                                          \
    SMART_SMART_EQ_OP(!=, type_, , )                                          \
    SMART_RAW_EQ_OP(==, type_, , )                                            \
    SMART_RAW_EQ_OP(!=, type_, , )                                            \
    RAW_SMART_EQ_OP(==, type_, , )                                            \
    RAW_SMART_EQ_OP(!=, type_, , )

#ifdef NSCAP_DONT_PROVIDE_NONCONST_OPEQ
#define ALL_EQ_OPS(type_)                                                     \
    CONST_OPEQ_OPS(type_)
#else
#define ALL_EQ_OPS(type_)                                                     \
    NON_CONST_OPEQ_OPS(type_)                                                 \
    CONST_OPEQ_OPS(type_)
#endif

ALL_EQ_OPS(nsAutoPtr)
ALL_EQ_OPS(nsAutoArrayPtr)
ALL_EQ_OPS(nsMemoryAutoPtr)
ALL_EQ_OPS(nsRefPtr)
