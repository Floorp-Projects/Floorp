/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMemory_h__
#define nsMemory_h__

#include "nsXPCOM.h"

class nsIMemory;

#define NS_MEMORY_CONTRACTID "@mozilla.org/xpcom/memory-service;1"
#define NS_MEMORY_CID                                \
{ /* 30a04e40-38e7-11d4-8cf5-0060b0fc14a3 */         \
    0x30a04e40,                                      \
    0x38e7,                                          \
    0x11d4,                                          \
    {0x8c, 0xf5, 0x00, 0x60, 0xb0, 0xfc, 0x14, 0xa3} \
}


/**
 * Static helper routines to manage memory. These routines allow easy access
 * to xpcom's built-in (global) nsIMemory implementation, without needing
 * to go through the service manager to get it. However this requires clients
 * to link with the xpcom DLL.
 *
 * This class is not threadsafe and is intented for use only on the main
 * thread.
 */
class nsMemory
{
public:
  static NS_HIDDEN_(void*) Alloc(size_t aSize)
  {
    return NS_Alloc(aSize);
  }

  static NS_HIDDEN_(void*) Realloc(void* aPtr, size_t aSize)
  {
    return NS_Realloc(aPtr, aSize);
  }

  static NS_HIDDEN_(void) Free(void* aPtr)
  {
    NS_Free(aPtr);
  }

  static NS_COM_GLUE nsresult   HeapMinimize(bool aImmediate);
  static NS_COM_GLUE void*      Clone(const void* aPtr, size_t aSize);
  static NS_COM_GLUE nsIMemory* GetGlobalMemoryService();       // AddRefs
};

/**
 * Macro to free all elements of an XPCOM array of a given size using
 * freeFunc, then frees the array itself using nsMemory::Free().
 *
 * Note that this macro (and its wrappers) can be used to deallocate a
 * partially- or completely-built array while unwinding an error
 * condition inside the XPCOM routine that was going to return the
 * array.  For this to work on a partially-built array, your code
 * needs to be building the array from index 0 upwards, and simply
 * pass the number of elements that have already been built (and thus
 * need to be freed) as |size|.
 *
 * Thanks to <alecf@netscape.com> for suggesting this form, which
 * allows the macro to be used with NS_RELEASE / NS_RELEASE_IF in
 * addition to nsMemory::Free.
 *
 * @param size      Number of elements in the array.  If not a constant, this
 *                  should be a int32_t.  Note that this means this macro
 *                  will not work if size >= 2^31.
 * @param array     The array to be freed.
 * @param freeFunc  The function or macro to be used to free it.
 *                  For arrays of nsISupports (or any class derived
 *                  from it), NS_IF_RELEASE (or NS_RELEASE) should be
 *                  passed as freeFunc.  For most (all?) other pointer
 *                  types (including XPCOM strings and wstrings),
 *                  nsMemory::Free should be used, since the
 *                  shared-allocator (nsMemory) is what will have been
 *                  used to allocate the memory.
 */
#define NS_FREE_XPCOM_POINTER_ARRAY(size, array, freeFunc)                    \
    PR_BEGIN_MACRO                                                            \
        int32_t iter_ = int32_t(size);                                        \
        while (--iter_ >= 0)                                                  \
            freeFunc((array)[iter_]);                                         \
        NS_Free((array));                                                     \
    PR_END_MACRO

// convenience macros for commonly used calls.  mmmmm.  syntactic sugar.

/**
 * Macro to free arrays of non-refcounted objects allocated by the
 * shared allocator (nsMemory) such as strings and wstrings.  A
 * convenience wrapper around NS_FREE_XPCOM_POINTER_ARRAY.
 *
 * @param size      Number of elements in the array.  If not a constant, this
 *                  should be a int32_t.  Note that this means this macro
 *                  will not work if size >= 2^31.
 * @param array     The array to be freed.
 */
#define NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(size, array)                    \
    NS_FREE_XPCOM_POINTER_ARRAY((size), (array), NS_Free)

/**
 * Macro to free an array of pointers to nsISupports (or classes
 * derived from it).  A convenience wrapper around
 * NS_FREE_XPCOM_POINTER_ARRAY.
 *
 * Note that if you know that none of your nsISupports pointers are
 * going to be 0, you can gain a bit of speed by calling
 * NS_FREE_XPCOM_POINTER_ARRAY directly and using NS_RELEASE as your
 * free function.
 *
 * @param size      Number of elements in the array.  If not a constant, this
 *                  should be a int32_t.  Note that this means this macro
 *                  will not work if size >= 2^31.
 * @param array     The array to be freed.
 */
#define NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(size, array)                    \
    NS_FREE_XPCOM_POINTER_ARRAY((size), (array), NS_IF_RELEASE)

/**
 * A macro, NS_ALIGNMENT_OF(t_) that determines the alignment
 * requirements of a type.
 */
namespace mozilla {
template<class T>
struct AlignmentTestStruct
{
  char c;
  T t;
};
}

#define NS_ALIGNMENT_OF(t_) \
  (sizeof(mozilla::AlignmentTestStruct<t_>) - sizeof(t_))

/**
 * An enumeration type used to represent a method of assignment.
 */
enum nsAssignmentType
{
  NS_ASSIGNMENT_COPY,   // copy by value
  NS_ASSIGNMENT_DEPEND, // copy by reference
  NS_ASSIGNMENT_ADOPT   // copy by reference (take ownership of resource)
};

#endif // nsMemory_h__

