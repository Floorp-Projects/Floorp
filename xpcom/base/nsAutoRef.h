/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set shiftwidth=4 tabstop=8 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAutoRef_h_
#define nsAutoRef_h_

#include "mozilla/Attributes.h"

#include "nscore.h" // for nsnull, bool

template <class T> class nsSimpleRef;
template <class T> class nsAutoRefBase;
template <class T> class nsReturnRef;
template <class T> class nsReturningRef;

/**
 * template <class T> class nsAutoRef
 *
 * A class that holds a handle to a resource that must be released.
 * No reference is added on construction.
 *
 * No copy constructor nor copy assignment operators are available, so the
 * resource will be held until released on destruction or explicitly
 * |reset()| or transferred through provided methods.
 *
 * The publicly available methods are the public methods on this class and its
 * public base classes |nsAutoRefBase<T>| and |nsSimpleRef<T>|.
 *
 * For ref-counted resources see also |nsCountedRef<T>|.
 * For function return values see |nsReturnRef<T>|.
 *
 * For each class |T|, |nsAutoRefTraits<T>| or |nsSimpleRef<T>| must be
 * specialized to use |nsAutoRef<T>| and |nsCountedRef<T>|.
 *
 * @param T  A class identifying the type of reference held by the
 *           |nsAutoRef<T>| and the unique set methods for managing references
 *           to the resource (defined by |nsAutoRefTraits<T>| or
 *           |nsSimpleRef<T>|).
 *
 *           Often this is the class representing the resource.  Sometimes a
 *           new possibly-incomplete class may need to be declared.
 *
 *
 * Example:  An Automatically closing file descriptor
 *
 * // References that are simple integral types (as file-descriptors are)
 * // usually need a new class to represent the resource and how to handle its
 * // references.
 * class nsRawFD;
 *
 * // Specializing nsAutoRefTraits<nsRawFD> describes how to manage file
 * // descriptors, so that nsAutoRef<nsRawFD> provides automatic closing of
 * // its file descriptor on destruction.
 * template <>
 * class nsAutoRefTraits<nsRawFD> {
 * public:
 *     // The file descriptor is held in an int.
 *     typedef int RawRef;
 *     // -1 means that there is no file associated with the handle.
 *     static int Void() { return -1; }
 *     // The file associated with a file descriptor is released with close().
 *     static void Release(RawRef aFD) { close(aFD); }
 * };
 *
 * // A function returning a file descriptor that must be closed.
 * nsReturnRef<nsRawFD> get_file(const char *filename) {
 *     // Constructing from a raw file descriptor assumes ownership.
 *     nsAutoRef<nsRawFD> fd(open(filename, O_RDONLY));
 *     fcntl(fd, F_SETFD, FD_CLOEXEC);
 *     return fd.out();
 * }
 *
 * void f() {
 *     unsigned char buf[1024];
 *
 *     // Hold a file descriptor for /etc/hosts in fd1.
 *     nsAutoRef<nsRawFD> fd1(get_file("/etc/hosts"));
 *
 *     nsAutoRef<nsRawFD> fd2;
 *     fd2.steal(fd1); // fd2 takes the file descriptor from fd1
 *     ssize_t count = read(fd1, buf, 1024); // error fd1 has no file
 *     count = read(fd2, buf, 1024); // reads from /etc/hosts
 *
 *     // If the file descriptor is not stored then it is closed.
 *     get_file("/etc/login.defs"); // login.defs is closed
 *
 *     // Now use fd1 to hold a file descriptor for /etc/passwd.
 *     fd1 = get_file("/etc/passwd");
 *
 *     // The nsAutoRef<nsRawFD> can give up the file descriptor if explicitly
 *     // instructed, but the caller must then ensure that the file is closed.
 *     int rawfd = fd1.disown();
 *
 *     // Assume ownership of another file descriptor.
 *     fd1.own(open("/proc/1/maps");
 *
 *     // On destruction, fd1 closes /proc/1/maps and fd2 closes /etc/hosts,
 *     // but /etc/passwd is not closed.
 * }
 *
 */


