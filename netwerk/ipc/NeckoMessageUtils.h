/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_NeckoMessageUtils_h
#define mozilla_net_NeckoMessageUtils_h

#include "IPC/IPCMessageUtils.h"
#include "nsStringGlue.h"
#include "nsIURI.h"
#include "nsIIPCSerializable.h"
#include "nsIClassInfo.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"
#include "nsStringStream.h"
#include "prio.h"
#include "mozilla/Util.h" // for DebugOnly
#include "SerializedLoadContext.h"

namespace IPC {

// Since IPDL doesn't have any knowledge of pointers, there's no easy way to
// pass around nsIURI pointers.  This is a very thin wrapper that IPDL can
// easily work with, allowing for conversion to and from an nsIURI pointer.

class URI {
 public:
  URI() : mURI(nullptr) {}
  URI(nsIURI* aURI) : mURI(aURI) {}
  operator nsIURI*() const { return mURI.get(); }

  friend struct ParamTraits<URI>;
  
 private:
  // Unimplemented
  URI& operator=(URI&);

  nsCOMPtr<nsIURI> mURI;
};
  
template<>
struct ParamTraits<URI>
{
  typedef URI paramType;
  
  static void Write(Message* aMsg, const paramType& aParam)
  {
    bool isNull = !aParam.mURI;
    WriteParam(aMsg, isNull);
    if (isNull)
      return;
    
    nsCOMPtr<nsIIPCSerializable> serializable = do_QueryInterface(aParam.mURI);
    if (!serializable) {
      nsCString scheme;
      aParam.mURI->GetScheme(scheme);
      if (!scheme.EqualsASCII("about:") &&
          !scheme.EqualsASCII("javascript:") &&
          !scheme.EqualsASCII("javascript"))
        NS_WARNING("All IPDL URIs must be serializable or an allowed scheme");
    }
    
    bool isSerialized = !!serializable;
    WriteParam(aMsg, isSerialized);
    if (!isSerialized) {
      nsCString spec, charset;
      aParam.mURI->GetSpec(spec);
      aParam.mURI->GetOriginCharset(charset);
      WriteParam(aMsg, spec);
      WriteParam(aMsg, charset);
      return;
    }
    
    nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(aParam.mURI);
    char cidStr[NSID_LENGTH];
    nsCID cid;
    mozilla::DebugOnly<nsresult> rv = classInfo->GetClassIDNoAlloc(&cid);
    NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "All IPDL URIs must report a valid class ID");
    
    cid.ToProvidedString(cidStr);
    WriteParam(aMsg, nsCAutoString(cidStr));
    serializable->Write(aMsg);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull))
      return false;
    if (isNull) {
      aResult->mURI = nullptr;
      return true;
    }

    bool isSerialized;
    if (!ReadParam(aMsg, aIter, &isSerialized))
      return false;
    if (!isSerialized) {
      nsCString spec, charset;
      if (!ReadParam(aMsg, aIter, &spec) ||
          !ReadParam(aMsg, aIter, &charset))
        return false;
      
      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), spec, charset.get());
      if (NS_FAILED(rv))
        return false;
      
      uri.swap(aResult->mURI);
      return true;
    }
    
    nsCAutoString cidStr;
    nsCID cid;
    if (!ReadParam(aMsg, aIter, &cidStr) ||
        !cid.Parse(cidStr.get()))
      return false;

    nsCOMPtr<nsIURI> uri = do_CreateInstance(cid);
    if (!uri)
      return false;
    nsCOMPtr<nsIIPCSerializable> serializable = do_QueryInterface(uri);
    if (!serializable || !serializable->Read(aMsg, aIter))
      return false;

    uri.swap(aResult->mURI);
    return true;
  }

  static void Log(const paramType& aParam, std::wstring* aLog)
  {
    nsIURI* uri = aParam.mURI;
    if (uri) {
      nsCString spec;
      uri->GetSpec(spec);
      aLog->append(StringPrintf(L"[%s]", spec.get()));
    }
    else {
      aLog->append(L"[]");
    }
  }
};

class InputStream {
 public:
  InputStream() : mStream(nullptr) {}
  InputStream(nsIInputStream* aStream) : mStream(aStream) {}
  operator nsIInputStream*() const { return mStream.get(); }

  friend struct ParamTraits<InputStream>;

 private:
  // Unimplemented
  InputStream& operator=(InputStream&);

  nsCOMPtr<nsIInputStream> mStream;
};

template<>
struct ParamTraits<InputStream>
{
  typedef InputStream paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    bool isNull = !aParam.mStream;
    aMsg->WriteBool(isNull);

    if (isNull)
      return;

    nsCOMPtr<nsIIPCSerializable> serializable = do_QueryInterface(aParam.mStream);
    bool isSerializable = !!serializable;
    WriteParam(aMsg, isSerializable);

    if (!serializable) {
      NS_WARNING("nsIInputStream implementation doesn't support nsIIPCSerializable; falling back to copying data");

      nsCString streamString;
      PRUint32 bytes;

      aParam.mStream->Available(&bytes);
      if (bytes > 0) {
        mozilla::DebugOnly<nsresult> rv =
          NS_ReadInputStreamToString(aParam.mStream, streamString, bytes);
        NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "Can't read input stream into a string!");
      }

