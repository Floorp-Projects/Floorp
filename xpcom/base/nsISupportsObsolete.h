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
 * The Original Code is XPCOM.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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


#ifndef nsISupportsObsolete_h__
#define nsISupportsObsolete_h__

#if defined(NS_MT_SUPPORTED)
#include "prcmon.h"
#endif  /* NS_MT_SUPPORTED */
///////////////////////////////////////////////////////////////////////////////


#define NS_INIT_REFCNT() NS_INIT_ISUPPORTS()


#define NS_DECL_ISUPPORTS_EXPORTED                                            \
public:                                                                       \
  NS_EXPORT NS_IMETHOD QueryInterface(REFNSIID aIID,                          \
                            void** aInstancePtr);                             \
  NS_EXPORT NS_IMETHOD_(nsrefcnt) AddRef(void);                               \
  NS_EXPORT NS_IMETHOD_(nsrefcnt) Release(void);                              \
protected:                                                                    \
  nsrefcnt mRefCnt;                                                           \
  NS_DECL_OWNINGTHREAD                                                        \
public:


#ifdef NS_DEBUG

/*
 * Adding this debug-only function as per bug #26803.  If you are debugging
 * and this function returns wrong refcounts, fix the objects |AddRef()| and
 * |Release()| to do the right thing.
 *
 * Of course, this function is only available for debug builds.
 */

inline
nsrefcnt
NS_DebugGetRefCount( nsISupports* obj )
    // Warning: don't apply this to an object whose refcount is
    //  |0| or not yet initialized ... it may be destroyed.
  {
    nsrefcnt ref_count = 0;

    if ( obj )
      {
          // |AddRef()| and |Release()| are supposed to return
          //  the new refcount of the object
        obj->AddRef();
        ref_count = obj->Release();
          // Can't use |NS_[ADDREF|RELEASE]| since (a) they _don't_ return
          //  the refcount, and (b) we don't want to log these guaranteed
          //  balanced calls.

        NS_ASSERTION(ref_count,
                     "Oops! Calling |NS_DebugGetRefCount()| probably just "
                     "destroyed this object.");
      }

     return ref_count;
  }

#endif // NS_DEBUG

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
 *                  should be a PRInt32.  Note that this means this macro 
 *                  will not work if size >= 2^31.
 * @param array     The array to be freed.
 */
#define NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(size, array)                    \
    NS_FREE_XPCOM_POINTER_ARRAY((size), (array), NS_IF_RELEASE)


///////////////////////////////////////////////////////////////////////////////

/* use these functions to associate get/set methods with a
   C++ member variable
*/

#define NS_METHOD_GETTER(_method, _type, _member) \
_method(_type* aResult) \
{\
    if (!aResult) return NS_ERROR_NULL_POINTER; \
    *aResult = _member; \
    return NS_OK; \
}
    
#define NS_METHOD_SETTER(_method, _type, _member) \
_method(_type aResult) \
{ \
    _member = aResult; \
    return NS_OK; \
}

/*
 * special for strings to get/set char* strings
 * using PL_strdup and PR_FREEIF
 */
#define NS_METHOD_GETTER_STR(_method,_member) \
_method(char* *aString)\
{\
    if (!aString) return NS_ERROR_NULL_POINTER; \
    *aString = PL_strdup(_member); \
    return NS_OK; \
}

#define NS_METHOD_SETTER_STR(_method, _member) \
_method(const char *aString)\
{\
    PR_FREEIF(_member);\
    if (aString) _member = PL_strdup(aString); \
    else _member = nsnull;\
    return NS_OK; \
}

/* Getter/Setter macros.
   Usage:
   NS_IMPL_[CLASS_]GETTER[_<type>](method, [type,] member);
   NS_IMPL_[CLASS_]SETTER[_<type>](method, [type,] member);
   NS_IMPL_[CLASS_]GETSET[_<type>]([class, ]postfix, [type,] member);
   
   where:
   CLASS_  - implementation is inside a class definition
             (otherwise the class name is needed)
             Do NOT use in publicly exported header files, because
             the implementation may be included many times over.
             Instead, use the non-CLASS_ version.
   _<type> - For more complex (STR, IFACE) data types
             (otherwise the simple data type is needed)
   method  - name of the method, such as GetWidth or SetColor
   type    - simple data type if required
   member  - class member variable such as m_width or mColor
   class   - the class name, such as Window or MyObject
   postfix - Method part after Get/Set such as "Width" for "GetWidth"
   
   Example:
   class Window {
   public:
     NS_IMPL_CLASS_GETSET(Width, int, m_width);
     NS_IMPL_CLASS_GETTER_STR(GetColor, m_color);
     NS_IMETHOD SetColor(char *color);
     
   private:
     int m_width;     // read/write
     char *m_color;   // readonly
   };

   // defined outside of class
   NS_IMPL_SETTER_STR(Window::GetColor, m_color);

   Questions/Comments to alecf@netscape.com
*/

   
/*
 * Getter/Setter implementation within a class definition
 */