template <class T>
class nsAutoRef : public nsAutoRefBase<T>
{
protected:
    typedef nsAutoRef<T> ThisClass;
    typedef nsAutoRefBase<T> BaseClass;
    typedef nsSimpleRef<T> SimpleRef;
    typedef typename BaseClass::RawRefOnly RawRefOnly;
    typedef typename BaseClass::LocalSimpleRef LocalSimpleRef;

public:
    nsAutoRef()
    {
    }

    // Explicit construction is required so as not to risk unintentionally
    // releasing the resource associated with a raw ref.
    explicit nsAutoRef(RawRefOnly aRefToRelease)
        : BaseClass(aRefToRelease)
    {
    }

    // Construction from a nsReturnRef<T> function return value, which expects
    // to give up ownership, transfers ownership.
    // (nsReturnRef<T> is converted to const nsReturningRef<T>.)
    explicit nsAutoRef(const nsReturningRef<T>& aReturning)
        : BaseClass(aReturning)
    {
    }

    // The only assignment operator provided is for transferring from an
    // nsReturnRef smart reference, which expects to pass its ownership to
    // another object.
    //
    // With raw references and other smart references, the type of the lhs and
    // its taking and releasing nature is often not obvious from an assignment
    // statement.  Assignment from a raw ptr especially is not normally
    // expected to release the reference.
    //
    // Use |steal| for taking ownership from other smart refs.
    //
    // For raw references, use |own| to indicate intention to have the
    // resource released.
    //
    // Or, to create another owner of the same reference, use an nsCountedRef.

    ThisClass& operator=(const nsReturningRef<T>& aReturning)
    {
        BaseClass::steal(aReturning.mReturnRef);
        return *this;
    }

    // Conversion to a raw reference allow the nsAutoRef<T> to often be used
    // like a raw reference.
    operator typename SimpleRef::RawRef() const
    {
        return this->get();
    }

    // Transfer ownership from another smart reference.
    void steal(ThisClass& aOtherRef)
    {
        BaseClass::steal(aOtherRef);
    }

    // Assume ownership of a raw ref.
    //
    // |own| has similar function to |steal|, and is useful for receiving
    // ownership from a return value of a function.  It is named differently
    // because |own| requires more care to ensure that the function intends to
    // give away ownership, and so that |steal| can be safely used, knowing
    // that it won't steal ownership from any methods returning raw ptrs to
    // data owned by a foreign object.
    void own(RawRefOnly aRefToRelease)
    {
        BaseClass::own(aRefToRelease);
    }

    // Exchange ownership with |aOther|
    void swap(ThisClass& aOther)
    {
        LocalSimpleRef temp;
        temp.SimpleRef::operator=(*this);
        SimpleRef::operator=(aOther);
        aOther.SimpleRef::operator=(temp);
    }

    // Release the reference now.
    void reset()
    {
        this->SafeRelease();
        LocalSimpleRef empty;
        SimpleRef::operator=(empty);
    }

    // Pass out the reference for a function return values.
    nsReturnRef<T> out()
    {
        return nsReturnRef<T>(this->disown());
    }

    // operator->() and disown() are provided by nsAutoRefBase<T>.
    // The default nsSimpleRef<T> provides get().

private:
    // No copy constructor
    explicit nsAutoRef(ThisClass& aRefToSteal);
};

/**
 * template <class T> class nsCountedRef
 *
 * A class that creates (adds) a new reference to a resource on construction
 * or assignment and releases on destruction.
 *
 * This class is similar to nsAutoRef<T> and inherits its methods, but also
 * provides copy construction and assignment operators that enable more than
 * one concurrent reference to the same resource.
 *
 * Specialize |nsAutoRefTraits<T>| or |nsSimpleRef<T>| to use this.  This
 * class assumes that the resource itself counts references and so can only be
 * used when |T| represents a reference-counting resource.
 */

template <class T>
class nsCountedRef : public nsAutoRef<T>
{
protected:
    typedef nsCountedRef<T> ThisClass;
    typedef nsAutoRef<T> BaseClass;
    typedef nsSimpleRef<T> SimpleRef;
    typedef typename BaseClass::RawRef RawRef;

public:
    nsCountedRef()
    {
    }