      WriteParam(aMsg, streamString);
      return;
    }

    nsCOMPtr<nsIClassInfo> classInfo = do_QueryInterface(aParam.mStream);
    NS_ASSERTION(classInfo, "Must QI to nsIClassInfo for this to work!");

    char cidStr[NSID_LENGTH];
    nsCID cid;
    mozilla::DebugOnly<nsresult> rv = classInfo->GetClassIDNoAlloc(&cid);
    NS_ABORT_IF_FALSE(NS_SUCCEEDED(rv), "All IPDL streams must report a valid class ID");

    cid.ToProvidedString(cidStr);
    WriteParam(aMsg, nsCAutoString(cidStr));
    serializable->Write(aMsg);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull))
      return false;

    if (isNull) {
      aResult->mStream = nullptr;
      return true;
    }

    bool isSerializable;
    if (!ReadParam(aMsg, aIter, &isSerializable))
      return false;

    nsCOMPtr<nsIInputStream> stream;
    if (!isSerializable) {
      nsCString streamString;
      if (!ReadParam(aMsg, aIter, &streamString))
        return false;

      nsresult rv = NS_NewCStringInputStream(getter_AddRefs(stream), streamString);
      if (NS_FAILED(rv))
        return false;
    } else {
      nsCAutoString cidStr;
      nsCID cid;
      if (!ReadParam(aMsg, aIter, &cidStr) ||
          !cid.Parse(cidStr.get()))
        return false;

      stream = do_CreateInstance(cid);
      if (!stream)
        return false;
      nsCOMPtr<nsIIPCSerializable> serializable = do_QueryInterface(stream);
      if (!serializable || !serializable->Read(aMsg, aIter))
        return false;
    }

    stream.swap(aResult->mStream);
    return true;
  }
};

// nsIPermissionManager utilities

struct Permission
{
  nsCString host, type;
  PRUint32 capability, expireType;
  PRInt64 expireTime;

  Permission() { }
  Permission(const nsCString& aHost,
             const nsCString& aType,
             const PRUint32 aCapability,
             const PRUint32 aExpireType,
             const PRInt64 aExpireTime) : host(aHost),
                                          type(aType),
                                          capability(aCapability),
                                          expireType(aExpireType),
                                          expireTime(aExpireTime) { }
};

template<>
struct ParamTraits<Permission>
{
  static void Write(Message* aMsg, const Permission& aParam)
  {
    WriteParam(aMsg, aParam.host);
    WriteParam(aMsg, aParam.type);
    WriteParam(aMsg, aParam.capability);
    WriteParam(aMsg, aParam.expireType);
    WriteParam(aMsg, aParam.expireTime);
  }

  static bool Read(const Message* aMsg, void** aIter, Permission* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->host) &&
           ReadParam(aMsg, aIter, &aResult->type) &&
           ReadParam(aMsg, aIter, &aResult->capability) &&
           ReadParam(aMsg, aIter, &aResult->expireType) &&
           ReadParam(aMsg, aIter, &aResult->expireTime);
  }

  static void Log(const Permission& aParam, std::wstring* aLog)
  {
    aLog->append(StringPrintf(L"[%s]", aParam.host.get()));
  }
};

template<>
struct ParamTraits<PRNetAddr>
{
  static void Write(Message* aMsg, const PRNetAddr &aParam)
  {
    WriteParam(aMsg, aParam.raw.family);
    if (aParam.raw.family == PR_AF_UNSPEC) {
      aMsg->WriteBytes(aParam.raw.data, sizeof(aParam.raw.data));
    } else if (aParam.raw.family == PR_AF_INET) {
      WriteParam(aMsg, aParam.inet.port);
      WriteParam(aMsg, aParam.inet.ip);
    } else if (aParam.raw.family == PR_AF_INET6) {
      WriteParam(aMsg, aParam.ipv6.port);
      WriteParam(aMsg, aParam.ipv6.flowinfo);
      WriteParam(aMsg, aParam.ipv6.ip.pr_s6_addr64[0]);
      WriteParam(aMsg, aParam.ipv6.ip.pr_s6_addr64[1]);
      WriteParam(aMsg, aParam.ipv6.scope_id);
#if defined(XP_UNIX) || defined(XP_OS2)
    } else if (aParam.raw.family == PR_AF_LOCAL) {
      // Train's already off the rails:  let's get a stack trace at least...
      NS_RUNTIMEABORT("Error: please post stack trace to "
                      "https://bugzilla.mozilla.org/show_bug.cgi?id=661158");
      aMsg->WriteBytes(aParam.local.path, sizeof(aParam.local.path));
#endif
    }

    /* If we get here without hitting any of the cases above, there's not much
     * we can do but let the deserializer fail when it gets this message */
  }

  static bool Read(const Message* aMsg, void** aIter, PRNetAddr* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->raw.family))
      return false;

    if (aResult->raw.family == PR_AF_UNSPEC) {
      return aMsg->ReadBytes(aIter,
                             reinterpret_cast<const char**>(&aResult->raw.data),
                             sizeof(aResult->raw.data));
    } else if (aResult->raw.family == PR_AF_INET) {
      return ReadParam(aMsg, aIter, &aResult->inet.port) &&
             ReadParam(aMsg, aIter, &aResult->inet.ip);
    } else if (aResult->raw.family == PR_AF_INET6) {
      return ReadParam(aMsg, aIter, &aResult->ipv6.port) &&
             ReadParam(aMsg, aIter, &aResult->ipv6.flowinfo) &&
             ReadParam(aMsg, aIter, &aResult->ipv6.ip.pr_s6_addr64[0]) &&
             ReadParam(aMsg, aIter, &aResult->ipv6.ip.pr_s6_addr64[1]) &&
             ReadParam(aMsg, aIter, &aResult->ipv6.scope_id);
#if defined(XP_UNIX) || defined(XP_OS2)
    } else if (aResult->raw.family == PR_AF_LOCAL) {
      return aMsg->ReadBytes(aIter,
                             reinterpret_cast<const char**>(&aResult->local.path),
                             sizeof(aResult->local.path));
#endif
    }

    /* We've been tricked by some socket family we don't know about! */
    return false;
  }
};

}

#endif // mozilla_net_NeckoMessageUtils_h