/* simple data types */
#define NS_IMPL_CLASS_GETTER(_method, _type, _member) \
NS_IMETHOD NS_METHOD_GETTER(_method, _type, _member)

#define NS_IMPL_CLASS_SETTER(_method, _type, _member) \
NS_IMETHOD NS_METHOD_SETTER(_method, _type, _member)

#define NS_IMPL_CLASS_GETSET(_postfix, _type, _member) \
NS_IMPL_CLASS_GETTER(Get##_postfix, _type, _member) \
NS_IMPL_CLASS_SETTER(Set##_postfix, _type, _member)

/* strings */
#define NS_IMPL_CLASS_GETTER_STR(_method, _member) \
NS_IMETHOD NS_METHOD_GETTER_STR(_method, _member)

#define NS_IMPL_CLASS_SETTER_STR(_method, _member) \
NS_IMETHOD NS_METHOD_SETTER_STR(_method, _member)

#define NS_IMPL_CLASS_GETSET_STR(_postfix, _member) \
NS_IMPL_CLASS_GETTER_STR(Get##_postfix, _member) \
NS_IMPL_CLASS_SETTER_STR(Set##_postfix, _member)

/* Getter/Setter implementation outside of a class definition */

/* simple data types */
#define NS_IMPL_GETTER(_method, _type, _member) \
NS_IMETHODIMP NS_METHOD_GETTER(_method, _type, _member)

#define NS_IMPL_SETTER(_method, _type, _member) \
NS_IMETHODIMP NS_METHOD_SETTER(_method, _type, _member)

#define NS_IMPL_GETSET(_class, _postfix, _type, _member) \
NS_IMPL_GETTER(_class::Get##_postfix, _type, _member) \
NS_IMPL_SETTER(_class::Set##_postfix, _type, _member)

/* strings */
#define NS_IMPL_GETTER_STR(_method, _member) \
NS_IMETHODIMP NS_METHOD_GETTER_STR(_method, _member)

#define NS_IMPL_SETTER_STR(_method, _member) \
NS_IMETHODIMP NS_METHOD_SETTER_STR(_method, _member)

#define NS_IMPL_GETSET_STR(_class, _postfix, _member) \
NS_IMPL_GETTER_STR(_class::Get##_postfix, _member) \
NS_IMPL_SETTER_STR(_class::Set##_postfix, _member)

/**
 * IID for the nsIsThreadsafe interface
 * {88210890-47a6-11d2-bec3-00805f8a66dc}
 *
 * This interface is *only* used for debugging purposes to determine if
 * a given component is threadsafe.
 */
#define NS_ISTHREADSAFE_IID                                                   \
  { 0x88210890, 0x47a6, 0x11d2,                                               \
    {0xbe, 0xc3, 0x00, 0x80, 0x5f, 0x8a, 0x66, 0xdc} }


#if defined(NS_MT_SUPPORTED)

#define NS_LOCK_INSTANCE()                                                    \
  PR_CEnterMonitor((void*)this)
#define NS_UNLOCK_INSTANCE()                                                  \
  PR_CExitMonitor((void*)this)

#else

#define NS_LOCK_INSTANCE()
#define NS_UNLOCK_INSTANCE()

#endif

/**
 * This implements query interface with two assumptions: First, the
 * class in question implements nsISupports and its own interface and
 * nothing else. Second, the implementation of the class's primary
 * inheritance chain leads to its own interface.
 *
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */
#if defined(NS_DEBUG)
#define NS_VERIFY_THREADSAFE_INTERFACE(_iface)                                \
 if (NULL != (_iface)) {                                                      \
   nsISupports* tmp;                                                          \
   static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);               \
   NS_PRECONDITION((NS_OK == _iface->QueryInterface(kIsThreadsafeIID,         \
                                                    (void**)&tmp)),           \
                   "Interface is not threadsafe");                            \
 }
#else
#define NS_VERIFY_THREADSAFE_INTERFACE(_iface)
#endif


/*
 The following macro is deprecated.  We need to switch all instances
 to |NS_IMPL_QUERY_INTERFACE1|, or |NS_IMPL_QUERY_INTERFACE0| depending
 on how they were using it.
*/

#define NS_IMPL_QUERY_INTERFACE(_class,_classiiddef)                          \
NS_IMETHODIMP _class::QueryInterface(REFNSIID aIID, void** aInstancePtr)      \
{                                                                             \
  if (NULL == aInstancePtr) {                                                 \
    return NS_ERROR_NULL_POINTER;                                             \
  }                                                                           \
                                                                              \
  *aInstancePtr = NULL;                                                       \
                                                                              \
  static NS_DEFINE_IID(kClassIID, _classiiddef);                              \
  if (aIID.Equals(kClassIID)) {                                               \
    *aInstancePtr = (void*) this;                                             \
    NS_ADDREF_THIS();                                                         \
    return NS_OK;                                                             \
  }                                                                           \
  if (aIID.Equals(NS_GET_IID(nsISupports))) {                                 \
    *aInstancePtr = (void*) ((nsISupports*)this);                             \
    NS_ADDREF_THIS();                                                         \
    return NS_OK;                                                             \
  }                                                                           \
  return NS_NOINTERFACE;                                                      \
}

/**
 * Convenience macro for implementing all nsISupports methods for
 * a simple class.
 * @param _class The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */

#define NS_IMPL_ISUPPORTS(_class,_classiiddef)                                \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE(_class,_classiiddef)

#define NS_IMPL_ISUPPORTS0(_class)                                            \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE0(_class)

#define NS_IMPL_ISUPPORTS1(_class, _interface)                                \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE1(_class, _interface)

#define NS_IMPL_ISUPPORTS2(_class, _i1, _i2)                                  \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE2(_class, _i1, _i2)

#define NS_IMPL_ISUPPORTS3(_class, _i1, _i2, _i3)                             \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE3(_class, _i1, _i2, _i3)

#define NS_IMPL_ISUPPORTS4(_class, _i1, _i2, _i3, _i4)                        \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE4(_class, _i1, _i2, _i3, _i4)

#define NS_IMPL_ISUPPORTS5(_class, _i1, _i2, _i3, _i4, _i5)                   \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE5(_class, _i1, _i2, _i3, _i4, _i5)

#define NS_IMPL_ISUPPORTS6(_class, _i1, _i2, _i3, _i4, _i5, _i6)              \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE6(_class, _i1, _i2, _i3, _i4, _i5, _i6)

#define NS_IMPL_ISUPPORTS7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)         \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)

#define NS_IMPL_ISUPPORTS8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)    \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)

#define NS_IMPL_ISUPPORTS9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,    \
                           _i9)                                               \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8, _i9)

#define NS_IMPL_ISUPPORTS10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,   \
                            _i9, _i10)                                        \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8,   \
                            _i9, _i10)

////////////////////////////////////////////////////////////////////////////////


#define NS_IMPL_QUERY_INTERFACE_INHERITED(Class, Super, i1)                   \
  NS_IMPL_QUERY_INTERFACE_INHERITED1(Class, Super, i1)                        


// below should go into nsIMemoryUtils.h or something similar.

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
 *                  should be a PRInt32.  Note that this means this macro 
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
        PRInt32 iter_ = PRInt32(size);                                        \
        while (--iter_ >= 0)                                                  \
            freeFunc((array)[iter_]);                                         \
        nsMemory::Free((array));                                              \
    PR_END_MACRO

// convenience macros for commonly used calls.  mmmmm.  syntactic sugar.

/** 
 * Macro to free arrays of non-refcounted objects allocated by the
 * shared allocator (nsMemory) such as strings and wstrings.  A
 * convenience wrapper around NS_FREE_XPCOM_POINTER_ARRAY.
 *
 * @param size      Number of elements in the array.  If not a constant, this 
 *                  should be a PRInt32.  Note that this means this macro 
 *                  will not work if size >= 2^31.
 * @param array     The array to be freed.
 */
#define NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(size, array)                    \
    NS_FREE_XPCOM_POINTER_ARRAY((size), (array), nsMemory::Free)


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
 *                  should be a PRInt32.  Note that this means this macro 
 *                  will not work if size >= 2^31.
 * @param array     The array to be freed.
 */
#define NS_FREE_XPCOM_ISUPPORTS_POINTER_ARRAY(size, array)                    \
    NS_FREE_XPCOM_POINTER_ARRAY((size), (array), NS_IF_RELEASE)

#endif