    // Construction and assignment from a another nsCountedRef
    // or a raw ref copies and increments the ref count.
    nsCountedRef(const ThisClass& aRefToCopy)
    {
        SimpleRef::operator=(aRefToCopy);
        SafeAddRef();
    }
    ThisClass& operator=(const ThisClass& aRefToCopy)
    {
        if (this == &aRefToCopy)
            return *this;

        this->SafeRelease();
        SimpleRef::operator=(aRefToCopy);
        SafeAddRef();
        return *this;
    }

    // Implicit conversion from another smart ref argument (to a raw ref) is
    // accepted here because construction and assignment safely creates a new
    // reference without interfering with the reference to copy.
    explicit nsCountedRef(RawRef aRefToCopy)
        : BaseClass(aRefToCopy)
    {
        SafeAddRef();
    }
    ThisClass& operator=(RawRef aRefToCopy)
    {
        this->own(aRefToCopy);
        SafeAddRef();
        return *this;
    }

    // Construction and assignment from an nsReturnRef function return value,
    // which expects to give up ownership, transfers ownership.
    explicit nsCountedRef(const nsReturningRef<T>& aReturning)
        : BaseClass(aReturning)
    {
    }
    ThisClass& operator=(const nsReturningRef<T>& aReturning)
    {
        BaseClass::operator=(aReturning);
        return *this;
    }

protected:
    // Increase the reference count if there is a resource.
    void SafeAddRef()
    {
        if (this->HaveResource())
            this->AddRef(this->get());
    }
};

/**
 * template <class T> class nsReturnRef
 *
 * A type for function return values that hold a reference to a resource that
 * must be released.  See also |nsAutoRef<T>::out()|.
 */

template <class T>
class nsReturnRef : public nsAutoRefBase<T>
{
protected:
    typedef nsAutoRefBase<T> BaseClass;
    typedef typename BaseClass::RawRefOnly RawRefOnly;

public:
    // For constructing a return value with no resource
    nsReturnRef()
    {
    }

    // For returning a smart reference from a raw reference that must be
    // released.  Explicit construction is required so as not to risk
    // unintentionally releasing the resource associated with a raw ref.
    explicit nsReturnRef(RawRefOnly aRefToRelease)
        : BaseClass(aRefToRelease)
    {
    }

    // Copy construction transfers ownership
    nsReturnRef(nsReturnRef<T>& aRefToSteal)
        : BaseClass(aRefToSteal)
    {
    }

    nsReturnRef(const nsReturningRef<T>& aReturning)
        : BaseClass(aReturning)
    {
    }

    // Conversion to a temporary (const) object referring to this object so
    // that the reference may be passed from a function return value
    // (temporary) to another smart reference.  There is no need to use this
    // explicitly.  Simply assign a nsReturnRef<T> function return value to a
    // smart reference.
    operator nsReturningRef<T>()
    {
        return nsReturningRef<T>(*this);
    }

    // No conversion to RawRef operator is provided on nsReturnRef, to ensure
    // that the return value is not carelessly assigned to a raw ptr (and the
    // resource then released).  If passing to a function that takes a raw
    // ptr, use get or disown as appropriate.
};

/**
 * template <class T> class nsReturningRef
 *
 * A class to allow ownership to be transferred from nsReturnRef function
 * return values.
 *
 * It should not be necessary for clients to reference this
 * class directly.  Simply pass an nsReturnRef<T> to a parameter taking an
 * |nsReturningRef<T>|.
 *
 * The conversion operator on nsReturnRef constructs a temporary wrapper of
 * class nsReturningRef<T> around a non-const reference to the nsReturnRef.
 * The wrapper can then be passed as an rvalue parameter.
 */

template <class T>
class nsReturningRef
{
private:
    friend class nsReturnRef<T>;

    explicit nsReturningRef(nsReturnRef<T>& aReturnRef)
        : mReturnRef(aReturnRef)
    {
    }
public:
    nsReturnRef<T>& mReturnRef;
};

