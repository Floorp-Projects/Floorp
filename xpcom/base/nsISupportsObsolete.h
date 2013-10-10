/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef nsISupportsObsolete_h__
#define nsISupportsObsolete_h__

#include "prcmon.h"

///////////////////////////////////////////////////////////////////////////////


#define NS_INIT_REFCNT() NS_INIT_ISUPPORTS()

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
#define NS_METHOD_GETTER_STR(_method,_member)   \
_method(char* *aString)                         \
{                                               \
    if (!aString) return NS_ERROR_NULL_POINTER; \
    if (!(*aString = PL_strdup(_member)))       \
      return NS_ERROR_OUT_OF_MEMORY;            \
    return NS_OK;                               \
}

#define NS_METHOD_SETTER_STR(_method, _member) \
_method(const char *aString)                   \
{                                              \
    if (_member) PR_Free(_member);             \
    if (!aString)                              \
      _member = nullptr;                        \
    else if (!(_member = PL_strdup(aString)))  \
      return NS_ERROR_OUT_OF_MEMORY;           \
    return NS_OK;                              \
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

#define NS_LOCK_INSTANCE()                                                    \
  PR_CEnterMonitor((void*)this)
#define NS_UNLOCK_INSTANCE()                                                  \
  PR_CExitMonitor((void*)this)

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
#if defined(DEBUG)
#define NS_VERIFY_THREADSAFE_INTERFACE(_iface)                                \
 if (nullptr != (_iface)) {                                                   \
   nsISupports* tmp;                                                          \
   static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);               \
   NS_PRECONDITION((NS_OK == _iface->QueryInterface(kIsThreadsafeIID,         \
                                                    (void**)&tmp)),           \
                   "Interface is not threadsafe");                            \
 }
#else
#define NS_VERIFY_THREADSAFE_INTERFACE(_iface)
#endif

////////////////////////////////////////////////////////////////////////////////



#endif
