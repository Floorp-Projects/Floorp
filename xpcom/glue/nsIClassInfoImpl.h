/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIClassInfoImpl_h__
#define nsIClassInfoImpl_h__

#include "nsIClassInfo.h"
#include "nsISupportsImpl.h"

#include <new>

/**
 * This header file provides macros which help you make your class implement
 * nsIClassInfo.  Implementing nsIClassInfo is particularly helpful if you have
 * a C++ class which implements multiple interfaces and which you access from
 * JavaScript.  If that class implements nsIClassInfo, the JavaScript code
 * won't have to call QueryInterface on instances of the class; all methods
 * from all interfaces returned by GetInterfaces() will be available
 * automagically.
 *
 * Here's all you need to do.  Given a class
 *
 *   class nsFooBar : public nsIFoo, public nsIBar { };
 *
 * you should already have the following nsISupports implementation in its cpp
 * file:
 *
 *   NS_IMPL_ISUPPORTS2(nsFooBar, nsIFoo, nsIBar).
 *
 * Change this to
 *
 *   NS_IMPL_CLASSINFO(nsFooBar, NULL, 0, NS_FOOBAR_CID)
 *   NS_IMPL_ISUPPORTS2_CI(nsFooBar, nsIFoo, nsIBar)
 *
 * If nsFooBar is threadsafe, change the 0 above to nsIClassInfo::THREADSAFE.
 * If it's a singleton, use nsIClassInfo::SINGLETON.  The full list of flags is
 * in nsIClassInfo.idl.
 *
 * The NULL parameter is there so you can pass a function for converting from
 * an XPCOM object to a scriptable helper.  Unless you're doing specialized JS
 * work, you can probably leave this as NULL.
 *
 * This file also defines the NS_IMPL_QUERY_INTERFACE2_CI macro, which you can
 * use to replace NS_IMPL_QUERY_INTERFACE2, if you use that instead of
 * NS_IMPL_ISUPPORTS2.
 *
 * That's it!  The rest is gory details.
 *
 *
 * Notice that nsFooBar didn't need to inherit from nsIClassInfo in order to
 * "implement" it.  However, after adding these macros to nsFooBar, you you can
 * QueryInterface an instance of nsFooBar to nsIClassInfo.  How can this be?
 *
 * The answer lies in the NS_IMPL_ISUPPORTS2_CI macro.  It modifies nsFooBar's
 * QueryInterface implementation such that, if we ask to QI to nsIClassInfo, it
 * returns a singleton object associated with the class.  (That singleton is
 * defined by NS_IMPL_CLASSINFO.)  So all nsFooBar instances will return the
 * same object when QI'ed to nsIClassInfo.  (You can see this in
 * NS_IMPL_QUERY_CLASSINFO below.)
 *
 * This hack breaks XPCOM's rules, since if you take an instance of nsFooBar,
 * QI it to nsIClassInfo, and then try to QI to nsIFoo, that will fail.  On the
 * upside, implementing nsIClassInfo doesn't add a vtable pointer to instances
 * of your class.
 *
 * In principal, you can also implement nsIClassInfo by inheriting from the
 * interface.  But some code expects that when it QI's an object to
 * nsIClassInfo, it gets back a singleton which isn't attached to any
 * particular object.  If a class were to implement nsIClassInfo through
 * inheritance, that code might QI to nsIClassInfo and keep the resulting
 * object alive, thinking it was only keeping alive the classinfo singleton,
 * but in fact keeping a whole instance of the class alive.  See, e.g., bug
 * 658632.
 *
 * Unless you specifically need to have a different nsIClassInfo instance for
 * each instance of your class, you should probably just implement nsIClassInfo
 * as a singleton.
 */

class NS_COM_GLUE GenericClassInfo : public nsIClassInfo
{
public:
  struct ClassInfoData
  {
    typedef NS_CALLBACK(GetInterfacesProc)(uint32_t* countp,
                                           nsIID*** array);
    typedef NS_CALLBACK(GetLanguageHelperProc)(uint32_t language,
                                               nsISupports** helper);

    GetInterfacesProc getinterfaces;
    GetLanguageHelperProc getlanguagehelper;
    uint32_t flags;
    nsCID cid;
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICLASSINFO

  GenericClassInfo(const ClassInfoData* data)
    : mData(data)
  { }

private:
  const ClassInfoData* mData;
};

#define NS_CLASSINFO_NAME(_class) g##_class##_classInfoGlobal
#define NS_CI_INTERFACE_GETTER_NAME(_class) _class##_GetInterfacesHelper
#define NS_DECL_CI_INTERFACE_GETTER(_class)                                   \
  extern NS_IMETHODIMP NS_CI_INTERFACE_GETTER_NAME(_class)                    \
     (uint32_t *, nsIID ***);

#define NS_IMPL_CLASSINFO(_class, _getlanguagehelper, _flags, _cid)     \
  NS_DECL_CI_INTERFACE_GETTER(_class)                                   \
  static const GenericClassInfo::ClassInfoData k##_class##ClassInfoData = { \
    NS_CI_INTERFACE_GETTER_NAME(_class),                                \
    _getlanguagehelper,                                                 \
    _flags | nsIClassInfo::SINGLETON_CLASSINFO,                         \
    _cid,                                                               \
  };                                                                    \
  static char k##_class##ClassInfoDataPlace[sizeof(GenericClassInfo)];  \
  nsIClassInfo* NS_CLASSINFO_NAME(_class) = NULL;

