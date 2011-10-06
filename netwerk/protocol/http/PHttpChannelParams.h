/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jae-Seong Lee-Russo <lusian@gmail.com>
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

#ifndef mozilla_net_PHttpChannelParams_h
#define mozilla_net_PHttpChannelParams_h

#define ALLOW_LATE_NSHTTP_H_INCLUDE 1
#include "base/basictypes.h"

#include "IPC/IPCMessageUtils.h"
#include "nsHttp.h"
#include "nsHttpHeaderArray.h"
#include "nsHttpResponseHead.h"

#include "nsIIPCSerializable.h"
#include "nsIClassInfo.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

struct RequestHeaderTuple {
  nsCString mHeader;
  nsCString mValue;
  bool      mMerge;
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
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->mHeader) ||
        !ReadParam(aMsg, aIter, &aResult->mValue)  ||
        !ReadParam(aMsg, aIter, &aResult->mMerge))
      return false;

    return true;
  }
};

template<>
struct ParamTraits<nsHttpAtom>
{
  typedef nsHttpAtom paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    // aParam.get() cannot be null.
    NS_ASSERTION(aParam.get(), "null nsHTTPAtom value");
    nsCAutoString value(aParam.get());
    WriteParam(aMsg, value);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    nsCAutoString value;
    if (!ReadParam(aMsg, aIter, &value))
      return false;

    *aResult = nsHttp::ResolveAtom(value.get());
    NS_ASSERTION(aResult->get(), "atom table not initialized");
    return true;
  }
};

template<>
struct ParamTraits<nsHttpHeaderArray::nsEntry>
{
  typedef nsHttpHeaderArray::nsEntry paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.header);
    WriteParam(aMsg, aParam.value);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->header) ||
        !ReadParam(aMsg, aIter, &aResult->value))
      return false;

    return true;
  }
};

template<>
struct ParamTraits<nsHttpHeaderArray>
{
  typedef nsHttpHeaderArray paramType;

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
struct ParamTraits<nsHttpResponseHead>
{
  typedef nsHttpResponseHead paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mHeaders);
    WriteParam(aMsg, aParam.mVersion);
    WriteParam(aMsg, aParam.mStatus);
    WriteParam(aMsg, aParam.mStatusText);
    WriteParam(aMsg, aParam.mContentLength);
    WriteParam(aMsg, aParam.mContentType);
    WriteParam(aMsg, aParam.mContentCharset);
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
        !ReadParam(aMsg, aIter, &aResult->mCacheControlNoStore) ||
        !ReadParam(aMsg, aIter, &aResult->mCacheControlNoCache) ||
        !ReadParam(aMsg, aIter, &aResult->mPragmaNoCache))
      return false;

    return true;
  }
};

} // namespace IPC

#endif // mozilla_net_PHttpChannelParams_h
