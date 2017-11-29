/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _JSEPTRANSPORT_H_
#define _JSEPTRANSPORT_H_

#include <string>
#include <vector>

#include <mozilla/RefPtr.h>
#include <mozilla/UniquePtr.h>
#include "nsISupportsImpl.h"

#include "signaling/src/sdp/SdpAttribute.h"

namespace mozilla {

class JsepDtlsTransport
{
public:
  JsepDtlsTransport() : mRole(kJsepDtlsInvalidRole) {}

  virtual ~JsepDtlsTransport() {}

  enum Role {
    kJsepDtlsClient,
    kJsepDtlsServer,
    kJsepDtlsInvalidRole
  };

  virtual const SdpFingerprintAttributeList&
  GetFingerprints() const
  {
    return mFingerprints;
  }

  virtual Role
  GetRole() const
  {
    return mRole;
  }

private:
  friend class JsepSessionImpl;

  SdpFingerprintAttributeList mFingerprints;
  Role mRole;
};

class JsepIceTransport
{
public:
  JsepIceTransport() {}

  virtual ~JsepIceTransport() {}

  const std::string&
  GetUfrag() const
  {
    return mUfrag;
  }
  const std::string&
  GetPassword() const
  {
    return mPwd;
  }
  const std::vector<std::string>&
  GetCandidates() const
  {
    return mCandidates;
  }

private:
  friend class JsepSessionImpl;

  std::string mUfrag;
  std::string mPwd;
  std::vector<std::string> mCandidates;
};

class JsepTransport
{
public:
  JsepTransport()
      : mComponents(0)
  {
  }

  JsepTransport(const JsepTransport& orig)
  {
    *this = orig;
  }

  JsepTransport& operator=(const JsepTransport& orig)
  {
    if (this != &orig) {
      mIce.reset(orig.mIce ? new JsepIceTransport(*orig.mIce) : nullptr);
      mDtls.reset(orig.mDtls ? new JsepDtlsTransport(*orig.mDtls) : nullptr);
      mTransportId = orig.mTransportId;
      mComponents = orig.mComponents;
    }
    return *this;
  }

  void Close()
  {
    mComponents = 0;
    mTransportId.clear();
    mIce.reset();
    mDtls.reset();
  }

  // Unique identifier for this transport within this call. Group?
  std::string mTransportId;

  // ICE stuff.
  UniquePtr<JsepIceTransport> mIce;
  UniquePtr<JsepDtlsTransport> mDtls;

  // Number of required components.
  size_t mComponents;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(JsepTransport);

protected:
  ~JsepTransport() {}
};

} // namespace mozilla

#endif