/**
 * template <class T> class nsAutoRefTraits
 *
 * A class describing traits of references managed by the default
 * |nsSimpleRef<T>| implementation and thus |nsAutoRef<T>| and |nsCountedRef|.
 * The default |nsSimpleRef<T> is suitable for resources with handles that
 * have a void value.  (If there is no such void value for a handle,
 * specialize |nsSimpleRef<T>|.)
 *
 * Specializations must be provided for each class |T| according to the
 * following pattern:
 *
 * // The template parameter |T| should be a class such that the set of fields
 * // in class nsAutoRefTraits<T> is unique for class |T|.  Usually the
 * // resource object class is sufficient.  For handles that are simple
 * // integral typedefs, a new unique possibly-incomplete class may need to be
 * // declared.
 *
 * template <>
 * class nsAutoRefTraits<T>
 * {
 *     // Specializations must provide a typedef for RawRef, describing the
 *     // type of the handle to the resource.
 *     typedef <handle-type> RawRef;
 *
 *     // Specializations should define Void(), a function returning a value
 *     // suitable for a handle that does not have an associated resource.
 *     //
 *     // The return type must be a suitable as the parameter to a RawRef
 *     // constructor and operator==.
 *     //
 *     // If this method is not accessible then some limited nsAutoRef
 *     // functionality will still be available, but the default constructor,
 *     // |reset|, and most transfer of ownership methods will not be available.
 *     static <return-type> Void();
 *
 *     // Specializations must define Release() to properly finalize the
 *     // handle to a non-void custom-deleted or reference-counted resource.
 *     static void Release(RawRef aRawRef);
 *
 *     // For reference-counted resources, if |nsCountedRef<T>| is to be used,
 *     // specializations must define AddRef to increment the reference count
 *     // held by a non-void handle.
 *     // (AddRef() is not necessary for |nsAutoRef<T>|.)
 *     static void AddRef(RawRef aRawRef);
 * };
 *
 * See nsPointerRefTraits for example specializations for simple pointer
 * references.  See nsAutoRef for an example specialization for a non-pointer
 * reference.
 */

template <class T> class nsAutoRefTraits;

/**
 * template <class T> class nsPointerRefTraits
 *
 * A convenience class useful as a base class for specializations of
 * |nsAutoRefTraits<T>| where the handle to the resource is a pointer to |T|.
 * By inheriting from this class, definitions of only Release(RawRef) and
 * possibly AddRef(RawRef) need to be added.
 *
 * Examples of use:
 *
 * template <>
 * class nsAutoRefTraits<PRFileDesc> : public nsPointerRefTraits<PRFileDesc>
 * {
 * public:
 *     static void Release(PRFileDesc *ptr) { PR_Close(ptr); }
 * };
 *
 * template <>
 * class nsAutoRefTraits<FcPattern> : public nsPointerRefTraits<FcPattern>
 * {
 * public:
 *     static void Release(FcPattern *ptr) { FcPatternDestroy(ptr); }
 *     static void AddRef(FcPattern *ptr) { FcPatternReference(ptr); }
 * };
 */

template <class T>
class nsPointerRefTraits
{
public:
    // The handle is a pointer to T.
    typedef T* RawRef;
    // A NULL pointer does not have a resource.
    static RawRef Void() { return nsnull; };
};

/**
 * template <class T> class nsSimpleRef
 *
 * Constructs a non-smart reference, and provides methods to test whether
 * there is an associated resource and (if so) get its raw handle.
 *
 * A default implementation is suitable for resources with handles that have a
 * void value.  This is not intended for direct use but used by |nsAutoRef<T>|
 * and thus |nsCountedRef<T>|.
 *
 * Specialize this class if there is no particular void value for the resource
 * handle.  A specialized implementation must also provide Release(RawRef),
 * and, if |nsCountedRef<T>| is required, AddRef(RawRef), as described in
 * nsAutoRefTraits<T>.
 */

template <class T>
class nsSimpleRef : protected nsAutoRefTraits<T>
{
protected:
    // The default implementation uses nsAutoRefTrait<T>.
    // Specializations need not define this typedef.
    typedef nsAutoRefTraits<T> Traits;
    // The type of the handle to the resource.
    // A specialization must provide a typedef for RawRef.
    typedef typename Traits::RawRef RawRef;

    // Construct with no resource.
    //
    // If this constructor is not accessible then some limited nsAutoRef
    // functionality will still be available, but the default constructor,
    // |reset|, and most transfer of ownership methods will not be available.
    nsSimpleRef()
        : mRawRef(Traits::Void())
    {
    }
    // Construct with a handle to a resource.
    // A specialization must provide this. 
    nsSimpleRef(RawRef aRawRef)
        : mRawRef(aRawRef)
    {
    }

