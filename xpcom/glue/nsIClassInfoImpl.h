/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIClassInfoImpl_h__
#define nsIClassInfoImpl_h__

#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"
#include "mozilla/MacroArgs.h"
#include "mozilla/MacroForEach.h"
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
 *   NS_IMPL_ISUPPORTS(nsFooBar, nsIFoo, nsIBar).
 *
 * Change this to
 *
 *   NS_IMPL_CLASSINFO(nsFooBar, nullptr, 0, NS_FOOBAR_CID)
 *   NS_IMPL_ISUPPORTS_CI(nsFooBar, nsIFoo, nsIBar)
 *
 * If nsFooBar is threadsafe, change the 0 above to nsIClassInfo::THREADSAFE.
 * If it's a singleton, use nsIClassInfo::SINGLETON.  The full list of flags is
 * in nsIClassInfo.idl.
 *
 * The nullptr parameter is there so you can pass a function for converting
 * from an XPCOM object to a scriptable helper.  Unless you're doing
 * specialized JS work, you can probably leave this as nullptr.
 *
 * This file also defines the NS_IMPL_QUERY_INTERFACE_CI macro, which you can
 * use to replace NS_IMPL_QUERY_INTERFACE, if you use that instead of
 * NS_IMPL_ISUPPORTS.
 *
 * That's it!  The rest is gory details.
 *
 *
 * Notice that nsFooBar didn't need to inherit from nsIClassInfo in order to
 * "implement" it.  However, after adding these macros to nsFooBar, you you can
 * QueryInterface an instance of nsFooBar to nsIClassInfo.  How can this be?
 *
 * The answer lies in the NS_IMPL_ISUPPORTS_CI macro.  It modifies nsFooBar's
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
    typedef NS_CALLBACK(GetInterfacesProc)(uint32_t* aCountP,
                                           nsIID*** aArray);
    typedef NS_CALLBACK(GetLanguageHelperProc)(uint32_t aLanguage,
                                               nsISupports** aHelper);

    GetInterfacesProc getinterfaces;
    GetLanguageHelperProc getlanguagehelper;
    uint32_t flags;
    nsCID cid;
  };

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSICLASSINFO

  explicit GenericClassInfo(const ClassInfoData* aData) : mData(aData) {}

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
  mozilla::AlignedStorage2<GenericClassInfo> k##_class##ClassInfoDataPlace;   \
  nsIClassInfo* NS_CLASSINFO_NAME(_class) = nullptr;

#define NS_IMPL_QUERY_CLASSINFO(_class)                                       \
  if ( aIID.Equals(NS_GET_IID(nsIClassInfo)) ) {                              \
    if (!NS_CLASSINFO_NAME(_class))                                           \
      NS_CLASSINFO_NAME(_class) = new (k##_class##ClassInfoDataPlace.addr())  \
        GenericClassInfo(&k##_class##ClassInfoData);                          \
    foundInterface = NS_CLASSINFO_NAME(_class);                               \
  } else

#define NS_CLASSINFO_HELPER_BEGIN(_class, _c)                                 \
NS_IMETHODIMP                                                                 \
NS_CI_INTERFACE_GETTER_NAME(_class)(uint32_t *count, nsIID ***array)          \
{                                                                             \
    *count = _c;                                                              \
    *array = (nsIID **)nsMemory::Alloc(sizeof (nsIID *) * _c);                \
    uint32_t i = 0;

#define NS_CLASSINFO_HELPER_ENTRY(_interface)                                 \
    (*array)[i++] = (nsIID*)nsMemory::Clone(&NS_GET_IID(_interface),          \
                                            sizeof(nsIID));

#define NS_CLASSINFO_HELPER_END                                               \
    MOZ_ASSERT(i == *count, "Incorrent number of entries");                   \
    return NS_OK;                                                             \
}

#define NS_IMPL_CI_INTERFACE_GETTER(aClass, ...)                              \
  MOZ_STATIC_ASSERT_VALID_ARG_COUNT(__VA_ARGS__);                             \
  NS_CLASSINFO_HELPER_BEGIN(aClass,                                           \
                            MOZ_PASTE_PREFIX_AND_ARG_COUNT(/* No prefix */,   \
                                                           __VA_ARGS__))      \
    MOZ_FOR_EACH(NS_CLASSINFO_HELPER_ENTRY, (), (__VA_ARGS__))                \
  NS_CLASSINFO_HELPER_END

#define NS_IMPL_QUERY_INTERFACE_CI(aClass, ...)                               \
  MOZ_STATIC_ASSERT_VALID_ARG_COUNT(__VA_ARGS__);                             \
  NS_INTERFACE_MAP_BEGIN(aClass)                                              \
    MOZ_FOR_EACH(NS_INTERFACE_MAP_ENTRY, (), (__VA_ARGS__))                   \
    NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, MOZ_ARG_1(__VA_ARGS__))     \
    NS_IMPL_QUERY_CLASSINFO(aClass)                                           \
  NS_INTERFACE_MAP_END

#define NS_IMPL_ISUPPORTS_CI(aClass, ...)                                     \
  NS_IMPL_ADDREF(aClass)                                                      \
  NS_IMPL_RELEASE(aClass)                                                     \
  NS_IMPL_QUERY_INTERFACE_CI(aClass, __VA_ARGS__)                             \
  NS_IMPL_CI_INTERFACE_GETTER(aClass, __VA_ARGS__)

#endif // nsIClassInfoImpl_h__
