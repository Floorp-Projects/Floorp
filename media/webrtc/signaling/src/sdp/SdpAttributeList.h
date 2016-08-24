/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDPATTRIBUTELIST_H_
#define _SDPATTRIBUTELIST_H_

#include "mozilla/UniquePtr.h"
#include "mozilla/Attributes.h"

#include "signaling/src/sdp/SdpAttribute.h"

namespace mozilla
{

class SdpAttributeList
{
public:
  virtual ~SdpAttributeList() {}
  typedef SdpAttribute::AttributeType AttributeType;

  // Avoid default params on virtual functions
  bool
  HasAttribute(AttributeType type) const
  {
    return HasAttribute(type, true);
  }

  const SdpAttribute*
  GetAttribute(AttributeType type) const
  {
    return GetAttribute(type, true);
  }

  virtual bool HasAttribute(AttributeType type, bool sessionFallback) const = 0;
  virtual const SdpAttribute* GetAttribute(AttributeType type,
                                           bool sessionFallback) const = 0;
  // The setter takes an attribute of any type, and takes ownership
  virtual void SetAttribute(SdpAttribute* attr) = 0;
  virtual void RemoveAttribute(AttributeType type) = 0;
  virtual void Clear() = 0;

  virtual const SdpConnectionAttribute& GetConnection() const = 0;
  virtual const SdpOptionsAttribute& GetIceOptions() const = 0;
  virtual const SdpRtcpAttribute& GetRtcp() const = 0;
  virtual const SdpRemoteCandidatesAttribute& GetRemoteCandidates() const = 0;
  virtual const SdpSetupAttribute& GetSetup() const = 0;
  virtual const SdpDtlsMessageAttribute& GetDtlsMessage() const = 0;

  // These attributes can appear multiple times, so the returned
  // classes actually represent a collection of values.
  virtual const std::vector<std::string>& GetCandidate() const = 0;
  virtual const SdpExtmapAttributeList& GetExtmap() const = 0;
  virtual const SdpFingerprintAttributeList& GetFingerprint() const = 0;
  virtual const SdpFmtpAttributeList& GetFmtp() const = 0;
  virtual const SdpGroupAttributeList& GetGroup() const = 0;
  virtual const SdpImageattrAttributeList& GetImageattr() const = 0;
  virtual const SdpSimulcastAttribute& GetSimulcast() const = 0;
  virtual const SdpMsidAttributeList& GetMsid() const = 0;
  virtual const SdpMsidSemanticAttributeList& GetMsidSemantic() const = 0;
  virtual const SdpRidAttributeList& GetRid() const = 0;
  virtual const SdpRtcpFbAttributeList& GetRtcpFb() const = 0;
  virtual const SdpRtpmapAttributeList& GetRtpmap() const = 0;
  virtual const SdpSctpmapAttributeList& GetSctpmap() const = 0;
  virtual const SdpSsrcAttributeList& GetSsrc() const = 0;
  virtual const SdpSsrcGroupAttributeList& GetSsrcGroup() const = 0;

  // These attributes are effectively simple types, so we'll make life
  // easy by just returning their value.
  virtual const std::string& GetIcePwd() const = 0;
  virtual const std::string& GetIceUfrag() const = 0;
  virtual const std::string& GetIdentity() const = 0;
  virtual const std::string& GetLabel() const = 0;
  virtual unsigned int GetMaxptime() const = 0;
  virtual const std::string& GetMid() const = 0;
  virtual unsigned int GetPtime() const = 0;

  // This is "special", because it's multiple things
  virtual SdpDirectionAttribute::Direction GetDirection() const = 0;

  virtual void Serialize(std::ostream&) const = 0;
};

inline std::ostream& operator<<(std::ostream& os, const SdpAttributeList& al)
{
  al.Serialize(os);
  return os;
}

} // namespace mozilla

#endif