    // Test whether there is an associated resource.  A specialization must
    // provide this.  The function is permitted to always return true if the
    // default constructor is not accessible, or if Release (and AddRef) can
    // deal with void handles.
    bool HaveResource() const
    {
        return mRawRef != Traits::Void();
    }

public:
    // A specialization must provide get() or loose some functionality.  This
    // is inherited by derived classes and the specialization may choose
    // whether it is public or protected.
    RawRef get() const
    {
        return mRawRef;
    }

private:
    RawRef mRawRef;
};


/**
 * template <class T> class nsAutoRefBase
 *
 * Internal base class for |nsAutoRef<T>| and |nsReturnRef<T>|.
 * Adds release on destruction to a |nsSimpleRef<T>|.
 */

template <class T>
class nsAutoRefBase : public nsSimpleRef<T>
{
protected:
    typedef nsAutoRefBase<T> ThisClass;
    typedef nsSimpleRef<T> SimpleRef;
    typedef typename SimpleRef::RawRef RawRef;

    nsAutoRefBase()
    {
    }

    // A type for parameters that should be passed a raw ref but should not
    // accept implicit conversions (from another smart ref).  (The only
    // conversion to this type is from a raw ref so only raw refs will be
    // accepted.)
    class RawRefOnly
    {
    public:
        RawRefOnly(RawRef aRawRef)
            : mRawRef(aRawRef)
        {
        }
        operator RawRef() const
        {
            return mRawRef;
        }
    private:
        RawRef mRawRef;
    };

    // Construction from a raw ref assumes ownership
    explicit nsAutoRefBase(RawRefOnly aRefToRelease)
        : SimpleRef(aRefToRelease)
    {
    }

    // Constructors that steal ownership
    explicit nsAutoRefBase(ThisClass& aRefToSteal)
        : SimpleRef(aRefToSteal.disown())
    {
    }
    explicit nsAutoRefBase(const nsReturningRef<T>& aReturning)
        : SimpleRef(aReturning.mReturnRef.disown())
    {
    }

    ~nsAutoRefBase()
    {
        SafeRelease();
    }

    // An internal class providing access to protected nsSimpleRef<T>
    // constructors for construction of temporary simple references (that are
    // not ThisClass).
    class LocalSimpleRef : public SimpleRef
    {
    public:
        LocalSimpleRef()
        {
        }
        explicit LocalSimpleRef(RawRef aRawRef)
            : SimpleRef(aRawRef)
        {
        }
    };

private:
    ThisClass& operator=(const ThisClass& aSmartRef) MOZ_DELETE;
    
public:
    RawRef operator->() const
    {
        return this->get();
    }

    // Transfer ownership to a raw reference.
    //
    // THE CALLER MUST ENSURE THAT THE REFERENCE IS EXPLICITLY RELEASED.
    //
    // Is this really what you want to use?  Using this removes any guarantee
    // of release.  Use nsAutoRef<T>::out() for return values, or an
    // nsAutoRef<T> modifiable lvalue for an out parameter.  Use disown() when
    // the reference must be stored in a POD type object, such as may be
    // preferred for a namespace-scope object with static storage duration,
    // for example.
    RawRef disown()
    {
        RawRef temp = this->get();
        LocalSimpleRef empty;
        SimpleRef::operator=(empty);
        return temp;
    }

protected:
    // steal and own are protected because they make no sense on nsReturnRef,
    // but steal is implemented on this class for access to aOtherRef.disown()
    // when aOtherRef is an nsReturnRef;

    // Transfer ownership from another smart reference.
    void steal(ThisClass& aOtherRef)
    {
        own(aOtherRef.disown());
    }
    // Assume ownership of a raw ref.
    void own(RawRefOnly aRefToRelease)
    {
        SafeRelease();
        LocalSimpleRef ref(aRefToRelease);
        SimpleRef::operator=(ref);
    }

    // Release a resource if there is one.
    void SafeRelease()
    {
        if (this->HaveResource())
            this->Release(this->get());
    }
};

#endif // !defined(nsAutoRef_h_)
