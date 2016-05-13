/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_PHttpChannelParams_h
#define mozilla_net_PHttpChannelParams_h

#define ALLOW_LATE_NSHTTP_H_INCLUDE 1
#include "base/basictypes.h"

#include "ipc/IPCMessageUtils.h"
#include "nsHttp.h"
#include "nsHttpHeaderArray.h"
#include "nsHttpResponseHead.h"

#include "nsIClassInfo.h"

namespace mozilla {
namespace net {

struct RequestHeaderTuple {
  nsCString mHeader;
  nsCString mValue;
  bool      mMerge;
  bool      mEmpty;

  bool operator ==(const RequestHeaderTuple &other) const {
    return mHeader.Equals(other.mHeader) &&
           mValue.Equals(other.mValue) &&
           mMerge == other.mMerge &&
           mEmpty == other.mEmpty;
  }
};

typedef nsTArray<RequestHeaderTuple> RequestHeaderTuples;

} // namespace net
} // namespace mozilla

namespace IPC {

template<>
struct ParamTraits<mozilla::net::RequestHeaderTuple>
{
  typedef mozilla::net::RequestHeaderTuple paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHeader);
    WriteParam(aMsg, aParam.mValue);
    WriteParam(aMsg, aParam.mMerge);
    WriteParam(aMsg, aParam.mEmpty);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->mHeader) ||
        !ReadParam(aMsg, aIter, &aResult->mValue)  ||
        !ReadParam(aMsg, aIter, &aResult->mMerge)  ||
        !ReadParam(aMsg, aIter, &aResult->mEmpty))
      return false;

    return true;
  }
};

template<>
struct ParamTraits<mozilla::net::nsHttpAtom>
{
  typedef mozilla::net::nsHttpAtom paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    // aParam.get() cannot be null.
    MOZ_ASSERT(aParam.get(), "null nsHTTPAtom value");
    nsAutoCString value(aParam.get());
    WriteParam(aMsg, value);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    nsAutoCString value;
    if (!ReadParam(aMsg, aIter, &value))
      return false;

    *aResult = mozilla::net::nsHttp::ResolveAtom(value.get());
    MOZ_ASSERT(aResult->get(), "atom table not initialized");
    return true;
  }
};

template<>
struct ParamTraits<mozilla::net::nsHttpHeaderArray::nsEntry>
{
  typedef mozilla::net::nsHttpHeaderArray::nsEntry paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.header);
    WriteParam(aMsg, aParam.value);
    switch (aParam.variety) {
      case mozilla::net::nsHttpHeaderArray::eVarietyUnknown:
        WriteParam(aMsg, (uint8_t)0);
        break;
      case mozilla::net::nsHttpHeaderArray::eVarietyRequestOverride:
        WriteParam(aMsg, (uint8_t)1);
        break;
      case mozilla::net::nsHttpHeaderArray::eVarietyRequestDefault:
        WriteParam(aMsg, (uint8_t)2);
        break;
      case mozilla::net::nsHttpHeaderArray::eVarietyResponseNetOriginalAndResponse:
        WriteParam(aMsg, (uint8_t)3);
        break;
      case mozilla::net::nsHttpHeaderArray::eVarietyResponseNetOriginal:
        WriteParam(aMsg, (uint8_t)4);
        break;
      case mozilla::net::nsHttpHeaderArray::eVarietyResponse:
        WriteParam(aMsg, (uint8_t)5);
    }
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    uint8_t variety;
    if (!ReadParam(aMsg, aIter, &aResult->header) ||
        !ReadParam(aMsg, aIter, &aResult->value)  ||
        !ReadParam(aMsg, aIter, &variety))
      return false;

    switch (variety) {
      case 0:
        aResult->variety = mozilla::net::nsHttpHeaderArray::eVarietyUnknown;
        break;
      case 1:
        aResult->variety = mozilla::net::nsHttpHeaderArray::eVarietyRequestOverride;
        break;
      case 2:
        aResult->variety = mozilla::net::nsHttpHeaderArray::eVarietyRequestDefault;
        break;
      case 3:
        aResult->variety = mozilla::net::nsHttpHeaderArray::eVarietyResponseNetOriginalAndResponse;
        break;
      case 4:
        aResult->variety = mozilla::net::nsHttpHeaderArray::eVarietyResponseNetOriginal;
        break;
      case 5:
        aResult->variety = mozilla::net::nsHttpHeaderArray::eVarietyResponse;
        break;
      default:
        return false;
    }

    return true;
  }
};


template<>
struct ParamTraits<mozilla::net::nsHttpHeaderArray>
{
  typedef mozilla::net::nsHttpHeaderArray paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    paramType& p = const_cast<paramType&>(aParam);

    WriteParam(aMsg, p.mHeaders);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->mHeaders))
      return false;

    return true;
  }
};

template<>
struct ParamTraits<mozilla::net::nsHttpResponseHead>
{
  typedef mozilla::net::nsHttpResponseHead paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHeaders);
    WriteParam(aMsg, aParam.mVersion);
    WriteParam(aMsg, aParam.mStatus);
    WriteParam(aMsg, aParam.mStatusText);
    WriteParam(aMsg, aParam.mContentLength);
    WriteParam(aMsg, aParam.mContentType);
    WriteParam(aMsg, aParam.mContentCharset);
    WriteParam(aMsg, aParam.mCacheControlPrivate);
    WriteParam(aMsg, aParam.mCacheControlNoStore);
    WriteParam(aMsg, aParam.mCacheControlNoCache);
    WriteParam(aMsg, aParam.mPragmaNoCache);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->mHeaders)             ||
        !ReadParam(aMsg, aIter, &aResult->mVersion)             ||
        !ReadParam(aMsg, aIter, &aResult->mStatus)              ||
        !ReadParam(aMsg, aIter, &aResult->mStatusText)          ||
        !ReadParam(aMsg, aIter, &aResult->mContentLength)       ||
        !ReadParam(aMsg, aIter, &aResult->mContentType)         ||
        !ReadParam(aMsg, aIter, &aResult->mContentCharset)      ||
        !ReadParam(aMsg, aIter, &aResult->mCacheControlPrivate) ||
        !ReadParam(aMsg, aIter, &aResult->mCacheControlNoStore) ||
        !ReadParam(aMsg, aIter, &aResult->mCacheControlNoCache) ||
        !ReadParam(aMsg, aIter, &aResult->mPragmaNoCache))
      return false;

    return true;
  }
};

} // namespace IPC

#endif // mozilla_net_PHttpChannelParams_h