#define NS_IMPL_QUERY_CLASSINFO(_class)                                       \
  if ( aIID.Equals(NS_GET_IID(nsIClassInfo)) ) {                              \
    if (!NS_CLASSINFO_NAME(_class))                                           \
      NS_CLASSINFO_NAME(_class) = new (k##_class##ClassInfoDataPlace)         \
        GenericClassInfo(&k##_class##ClassInfoData);                          \
    foundInterface = NS_CLASSINFO_NAME(_class);                               \
  } else

#define NS_CLASSINFO_HELPER_BEGIN(_class, _c)                                 \
NS_IMETHODIMP                                                                 \
NS_CI_INTERFACE_GETTER_NAME(_class)(uint32_t *count, nsIID ***array)          \
{                                                                             \
    *count = _c;                                                              \
    *array = (nsIID **)nsMemory::Alloc(sizeof (nsIID *) * _c);

#define NS_CLASSINFO_HELPER_ENTRY(_i, _interface)                             \
    (*array)[_i] = (nsIID *)nsMemory::Clone(&NS_GET_IID(_interface),          \
                                            sizeof(nsIID));

#define NS_CLASSINFO_HELPER_END                                               \
    return NS_OK;                                                             \
}

#define NS_IMPL_CI_INTERFACE_GETTER1(_class, _interface)                      \
   NS_CLASSINFO_HELPER_BEGIN(_class, 1)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _interface)                                 \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE1_CI(_class, _i1)                              \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS1_CI(_class, _interface)                             \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE1_CI(_class, _interface)                             \
  NS_IMPL_CI_INTERFACE_GETTER1(_class, _interface)

#define NS_IMPL_CI_INTERFACE_GETTER2(_class, _i1, _i2)                        \
   NS_CLASSINFO_HELPER_BEGIN(_class, 2)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE2_CI(_class, _i1, _i2)                         \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS2_CI(_class, _i1, _i2)                               \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE2_CI(_class, _i1, _i2)                               \
  NS_IMPL_CI_INTERFACE_GETTER2(_class, _i1, _i2)

#define NS_IMPL_CI_INTERFACE_GETTER3(_class, _i1, _i2, _i3)                   \
   NS_CLASSINFO_HELPER_BEGIN(_class, 3)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE3_CI(_class, _i1, _i2, _i3)                    \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS3_CI(_class, _i1, _i2, _i3)                          \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE3_CI(_class, _i1, _i2, _i3)                          \
  NS_IMPL_CI_INTERFACE_GETTER3(_class, _i1, _i2, _i3)

#define NS_IMPL_CI_INTERFACE_GETTER4(_class, _i1, _i2, _i3, _i4)              \
   NS_CLASSINFO_HELPER_BEGIN(_class, 4)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE4_CI(_class, _i1, _i2, _i3, _i4)               \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS4_CI(_class, _i1, _i2, _i3, _i4)                     \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE4_CI(_class, _i1, _i2, _i3, _i4)                     \
  NS_IMPL_CI_INTERFACE_GETTER4(_class, _i1, _i2, _i3, _i4)

#define NS_IMPL_CI_INTERFACE_GETTER5(_class, _i1, _i2, _i3, _i4, _i5)         \
   NS_CLASSINFO_HELPER_BEGIN(_class, 5)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE5_CI(_class, _i1, _i2, _i3, _i4, _i5)          \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS5_CI(_class, _i1, _i2, _i3, _i4, _i5)                \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE5_CI(_class, _i1, _i2, _i3, _i4, _i5)                \
  NS_IMPL_CI_INTERFACE_GETTER5(_class, _i1, _i2, _i3, _i4, _i5)

#define NS_IMPL_CI_INTERFACE_GETTER6(_class, _i1, _i2, _i3, _i4, _i5, _i6)    \
   NS_CLASSINFO_HELPER_BEGIN(_class, 6)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE6_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6)     \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS6_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6)           \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE6_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6)           \
  NS_IMPL_CI_INTERFACE_GETTER6(_class, _i1, _i2, _i3, _i4, _i5, _i6)

#define NS_IMPL_CI_INTERFACE_GETTER7(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7)                                     \
   NS_CLASSINFO_HELPER_BEGIN(_class, 7)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE7_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,     \
                                    _i7)                                      \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS7_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)      \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE7_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)      \
  NS_IMPL_CI_INTERFACE_GETTER7(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7)

