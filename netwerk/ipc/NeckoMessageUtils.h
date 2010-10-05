/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net> (Initial Developer)
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

#ifndef mozilla_net_NeckoMessageUtils_h
#define mozilla_net_NeckoMessageUtils_h

#include "IPC/IPCMessageUtils.h"
#include "nsStringGlue.h"
#include "nsIURI.h"
#include "nsIIPCSerializable.h"
#include "nsIClassInfo.h"
#include "nsComponentManagerUtils.h"
#include "nsNetUtil.h"

namespace IPC {

// Since IPDL doesn't have any knowledge of pointers, there's no easy way to
// pass around nsIURI pointers.  This is a very thin wrapper that IPDL can
// easily work with, allowing for conversion to and from an nsIURI pointer.

class URI {
 public:
  URI() : mURI(nsnull) {}
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
#ifdef DEBUG
    nsresult rv =
#endif
    classInfo->GetClassIDNoAlloc(&cid);
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
      aResult->mURI = nsnull;
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

}

#endif // mozilla_net_NeckoMessageUtils_h
