/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* nsXPIDLString.h --- an `|auto_ptr|' for character buffers, this functionality will be replaced
    by the new shared-buffer string (see bug #53065)
 */




#ifndef nsXPIDLString_h__
#define nsXPIDLString_h__

/*

  A set of string wrapper classes that ease transition to use of XPIDL
  interfaces. nsXPIDLString and nsXPIDLCString are to XPIDL `wstring'
  and `string' out params as nsCOMPtr is to generic XPCOM interface
  pointers. They help you deal with object ownership.

  Consider the following interface:

    interface nsIFoo {
      attribute string Bar;
    };

  This will generate the following C++ header file:

    class nsIFoo {
      NS_IMETHOD SetBar(const PRUnichar* aValue);
      NS_IMETHOD GetBar(PRUnichar* *aValue);
    };

  The GetBar() method will allocate a copy of the nsIFoo object's
  "bar" attribute, and leave you to deal with freeing it:

    nsIFoo* aFoo; // assume we get this somehow
    PRUnichar* bar;
    aFoo->GetFoo(&bar);
    // Use bar here...
    printf("bar is %s!\n", bar);
    nsMemory::Free(bar);

  This makes your life harder, because you need to convolute your code
  to ensure that you don't leak `bar'.

  Enter nsXPIDLString, which manages the ownership of the allocated
  string, and automatically destroys it when the nsXPIDLString goes
  out of scope:

    nsIFoo* aFoo;
    nsXPIDLString bar;
    aFoo->GetFoo( getter_Copies(bar) );
    // Use bar here...
    printf("bar is %s!\n", (const char*) bar);
    // no need to remember to nsMemory::Free().

  Like nsCOMPtr, nsXPIDLString uses some syntactic sugar to make it
  painfully clear exactly what the code expects. You need to wrap an
  nsXPIDLString object with either `getter_Copies()' or
  `getter_Shares()' before passing it to a getter: these tell the
  nsXPIDLString how ownership is being handled.

  In the case of `getter_Copies()', the callee is allocating a copy
  (which is usually the case). In the case of `getter_Shares()', the
  callee is returning a const reference to `the real deal' (this can
  be done using the [shared] attribute in XPIDL).

 */

#include "nscore.h"
#include "nsCom.h"
#include "prtypes.h"

#ifndef __PRUNICHAR__
#define __PRUNICHAR__
typedef PRUint16 PRUnichar;
#endif /* __PRUNICHAR__ */

////////////////////////////////////////////////////////////////////////
// nsXPIDLString
//
//   A wrapper for Unicode strings. With the |getter_Copies()| and
//   |getter_Shares()| helper functions, this can be used instead of
//   the "naked" |PRUnichar*| interface for |wstring| parameters in
//   XPIDL interfaces.
//

class NS_COM nsXPIDLString {
private:
    PRUnichar* mBuf;
    PRBool     mBufOwner;

    PRUnichar** StartAssignmentByValue();
    const PRUnichar** StartAssignmentByReference();

public:
    /**
     * Construct a new, uninitialized wrapper for a Unicode string.
     */
    nsXPIDLString() : mBuf(0), mBufOwner(PR_FALSE) {}

    ~nsXPIDLString();

    /**
     * Return a reference to the immutable Unicode string.
     */
    operator const PRUnichar*() const { return get(); }

    /**
     * Return a reference to the immutable Unicode string.
     */
    const PRUnichar* get() const { return mBuf; }

    /**
     * Make a copy of the Unicode string. Use this function in the
     * callee to ensure that the correct memory allocator is used.
     */
    static PRUnichar* Copy(const PRUnichar* aString);

    // A helper class for assignment-by-value. This class is an
    // implementation detail and should not be considered part of the
    // public interface.
    class NS_COM GetterCopies {
    private:
        nsXPIDLString& mXPIDLString;

    public:
        GetterCopies(nsXPIDLString& aXPIDLString)
            : mXPIDLString(aXPIDLString) {}

        operator PRUnichar**() {
            return mXPIDLString.StartAssignmentByValue();
        }

        friend GetterCopies getter_Copies(nsXPIDLString& aXPIDLString);
    };

    friend class GetterCopies;

    // A helper class for assignment-by-reference. This class is an
    // implementation detail and should not be considered part of the
    // public interface.
    class NS_COM GetterShares {
    private:
        nsXPIDLString& mXPIDLString;

    public:
        GetterShares(nsXPIDLString& aXPIDLString)
            : mXPIDLString(aXPIDLString) {}

        operator const PRUnichar**() {
            return mXPIDLString.StartAssignmentByReference();
        }

        friend GetterShares getter_Shares(nsXPIDLString& aXPIDLString);
    };

    friend class GetterShares;

private:
    // not to be implemented
    nsXPIDLString(nsXPIDLString& /* aXPIDLString */) {}
    void operator=(nsXPIDLString& /* aXPIDLString */) {}
};


/**
 * Use this function to "wrap" the nsXPIDLString object that is to
 * receive an |out| value.
 */
inline nsXPIDLString::GetterCopies
getter_Copies(nsXPIDLString& aXPIDLString)
{
    return nsXPIDLString::GetterCopies(aXPIDLString);
}

/**
 * Use this function to "wrap" the nsXPIDLString object that is to
 * receive a |[shared] out| value.
 */
inline nsXPIDLString::GetterShares
getter_Shares(nsXPIDLString& aXPIDLString)
{
    return nsXPIDLString::GetterShares(aXPIDLString);
}


// XXX THESE ARE NOT strcmp()! DON'T TRY TO USE THEM AS SUCH!
inline
PRBool
operator==(const PRUnichar* lhs, const nsXPIDLString& rhs)
{
    return lhs == NS_STATIC_CAST(const PRUnichar*, rhs);
}

inline
PRBool
operator==(const nsXPIDLString& lhs, const PRUnichar* rhs)
{
    return NS_STATIC_CAST(const PRUnichar*, lhs) == rhs;
}


#ifdef HAVE_CPP_TROUBLE_COMPARING_TO_ZERO

inline
PRBool
operator==(int lhs, const nsXPIDLString& rhs)
{
    return NS_REINTERPRET_CAST(PRUnichar*, lhs) == NS_STATIC_CAST(const PRUnichar*, rhs);
}

inline
PRBool
operator==(const nsXPIDLString& lhs, int rhs)
{
    return NS_STATIC_CAST(const PRUnichar*, lhs) == NS_REINTERPRET_CAST(PRUnichar*, rhs);
}

#endif

////////////////////////////////////////////////////////////////////////
// nsXPIDLCString
//
//   A wrapper for Unicode strings. With the |getter_Copies()| and
//   |getter_Shares()| helper functions, this can be used instead of
//   the "naked" |char*| interface for |string| parameters in XPIDL
//   interfaces.
//

class NS_COM nsXPIDLCString {
private:
    char*  mBuf;
    PRBool mBufOwner;

    char** StartAssignmentByValue();
    const char** StartAssignmentByReference();

public:
    /**
     * Construct a new, uninitialized wrapper for a single-byte string.
     */
    nsXPIDLCString() : mBuf(0), mBufOwner(PR_FALSE) {}

    ~nsXPIDLCString();

    /**
     * Assign a single-byte string to this wrapper. Copies
     * and owns the result.
     */
    nsXPIDLCString& operator=(const char* aString);

    /**
     * Return a reference to the immutable single-byte string.
     */
    operator const char*() const { return get(); }

    /**
     * Return a reference to the immutable single-byte string.
     */
    const char* get() const { return mBuf; }

    /**
     * Make a copy of the single-byte string. Use this function in the
     * callee to ensure that the correct memory allocator is used.
     */
    static char* Copy(const char* aString);

    // A helper class for assignment-by-value. This class is an
    // implementation detail and should not be considered part of the
    // public interface.
    class NS_COM GetterCopies {
    private:
        nsXPIDLCString& mXPIDLString;

    public:
        GetterCopies(nsXPIDLCString& aXPIDLString)
            : mXPIDLString(aXPIDLString) {}

        operator char**() {
            return mXPIDLString.StartAssignmentByValue();
        }

        friend GetterCopies getter_Copies(nsXPIDLCString& aXPIDLString);
    };

    friend class GetterCopies;

    // A helper class for assignment-by-reference. This class is an
    // implementation detail and should not be considered part of the
    // public interface.
    class NS_COM GetterShares {
    private:
        nsXPIDLCString& mXPIDLString;

    public:
        GetterShares(nsXPIDLCString& aXPIDLString)
            : mXPIDLString(aXPIDLString) {}

        operator const char**() {
            return mXPIDLString.StartAssignmentByReference();
        }

        friend GetterShares getter_Shares(nsXPIDLCString& aXPIDLString);
    };

    friend class GetterShares;

private:
    // not to be implemented
    nsXPIDLCString(nsXPIDLCString& /* aXPIDLString */) {}
    void operator=(nsXPIDLCString& /* aXPIDLCString */) {}
};

/**
 * Use this function to "wrap" the nsXPIDLCString object that is to
 * receive an |out| value.
 */
inline nsXPIDLCString::GetterCopies
getter_Copies(nsXPIDLCString& aXPIDLString)
{
    return nsXPIDLCString::GetterCopies(aXPIDLString);
}


/**
 * Use this function to "wrap" the nsXPIDLCString object that is to
 * receive a |[shared] out| value.
 */
inline nsXPIDLCString::GetterShares
getter_Shares(nsXPIDLCString& aXPIDLString)
{
    return nsXPIDLCString::GetterShares(aXPIDLString);
}

// XXX THESE ARE NOT strcmp()! DON'T TRY TO USE THEM AS SUCH!
inline
PRBool
operator==(const char* lhs, const nsXPIDLCString& rhs)
{
    return lhs == NS_STATIC_CAST(const char*, rhs);
}

inline
PRBool
operator==(const nsXPIDLCString& lhs, const char* rhs)
{
    return NS_STATIC_CAST(const char*, lhs) == rhs;
}

#ifdef HAVE_CPP_TROUBLE_COMPARING_TO_ZERO

inline
PRBool
operator==(int lhs, const nsXPIDLCString& rhs)
{
    return NS_REINTERPRET_CAST(char*, lhs) == NS_STATIC_CAST(const char*, rhs);
}

inline
PRBool
operator==(const nsXPIDLCString& lhs, int rhs)
{
    return NS_STATIC_CAST(const char*, lhs) == NS_REINTERPRET_CAST(char*, rhs);
}

#endif

#endif // nsXPIDLString_h__