#define NS_IMPL_CI_INTERFACE_GETTER8(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7, _i8)                                \
   NS_CLASSINFO_HELPER_BEGIN(_class, 8)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
     NS_CLASSINFO_HELPER_ENTRY(7, _i8)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE8_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,     \
                                    _i7, _i8)                                 \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS8_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8) \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE8_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8) \
  NS_IMPL_CI_INTERFACE_GETTER8(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7, _i8)

#define NS_IMPL_CI_INTERFACE_GETTER9(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7, _i8, _i9)                           \
   NS_CLASSINFO_HELPER_BEGIN(_class, 9)                                       \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
     NS_CLASSINFO_HELPER_ENTRY(7, _i8)                                        \
     NS_CLASSINFO_HELPER_ENTRY(8, _i9)                                        \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE9_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,     \
                                    _i7, _i8, _i9)                            \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                               \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS9_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,      \
                              _i8, _i9)                                       \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE9_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,      \
                              _i8, _i9)                                       \
  NS_IMPL_CI_INTERFACE_GETTER9(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9)

#define NS_IMPL_CI_INTERFACE_GETTER10(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7, _i8, _i9, _i10)                    \
   NS_CLASSINFO_HELPER_BEGIN(_class, 10)                                      \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
     NS_CLASSINFO_HELPER_ENTRY(7, _i8)                                        \
     NS_CLASSINFO_HELPER_ENTRY(8, _i9)                                        \
     NS_CLASSINFO_HELPER_ENTRY(9, _i10)                                       \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_CI_INTERFACE_GETTER11(_class, _i1, _i2, _i3, _i4, _i5, _i6,   \
                                      _i7, _i8, _i9, _i10, _i11)              \
   NS_CLASSINFO_HELPER_BEGIN(_class, 11)                                      \
     NS_CLASSINFO_HELPER_ENTRY(0, _i1)                                        \
     NS_CLASSINFO_HELPER_ENTRY(1, _i2)                                        \
     NS_CLASSINFO_HELPER_ENTRY(2, _i3)                                        \
     NS_CLASSINFO_HELPER_ENTRY(3, _i4)                                        \
     NS_CLASSINFO_HELPER_ENTRY(4, _i5)                                        \
     NS_CLASSINFO_HELPER_ENTRY(5, _i6)                                        \
     NS_CLASSINFO_HELPER_ENTRY(6, _i7)                                        \
     NS_CLASSINFO_HELPER_ENTRY(7, _i8)                                        \
     NS_CLASSINFO_HELPER_ENTRY(8, _i9)                                        \
     NS_CLASSINFO_HELPER_ENTRY(9, _i10)                                       \
     NS_CLASSINFO_HELPER_ENTRY(10, _i11)                                      \
   NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE10_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7, _i8, _i9, _i10)                     \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                               \
    NS_INTERFACE_MAP_ENTRY(_i10)                                              \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_QUERY_INTERFACE11_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6,    \
                                     _i7, _i8, _i9, _i10, _i11)               \
  NS_INTERFACE_MAP_BEGIN(_class)                                              \
    NS_INTERFACE_MAP_ENTRY(_i1)                                               \
    NS_INTERFACE_MAP_ENTRY(_i2)                                               \
    NS_INTERFACE_MAP_ENTRY(_i3)                                               \
    NS_INTERFACE_MAP_ENTRY(_i4)                                               \
    NS_INTERFACE_MAP_ENTRY(_i5)                                               \
    NS_INTERFACE_MAP_ENTRY(_i6)                                               \
    NS_INTERFACE_MAP_ENTRY(_i7)                                               \
    NS_INTERFACE_MAP_ENTRY(_i8)                                               \
    NS_INTERFACE_MAP_ENTRY(_i9)                                               \
    NS_INTERFACE_MAP_ENTRY(_i10)                                              \
    NS_INTERFACE_MAP_ENTRY(_i11)                                              \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, _i1)                        \
    NS_IMPL_QUERY_CLASSINFO(_class)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS10_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9, _i10)                                \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE10_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9, _i10)                                \
  NS_IMPL_CI_INTERFACE_GETTER10(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,    \
                                _i8, _i9, _i10)

#define NS_IMPL_ISUPPORTS11_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9, _i10, _i11)                          \
  NS_IMPL_ADDREF(_class)                                                      \
  NS_IMPL_RELEASE(_class)                                                     \
  NS_IMPL_QUERY_INTERFACE11_CI(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,     \
                               _i8, _i9, _i10, _i11)                          \
  NS_IMPL_CI_INTERFACE_GETTER11(_class, _i1, _i2, _i3, _i4, _i5, _i6, _i7,    \
                                _i8, _i9, _i10, _i11)

#endif // nsIClassInfoImpl_h__
