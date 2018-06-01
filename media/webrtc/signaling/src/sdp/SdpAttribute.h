/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDPATTRIBUTE_H_
#define _SDPATTRIBUTE_H_

#include <algorithm>
#include <cctype>
#include <vector>
#include <ostream>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <string>

#include "mozilla/UniquePtr.h"
#include "mozilla/Attributes.h"
#include "mozilla/Assertions.h"
#include "mozilla/Maybe.h"

#include "signaling/src/sdp/SdpEnum.h"
#include "signaling/src/common/EncodingConstraints.h"

namespace mozilla
{

/**
 * Base class for SDP attributes
*/
class SdpAttribute
{
public:
  enum AttributeType {
    kFirstAttribute = 0,
    kBundleOnlyAttribute = 0,
    kCandidateAttribute,
    kConnectionAttribute,
    kDirectionAttribute,
    kDtlsMessageAttribute,
    kEndOfCandidatesAttribute,
    kExtmapAttribute,
    kFingerprintAttribute,
    kFmtpAttribute,
    kGroupAttribute,
    kIceLiteAttribute,
    kIceMismatchAttribute,
    kIceOptionsAttribute,
    kIcePwdAttribute,
    kIceUfragAttribute,
    kIdentityAttribute,
    kImageattrAttribute,
    kLabelAttribute,
    kMaxptimeAttribute,
    kMidAttribute,
    kMsidAttribute,
    kMsidSemanticAttribute,
    kPtimeAttribute,
    kRemoteCandidatesAttribute,
    kRidAttribute,
    kRtcpAttribute,
    kRtcpFbAttribute,
    kRtcpMuxAttribute,
    kRtcpRsizeAttribute,
    kRtpmapAttribute,
    kSctpmapAttribute,
    kSetupAttribute,
    kSimulcastAttribute,
    kSsrcAttribute,
    kSsrcGroupAttribute,
    kSctpPortAttribute,
    kMaxMessageSizeAttribute,
    kLastAttribute = kMaxMessageSizeAttribute
  };

  explicit SdpAttribute(AttributeType type) : mType(type) {}
  virtual ~SdpAttribute() {}

  AttributeType
  GetType() const
  {
    return mType;
  }

  virtual void Serialize(std::ostream&) const = 0;

  static bool IsAllowedAtSessionLevel(AttributeType type);
  static bool IsAllowedAtMediaLevel(AttributeType type);
  static const std::string GetAttributeTypeString(AttributeType type);

protected:
  AttributeType mType;
};

inline std::ostream& operator<<(std::ostream& os, const SdpAttribute& attr)
{
  attr.Serialize(os);
  return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                const SdpAttribute::AttributeType type)
{
  os << SdpAttribute::GetAttributeTypeString(type);
  return os;
}

///////////////////////////////////////////////////////////////////////////
// a=candidate, RFC5245
//-------------------------------------------------------------------------
//
// candidate-attribute   = "candidate" ":" foundation SP component-id SP
//                          transport SP
//                          priority SP
//                          connection-address SP     ;from RFC 4566
//                          port         ;port from RFC 4566
//                          SP cand-type
//                          [SP rel-addr]
//                          [SP rel-port]
//                          *(SP extension-att-name SP
//                               extension-att-value)
// foundation            = 1*32ice-char
// component-id          = 1*5DIGIT
// transport             = "UDP" / transport-extension
// transport-extension   = token              ; from RFC 3261
// priority              = 1*10DIGIT
// cand-type             = "typ" SP candidate-types
// candidate-types       = "host" / "srflx" / "prflx" / "relay" / token
// rel-addr              = "raddr" SP connection-address
// rel-port              = "rport" SP port
// extension-att-name    = byte-string    ;from RFC 4566
// extension-att-value   = byte-string
// ice-char              = ALPHA / DIGIT / "+" / "/"

// We use a SdpMultiStringAttribute for candidates

///////////////////////////////////////////////////////////////////////////
// a=connection, RFC4145
//-------------------------------------------------------------------------
//         connection-attr        = "a=connection:" conn-value
//         conn-value             = "new" / "existing"
class SdpConnectionAttribute : public SdpAttribute
{
public:
  enum ConnValue { kNew, kExisting };

  explicit SdpConnectionAttribute(SdpConnectionAttribute::ConnValue value)
      : SdpAttribute(kConnectionAttribute), mValue(value)
  {
  }

  virtual void Serialize(std::ostream& os) const override;

  ConnValue mValue;
};

inline std::ostream& operator<<(std::ostream& os,
                                SdpConnectionAttribute::ConnValue c)
{
  switch (c) {
    case SdpConnectionAttribute::kNew:
      os << "new";
      break;
    case SdpConnectionAttribute::kExisting:
      os << "existing";
      break;
    default:
      MOZ_ASSERT(false);
      os << "?";
  }
  return os;
}

///////////////////////////////////////////////////////////////////////////
// a=sendrecv / a=sendonly / a=recvonly / a=inactive, RFC 4566
//-------------------------------------------------------------------------
class SdpDirectionAttribute : public SdpAttribute
{
public:
  enum Direction {
    kInactive = 0,
    kSendonly = sdp::kSend,
    kRecvonly = sdp::kRecv,
    kSendrecv = sdp::kSend | sdp::kRecv
  };

  explicit SdpDirectionAttribute(Direction value)
      : SdpAttribute(kDirectionAttribute), mValue(value)
  {
  }

  virtual void Serialize(std::ostream& os) const override;

  Direction mValue;
};

inline std::ostream& operator<<(std::ostream& os,
                                SdpDirectionAttribute::Direction d)
{
  switch (d) {
    case SdpDirectionAttribute::kSendonly:
      os << "sendonly";
      break;
    case SdpDirectionAttribute::kRecvonly:
      os << "recvonly";
      break;
    case SdpDirectionAttribute::kSendrecv:
      os << "sendrecv";
      break;
    case SdpDirectionAttribute::kInactive:
      os << "inactive";
      break;
    default:
      MOZ_ASSERT(false);
      os << "?";
  }
  return os;
}

inline SdpDirectionAttribute::Direction
reverse(SdpDirectionAttribute::Direction d)
{
  switch (d) {
    case SdpDirectionAttribute::Direction::kInactive:
      return SdpDirectionAttribute::Direction::kInactive;
    case SdpDirectionAttribute::Direction::kSendonly:
      return SdpDirectionAttribute::Direction::kRecvonly;
    case SdpDirectionAttribute::Direction::kRecvonly:
      return SdpDirectionAttribute::Direction::kSendonly;
    case SdpDirectionAttribute::Direction::kSendrecv:
      return SdpDirectionAttribute::Direction::kSendrecv;
  }
  MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Invalid direction!");
  MOZ_RELEASE_ASSERT(false);
}

inline SdpDirectionAttribute::Direction
operator|(SdpDirectionAttribute::Direction d1,
          SdpDirectionAttribute::Direction d2)
{
  return (SdpDirectionAttribute::Direction)((unsigned)d1 | (unsigned)d2);
}

inline SdpDirectionAttribute::Direction
operator&(SdpDirectionAttribute::Direction d1,
          SdpDirectionAttribute::Direction d2)
{
  return (SdpDirectionAttribute::Direction)((unsigned)d1 & (unsigned)d2);
}

inline SdpDirectionAttribute::Direction
operator|=(SdpDirectionAttribute::Direction& d1,
           SdpDirectionAttribute::Direction d2)
{
  d1 = d1 | d2;
  return d1;
}

inline SdpDirectionAttribute::Direction
operator&=(SdpDirectionAttribute::Direction& d1,
           SdpDirectionAttribute::Direction d2)
{
  d1 = d1 & d2;
  return d1;
}

///////////////////////////////////////////////////////////////////////////
// a=dtls-message, draft-rescorla-dtls-in-sdp
//-------------------------------------------------------------------------
//   attribute               =/   dtls-message-attribute
//
//   dtls-message-attribute  =    "dtls-message" ":" role SP value
//
//   role                    =    "client" / "server"
//
//   value                   =    1*(ALPHA / DIGIT / "+" / "/" / "=" )
//                                ; base64 encoded message
class SdpDtlsMessageAttribute : public SdpAttribute
{
public:
  enum Role {
    kClient,
    kServer
  };

  explicit SdpDtlsMessageAttribute(Role role, const std::string& value)
    : SdpAttribute(kDtlsMessageAttribute),
      mRole(role),
      mValue(value)
  {}

  explicit SdpDtlsMessageAttribute(const std::string& unparsed)
    : SdpAttribute(kDtlsMessageAttribute),
      mRole(kClient)
  {
    std::istringstream is(unparsed);
    std::string error;
    // We're not really worried about errors here if we don't parse;
    // this attribute is a pure optimization.
    Parse(is, &error);
  }

  virtual void Serialize(std::ostream& os) const override;
  bool Parse(std::istream& is, std::string* error);

  Role mRole;
  std::string mValue;
};

inline std::ostream& operator<<(std::ostream& os,
                                SdpDtlsMessageAttribute::Role r)
{
  switch (r) {
    case SdpDtlsMessageAttribute::kClient:
      os << "client";
      break;
    case SdpDtlsMessageAttribute::kServer:
      os << "server";
      break;
    default:
      MOZ_ASSERT(false);
      os << "?";
  }
  return os;
}


///////////////////////////////////////////////////////////////////////////
// a=extmap, RFC5285
//-------------------------------------------------------------------------
// RFC5285
//        extmap = mapentry SP extensionname [SP extensionattributes]
//
//        extensionname = URI
//
//        direction = "sendonly" / "recvonly" / "sendrecv" / "inactive"
//
//        mapentry = "extmap:" 1*5DIGIT ["/" direction]
//
//        extensionattributes = byte-string
//
//        URI = <Defined in RFC 3986>
//
//        byte-string = <Defined in RFC 4566>
//
//        SP = <Defined in RFC 5234>
//
//        DIGIT = <Defined in RFC 5234>
class SdpExtmapAttributeList : public SdpAttribute
{
public:
  SdpExtmapAttributeList() : SdpAttribute(kExtmapAttribute) {}

  struct Extmap {
    uint16_t entry;
    SdpDirectionAttribute::Direction direction;
    bool direction_specified;
    std::string extensionname;
    std::string extensionattributes;
  };

  void
  PushEntry(uint16_t entry, SdpDirectionAttribute::Direction direction,
            bool direction_specified, const std::string& extensionname,
            const std::string& extensionattributes = "")
  {
    Extmap value = { entry, direction, direction_specified, extensionname,
                     extensionattributes };
    mExtmaps.push_back(value);
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<Extmap> mExtmaps;
};

///////////////////////////////////////////////////////////////////////////
// a=fingerprint, RFC4572
//-------------------------------------------------------------------------
//   fingerprint-attribute  =  "fingerprint" ":" hash-func SP fingerprint
//
//   hash-func              =  "sha-1" / "sha-224" / "sha-256" /
//                             "sha-384" / "sha-512" /
//                             "md5" / "md2" / token
//                             ; Additional hash functions can only come
//                             ; from updates to RFC 3279
//
//   fingerprint            =  2UHEX *(":" 2UHEX)
//                             ; Each byte in upper-case hex, separated
//                             ; by colons.
//
//   UHEX                   =  DIGIT / %x41-46 ; A-F uppercase
class SdpFingerprintAttributeList : public SdpAttribute
{
public:
  SdpFingerprintAttributeList() : SdpAttribute(kFingerprintAttribute) {}

  enum HashAlgorithm {
    kSha1,
    kSha224,
    kSha256,
    kSha384,
    kSha512,
    kMd5,
    kMd2,
    kUnknownAlgorithm
  };

  struct Fingerprint {
    HashAlgorithm hashFunc;
    std::vector<uint8_t> fingerprint;
  };

  // For use by application programmers. Enforces that it's a known and
  // non-crazy algorithm.
  void
  PushEntry(std::string algorithm_str,
            const std::vector<uint8_t>& fingerprint,
            bool enforcePlausible = true)
  {
    std::transform(algorithm_str.begin(),
                   algorithm_str.end(),
                   algorithm_str.begin(),
                   ::tolower);

    SdpFingerprintAttributeList::HashAlgorithm algorithm =
        SdpFingerprintAttributeList::kUnknownAlgorithm;

    if (algorithm_str == "sha-1") {
      algorithm = SdpFingerprintAttributeList::kSha1;
    } else if (algorithm_str == "sha-224") {
      algorithm = SdpFingerprintAttributeList::kSha224;
    } else if (algorithm_str == "sha-256") {
      algorithm = SdpFingerprintAttributeList::kSha256;
    } else if (algorithm_str == "sha-384") {
      algorithm = SdpFingerprintAttributeList::kSha384;
    } else if (algorithm_str == "sha-512") {
      algorithm = SdpFingerprintAttributeList::kSha512;
    } else if (algorithm_str == "md5") {
      algorithm = SdpFingerprintAttributeList::kMd5;
    } else if (algorithm_str == "md2") {
      algorithm = SdpFingerprintAttributeList::kMd2;
    }

    if ((algorithm == SdpFingerprintAttributeList::kUnknownAlgorithm) ||
        fingerprint.empty()) {
      if (enforcePlausible) {
        MOZ_ASSERT(false, "Unknown fingerprint algorithm");
      } else {
        return;
      }
    }

    PushEntry(algorithm, fingerprint);
  }

  void
  PushEntry(HashAlgorithm hashFunc, const std::vector<uint8_t>& fingerprint)
  {
    Fingerprint value = { hashFunc, fingerprint };
    mFingerprints.push_back(value);
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<Fingerprint> mFingerprints;

  static std::string FormatFingerprint(const std::vector<uint8_t>& fp);
  static std::vector<uint8_t> ParseFingerprint(const std::string& str);
};

inline std::ostream& operator<<(std::ostream& os,
                                SdpFingerprintAttributeList::HashAlgorithm a)
{
  switch (a) {
    case SdpFingerprintAttributeList::kSha1:
      os << "sha-1";
      break;
    case SdpFingerprintAttributeList::kSha224:
      os << "sha-224";
      break;
    case SdpFingerprintAttributeList::kSha256:
      os << "sha-256";
      break;
    case SdpFingerprintAttributeList::kSha384:
      os << "sha-384";
      break;
    case SdpFingerprintAttributeList::kSha512:
      os << "sha-512";
      break;
    case SdpFingerprintAttributeList::kMd5:
      os << "md5";
      break;
    case SdpFingerprintAttributeList::kMd2:
      os << "md2";
      break;
    default:
      MOZ_ASSERT(false);
      os << "?";
  }
  return os;
}

///////////////////////////////////////////////////////////////////////////
// a=group, RFC5888
//-------------------------------------------------------------------------
//         group-attribute     = "a=group:" semantics
//                               *(SP identification-tag)
//         semantics           = "LS" / "FID" / semantics-extension
//         semantics-extension = token
//         identification-tag  = token
class SdpGroupAttributeList : public SdpAttribute
{
public:
  SdpGroupAttributeList() : SdpAttribute(kGroupAttribute) {}

  enum Semantics {
    kLs,    // RFC5888
    kFid,   // RFC5888
    kSrf,   // RFC3524
    kAnat,  // RFC4091
    kFec,   // RFC5956
    kFecFr, // RFC5956
    kCs,    // draft-mehta-rmt-flute-sdp-05
    kDdp,   // RFC5583
    kDup,   // RFC7104
    kBundle // draft-ietf-mmusic-bundle
  };

  struct Group {
    Semantics semantics;
    std::vector<std::string> tags;
  };

  void
  PushEntry(Semantics semantics, const std::vector<std::string>& tags)
  {
    Group value = { semantics, tags };
    mGroups.push_back(value);
  }

  void
  RemoveMid(const std::string& mid)
  {
    for (auto i = mGroups.begin(); i != mGroups.end();) {
      auto tag = std::find(i->tags.begin(), i->tags.end(), mid);
      if (tag != i->tags.end()) {
        i->tags.erase(tag);
      }

      if (i->tags.empty()) {
        i = mGroups.erase(i);
      } else {
        ++i;
      }
    }
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<Group> mGroups;
};

inline std::ostream& operator<<(std::ostream& os,
                                SdpGroupAttributeList::Semantics s)
{
  switch (s) {
    case SdpGroupAttributeList::kLs:
      os << "LS";
      break;
    case SdpGroupAttributeList::kFid:
      os << "FID";
      break;
    case SdpGroupAttributeList::kSrf:
      os << "SRF";
      break;
    case SdpGroupAttributeList::kAnat:
      os << "ANAT";
      break;
    case SdpGroupAttributeList::kFec:
      os << "FEC";
      break;
    case SdpGroupAttributeList::kFecFr:
      os << "FEC-FR";
      break;
    case SdpGroupAttributeList::kCs:
      os << "CS";
      break;
    case SdpGroupAttributeList::kDdp:
      os << "DDP";
      break;
    case SdpGroupAttributeList::kDup:
      os << "DUP";
      break;
    case SdpGroupAttributeList::kBundle:
      os << "BUNDLE";
      break;
    default:
      MOZ_ASSERT(false);
      os << "?";
  }
  return os;
}

///////////////////////////////////////////////////////////////////////////
// a=identity, draft-ietf-rtcweb-security-arch
//-------------------------------------------------------------------------
//   identity-attribute  = "identity:" identity-assertion
//                         [ SP identity-extension
//                           *(";" [ SP ] identity-extension) ]
//   identity-assertion  = base64
//   base64              = 1*(ALPHA / DIGIT / "+" / "/" / "=" )
//   identity-extension  = extension-att-name [ "=" extension-att-value ]
//   extension-att-name  = token
//   extension-att-value = 1*(%x01-09 / %x0b-0c / %x0e-3a / %x3c-ff)
//                         ; byte-string from [RFC4566] omitting ";"

// We're just using an SdpStringAttribute for this right now
#if 0
class SdpIdentityAttribute : public SdpAttribute
{
public:
  explicit SdpIdentityAttribute(const std::string &assertion,
                                const std::vector<std::string> &extensions =
                                    std::vector<std::string>()) :
    SdpAttribute(kIdentityAttribute),
    mAssertion(assertion),
    mExtensions(extensions) {}

  virtual void Serialize(std::ostream& os) const override;

  std::string mAssertion;
  std::vector<std::string> mExtensions;
}
#endif

///////////////////////////////////////////////////////////////////////////
// a=imageattr, RFC6236
//-------------------------------------------------------------------------
//     image-attr = "imageattr:" PT 1*2( 1*WSP ( "send" / "recv" )
//                                       1*WSP attr-list )
//     PT = 1*DIGIT / "*"
//     attr-list = ( set *(1*WSP set) ) / "*"
//       ;  WSP and DIGIT defined in [RFC5234]
//
//     set= "[" "x=" xyrange "," "y=" xyrange *( "," key-value ) "]"
//                ; x is the horizontal image size range (pixel count)
//                ; y is the vertical image size range (pixel count)
//
//     key-value = ( "sar=" srange )
//               / ( "par=" prange )
//               / ( "q=" qvalue )
//                ; Key-value MAY be extended with other keyword
//                ;  parameters.
//                ; At most, one instance each of sar, par, or q
//                ;  is allowed in a set.
//                ;
//                ; sar (sample aspect ratio) is the sample aspect ratio
//                ;  associated with the set (optional, MAY be ignored)
//                ; par (picture aspect ratio) is the allowed
//                ;  ratio between the display's x and y physical
//                ;  size (optional)
//                ; q (optional, range [0.0..1.0], default value 0.5)
//                ;  is the preference for the given set,
//                ;  a higher value means a higher preference
//
//     onetonine = "1" / "2" / "3" / "4" / "5" / "6" / "7" / "8" / "9"
//                ; Digit between 1 and 9
//     xyvalue = onetonine *5DIGIT
//                ; Digit between 1 and 9 that is
//                ; followed by 0 to 5 other digits
//     step = xyvalue
//     xyrange = ( "[" xyvalue ":" [ step ":" ] xyvalue "]" )
//                ; Range between a lower and an upper value
//                ; with an optional step, default step = 1
//                ; The rightmost occurrence of xyvalue MUST have a
//                ; higher value than the leftmost occurrence.
//             / ( "[" xyvalue 1*( "," xyvalue ) "]" )
//                ; Discrete values separated by ','
//             / ( xyvalue )
//                ; A single value
//     spvalue = ( "0" "." onetonine *3DIGIT )
//                ; Values between 0.1000 and 0.9999
//             / ( onetonine "." 1*4DIGIT )
//                ; Values between 1.0000 and 9.9999
//     srange =  ( "[" spvalue 1*( "," spvalue ) "]" )
//                ; Discrete values separated by ','.
//                ; Each occurrence of spvalue MUST be
//                ; greater than the previous occurrence.
//             / ( "[" spvalue "-" spvalue "]" )
//                ; Range between a lower and an upper level (inclusive)
//                ; The second occurrence of spvalue MUST have a higher
//                ; value than the first
//             / ( spvalue )
//                ; A single value
//
//     prange =  ( "[" spvalue "-" spvalue "]" )
//                ; Range between a lower and an upper level (inclusive)
//                ; The second occurrence of spvalue MUST have a higher
//                ; value than the first
//
//     qvalue  = ( "0" "." 1*2DIGIT )
//             / ( "1" "." 1*2("0") )
//                ; Values between 0.00 and 1.00
//
//  XXX TBD -- We don't use this yet, and it's a project unto itself.
//

class SdpImageattrAttributeList : public SdpAttribute
{
public:
  SdpImageattrAttributeList() : SdpAttribute(kImageattrAttribute) {}

  class XYRange
  {
    public:
      XYRange() : min(0), max(0), step(1) {}
      void Serialize(std::ostream& os) const;
      bool Parse(std::istream& is, std::string* error);
      bool ParseAfterBracket(std::istream& is, std::string* error);
      bool ParseAfterMin(std::istream& is, std::string* error);
      bool ParseDiscreteValues(std::istream& is, std::string* error);
      std::vector<uint32_t> discreteValues;
      // min/max are used iff discreteValues is empty
      uint32_t min;
      uint32_t max;
      uint32_t step;
  };

  class SRange
  {
    public:
      SRange() : min(0), max(0) {}
      void Serialize(std::ostream& os) const;
      bool Parse(std::istream& is, std::string* error);
      bool ParseAfterBracket(std::istream& is, std::string* error);
      bool ParseAfterMin(std::istream& is, std::string* error);
      bool ParseDiscreteValues(std::istream& is, std::string* error);
      bool IsSet() const
      {
        return !discreteValues.empty() || (min && max);
      }
      std::vector<float> discreteValues;
      // min/max are used iff discreteValues is empty
      float min;
      float max;
  };

  class PRange
  {
    public:
      PRange() : min(0), max(0) {}
      void Serialize(std::ostream& os) const;
      bool Parse(std::istream& is, std::string* error);
      bool IsSet() const
      {
        return min && max;
      }
      float min;
      float max;
  };

  class Set
  {
    public:
      Set() : qValue(-1) {}
      void Serialize(std::ostream& os) const;
      bool Parse(std::istream& is, std::string* error);
      XYRange xRange;
      XYRange yRange;
      SRange sRange;
      PRange pRange;
      float qValue;
  };

  class Imageattr
  {
    public:
      Imageattr() : pt(), sendAll(false), recvAll(false) {}
      void Serialize(std::ostream& os) const;
      bool Parse(std::istream& is, std::string* error);
      bool ParseSets(std::istream& is, std::string* error);
      // If not set, this means all payload types
      Maybe<uint16_t> pt;
      bool sendAll;
      std::vector<Set> sendSets;
      bool recvAll;
      std::vector<Set> recvSets;
  };

  virtual void Serialize(std::ostream& os) const override;
  bool PushEntry(const std::string& raw, std::string* error, size_t* errorPos);

  std::vector<Imageattr> mImageattrs;
};

///////////////////////////////////////////////////////////////////////////
// a=msid, draft-ietf-mmusic-msid
//-------------------------------------------------------------------------
//   msid-attr = "msid:" identifier [ SP appdata ]
//   identifier = 1*64token-char ; see RFC 4566
//   appdata = 1*64token-char  ; see RFC 4566
class SdpMsidAttributeList : public SdpAttribute
{
public:
  SdpMsidAttributeList() : SdpAttribute(kMsidAttribute) {}

  struct Msid {
    std::string identifier;
    std::string appdata;
  };

  void
  PushEntry(const std::string& identifier, const std::string& appdata = "")
  {
    Msid value = { identifier, appdata };
    mMsids.push_back(value);
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<Msid> mMsids;
};

///////////////////////////////////////////////////////////////////////////
// a=msid-semantic, draft-ietf-mmusic-msid
//-------------------------------------------------------------------------
//   msid-semantic-attr = "msid-semantic:" msid-semantic msid-list
//   msid-semantic = token ; see RFC 4566
//   msid-list = *(" " msid-id) / " *"
class SdpMsidSemanticAttributeList : public SdpAttribute
{
public:
  SdpMsidSemanticAttributeList() : SdpAttribute(kMsidSemanticAttribute) {}

  struct MsidSemantic
  {
    // TODO: Once we have some more of these, we might want to make an enum
    std::string semantic;
    std::vector<std::string> msids;
  };

  void
  PushEntry(const std::string& semantic, const std::vector<std::string>& msids)
  {
    MsidSemantic value = {semantic, msids};
    mMsidSemantics.push_back(value);
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<MsidSemantic> mMsidSemantics;
};

///////////////////////////////////////////////////////////////////////////
// a=remote-candiate, RFC5245
//-------------------------------------------------------------------------
//   remote-candidate-att = "remote-candidates" ":" remote-candidate
//                           0*(SP remote-candidate)
//   remote-candidate = component-ID SP connection-address SP port
class SdpRemoteCandidatesAttribute : public SdpAttribute
{
public:
  struct Candidate {
    std::string id;
    std::string address;
    uint16_t port;
  };

  explicit SdpRemoteCandidatesAttribute(
      const std::vector<Candidate>& candidates)
      : SdpAttribute(kRemoteCandidatesAttribute), mCandidates(candidates)
  {
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<Candidate> mCandidates;
};

/*
a=rid, draft-pthatcher-mmusic-rid-01

   rid-syntax        = "a=rid:" rid-identifier SP rid-dir
                       [ rid-pt-param-list / rid-param-list ]

   rid-identifier    = 1*(alpha-numeric / "-" / "_")

   rid-dir           = "send" / "recv"

   rid-pt-param-list = SP rid-fmt-list *(";" rid-param)

   rid-param-list    = SP rid-param *(";" rid-param)

   rid-fmt-list      = "pt=" fmt *( "," fmt )
                        ; fmt defined in {{RFC4566}}

   rid-param         = rid-width-param
                       / rid-height-param
                       / rid-fps-param
                       / rid-fs-param
                       / rid-br-param
                       / rid-pps-param
                       / rid-depend-param
                       / rid-param-other

   rid-width-param   = "max-width" [ "=" int-param-val ]

   rid-height-param  = "max-height" [ "=" int-param-val ]

   rid-fps-param     = "max-fps" [ "=" int-param-val ]

   rid-fs-param      = "max-fs" [ "=" int-param-val ]

   rid-br-param      = "max-br" [ "=" int-param-val ]

   rid-pps-param     = "max-pps" [ "=" int-param-val ]

   rid-depend-param  = "depend=" rid-list

   rid-param-other   = 1*(alpha-numeric / "-") [ "=" param-val ]

   rid-list          = rid-identifier *( "," rid-identifier )

   int-param-val     = 1*DIGIT

   param-val         = *( %x20-58 / %x60-7E )
                       ; Any printable character except semicolon
*/
class SdpRidAttributeList : public SdpAttribute
{
public:
  explicit SdpRidAttributeList()
    : SdpAttribute(kRidAttribute)
  {}

  struct Rid
  {
    Rid() :
      direction(sdp::kSend)
    {}

    bool Parse(std::istream& is, std::string* error);
    bool ParseParameters(std::istream& is, std::string* error);
    bool ParseDepend(std::istream& is, std::string* error);
    bool ParseFormats(std::istream& is, std::string* error);
    void Serialize(std::ostream& os) const;
    void SerializeParameters(std::ostream& os) const;
    bool HasFormat(const std::string& format) const;
    bool HasParameters() const
    {
      return !formats.empty() ||
        constraints.maxWidth ||
        constraints.maxHeight ||
        constraints.maxFps ||
        constraints.maxFs ||
        constraints.maxBr ||
        constraints.maxPps ||
        !dependIds.empty();
    }


    std::string id;
    sdp::Direction direction;
    std::vector<uint16_t> formats; // Empty implies all
    EncodingConstraints constraints;
    std::vector<std::string> dependIds;
  };

  virtual void Serialize(std::ostream& os) const override;
  bool PushEntry(const std::string& raw, std::string* error, size_t* errorPos);

  std::vector<Rid> mRids;
};

///////////////////////////////////////////////////////////////////////////
// a=rtcp, RFC3605
//-------------------------------------------------------------------------
//   rtcp-attribute =  "a=rtcp:" port  [nettype space addrtype space
//                         connection-address] CRLF
class SdpRtcpAttribute : public SdpAttribute
{
public:
  explicit SdpRtcpAttribute(uint16_t port)
    : SdpAttribute(kRtcpAttribute),
      mPort(port),
      mNetType(sdp::kNetTypeNone),
      mAddrType(sdp::kAddrTypeNone)
  {}

  SdpRtcpAttribute(uint16_t port,
                   sdp::NetType netType,
                   sdp::AddrType addrType,
                   const std::string& address)
      : SdpAttribute(kRtcpAttribute),
        mPort(port),
        mNetType(netType),
        mAddrType(addrType),
        mAddress(address)
  {
    MOZ_ASSERT(netType != sdp::kNetTypeNone);
    MOZ_ASSERT(addrType != sdp::kAddrTypeNone);
    MOZ_ASSERT(!address.empty());
  }

  virtual void Serialize(std::ostream& os) const override;

  uint16_t mPort;
  sdp::NetType mNetType;
  sdp::AddrType mAddrType;
  std::string mAddress;
};

///////////////////////////////////////////////////////////////////////////
// a=rtcp-fb, RFC4585
//-------------------------------------------------------------------------
//    rtcp-fb-syntax = "a=rtcp-fb:" rtcp-fb-pt SP rtcp-fb-val CRLF
//
//    rtcp-fb-pt         = "*"   ; wildcard: applies to all formats
//                       / fmt   ; as defined in SDP spec
//
//    rtcp-fb-val        = "ack" rtcp-fb-ack-param
//                       / "nack" rtcp-fb-nack-param
//                       / "trr-int" SP 1*DIGIT
//                       / rtcp-fb-id rtcp-fb-param
//
//    rtcp-fb-id         = 1*(alpha-numeric / "-" / "_")
//
//    rtcp-fb-param      = SP "app" [SP byte-string]
//                       / SP token [SP byte-string]
//                       / ; empty
//
//    rtcp-fb-ack-param  = SP "rpsi"
//                       / SP "app" [SP byte-string]
//                       / SP token [SP byte-string]
//                       / ; empty
//
//    rtcp-fb-nack-param = SP "pli"
//                       / SP "sli"
//                       / SP "rpsi"
//                       / SP "app" [SP byte-string]
//                       / SP token [SP byte-string]
//                       / ; empty
//
class SdpRtcpFbAttributeList : public SdpAttribute
{
public:
  SdpRtcpFbAttributeList() : SdpAttribute(kRtcpFbAttribute) {}

  enum Type { kAck, kApp, kCcm, kNack, kTrrInt, kRemb };

  static const char* pli;
  static const char* sli;
  static const char* rpsi;
  static const char* app;

  static const char* fir;
  static const char* tmmbr;
  static const char* tstr;
  static const char* vbcm;

  struct Feedback {
    std::string pt;
    Type type;
    std::string parameter;
    std::string extra;
  };

  void
  PushEntry(const std::string& pt, Type type, const std::string& parameter = "",
            const std::string& extra = "")
  {
    Feedback value = { pt, type, parameter, extra };
    mFeedbacks.push_back(value);
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<Feedback> mFeedbacks;
};

inline std::ostream& operator<<(std::ostream& os,
                                SdpRtcpFbAttributeList::Type type)
{
  switch (type) {
    case SdpRtcpFbAttributeList::kAck:
      os << "ack";
      break;
    case SdpRtcpFbAttributeList::kApp:
      os << "app";
      break;
    case SdpRtcpFbAttributeList::kCcm:
      os << "ccm";
      break;
    case SdpRtcpFbAttributeList::kNack:
      os << "nack";
      break;
    case SdpRtcpFbAttributeList::kTrrInt:
      os << "trr-int";
      break;
    case SdpRtcpFbAttributeList::kRemb:
      os << "goog-remb";
      break;
    default:
      MOZ_ASSERT(false);
      os << "?";
  }
  return os;
}

///////////////////////////////////////////////////////////////////////////
// a=rtpmap, RFC4566
//-------------------------------------------------------------------------
// a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
class SdpRtpmapAttributeList : public SdpAttribute
{
public:
  SdpRtpmapAttributeList() : SdpAttribute(kRtpmapAttribute) {}

  // Minimal set to get going
  enum CodecType {
    kOpus,
    kG722,
    kPCMU,
    kPCMA,
    kVP8,
    kVP9,
    kiLBC,
    kiSAC,
    kH264,
    kRed,
    kUlpfec,
    kTelephoneEvent,
    kOtherCodec
  };

  struct Rtpmap {
    std::string pt;
    CodecType codec;
    std::string name;
    uint32_t clock;
    // Technically, this could mean something else in the future.
    // In practice, that's probably not going to happen.
    uint32_t channels;
  };

  void
  PushEntry(const std::string& pt, CodecType codec, const std::string& name,
            uint32_t clock, uint32_t channels = 0)
  {
    Rtpmap value = { pt, codec, name, clock, channels };
    mRtpmaps.push_back(value);
  }

  virtual void Serialize(std::ostream& os) const override;

  bool
  HasEntry(const std::string& pt) const
  {
    for (auto it = mRtpmaps.begin(); it != mRtpmaps.end(); ++it) {
      if (it->pt == pt) {
        return true;
      }
    }
    return false;
  }

  const Rtpmap&
  GetEntry(const std::string& pt) const
  {
    for (auto it = mRtpmaps.begin(); it != mRtpmaps.end(); ++it) {
      if (it->pt == pt) {
        return *it;
      }
    }
    MOZ_CRASH();
  }

  std::vector<Rtpmap> mRtpmaps;
};

inline std::ostream& operator<<(std::ostream& os,
                                SdpRtpmapAttributeList::CodecType c)
{
  switch (c) {
    case SdpRtpmapAttributeList::kOpus:
      os << "opus";
      break;
    case SdpRtpmapAttributeList::kG722:
      os << "G722";
      break;
    case SdpRtpmapAttributeList::kPCMU:
      os << "PCMU";
      break;
    case SdpRtpmapAttributeList::kPCMA:
      os << "PCMA";
      break;
    case SdpRtpmapAttributeList::kVP8:
      os << "VP8";
      break;
    case SdpRtpmapAttributeList::kVP9:
      os << "VP9";
      break;
    case SdpRtpmapAttributeList::kiLBC:
      os << "iLBC";
      break;
    case SdpRtpmapAttributeList::kiSAC:
      os << "iSAC";
      break;
    case SdpRtpmapAttributeList::kH264:
      os << "H264";
      break;
    case SdpRtpmapAttributeList::kRed:
      os << "red";
      break;
    case SdpRtpmapAttributeList::kUlpfec:
      os << "ulpfec";
      break;
    case SdpRtpmapAttributeList::kTelephoneEvent:
      os << "telephone-event";
      break;
    default:
      MOZ_ASSERT(false);
      os << "?";
  }
  return os;
}

///////////////////////////////////////////////////////////////////////////
// a=fmtp, RFC4566, RFC5576
//-------------------------------------------------------------------------
//       a=fmtp:<format> <format specific parameters>
//
class SdpFmtpAttributeList : public SdpAttribute
{
public:
  SdpFmtpAttributeList() : SdpAttribute(kFmtpAttribute) {}

  // Base class for format parameters
  class Parameters
  {
  public:
    explicit Parameters(SdpRtpmapAttributeList::CodecType aCodec)
        : codec_type(aCodec)
    {
    }

    virtual ~Parameters() {}
    virtual Parameters* Clone() const = 0;
    virtual void Serialize(std::ostream& os) const = 0;

    SdpRtpmapAttributeList::CodecType codec_type;
  };

  class RedParameters : public Parameters
  {
  public:
    RedParameters()
        : Parameters(SdpRtpmapAttributeList::kRed)
    {
    }

    virtual Parameters*
    Clone() const override
    {
      return new RedParameters(*this);
    }

    virtual void
    Serialize(std::ostream& os) const override
    {
      for(size_t i = 0; i < encodings.size(); ++i) {
        os << (i != 0 ? "/" : "")
           << std::to_string(encodings[i]);
      }
    }

    std::vector<uint8_t> encodings;
  };

  class H264Parameters : public Parameters
  {
  public:
    static const uint32_t kDefaultProfileLevelId = 0x420010;

    H264Parameters()
        : Parameters(SdpRtpmapAttributeList::kH264),
          packetization_mode(0),
          level_asymmetry_allowed(false),
          profile_level_id(kDefaultProfileLevelId),
          max_mbps(0),
          max_fs(0),
          max_cpb(0),
          max_dpb(0),
          max_br(0)
    {
      memset(sprop_parameter_sets, 0, sizeof(sprop_parameter_sets));
    }

    virtual Parameters*
    Clone() const override
    {
      return new H264Parameters(*this);
    }

    virtual void
    Serialize(std::ostream& os) const override
    {
      // Note: don't move this, since having an unconditional param up top
      // lets us avoid a whole bunch of conditional streaming of ';' below
      os << "profile-level-id=" << std::hex << std::setfill('0') << std::setw(6)
         << profile_level_id << std::dec << std::setfill(' ');

      os << ";level-asymmetry-allowed=" << (level_asymmetry_allowed ? 1 : 0);

      if (strlen(sprop_parameter_sets)) {
        os << ";sprop-parameter-sets=" << sprop_parameter_sets;
      }

      if (packetization_mode != 0) {
        os << ";packetization-mode=" << packetization_mode;
      }

      if (max_mbps != 0) {
        os << ";max-mbps=" << max_mbps;
      }

      if (max_fs != 0) {
        os << ";max-fs=" << max_fs;
      }

      if (max_cpb != 0) {
        os << ";max-cpb=" << max_cpb;
      }

      if (max_dpb != 0) {
        os << ";max-dpb=" << max_dpb;
      }

      if (max_br != 0) {
        os << ";max-br=" << max_br;
      }
    }

    static const size_t max_sprop_len = 128;
    char sprop_parameter_sets[max_sprop_len];
    unsigned int packetization_mode;
    bool level_asymmetry_allowed;
    unsigned int profile_level_id;
    unsigned int max_mbps;
    unsigned int max_fs;
    unsigned int max_cpb;
    unsigned int max_dpb;
    unsigned int max_br;
  };

  // Also used for VP9 since they share parameters
  class VP8Parameters : public Parameters
  {
  public:
    explicit VP8Parameters(SdpRtpmapAttributeList::CodecType type)
        : Parameters(type), max_fs(0), max_fr(0)
    {
    }

    virtual Parameters*
    Clone() const override
    {
      return new VP8Parameters(*this);
    }

    virtual void
    Serialize(std::ostream& os) const override
    {
      // draft-ietf-payload-vp8-11 says these are mandatory, upper layer
      // needs to ensure they're set properly.
      os << "max-fs=" << max_fs;
      os << ";max-fr=" << max_fr;
    }

    unsigned int max_fs;
    unsigned int max_fr;
  };

  class OpusParameters : public Parameters
  {
  public:
    enum { kDefaultMaxPlaybackRate = 48000,
           kDefaultStereo = 0,
           kDefaultUseInBandFec = 0 };
    OpusParameters() :
      Parameters(SdpRtpmapAttributeList::kOpus),
      maxplaybackrate(kDefaultMaxPlaybackRate),
      stereo(kDefaultStereo),
      useInBandFec(kDefaultUseInBandFec)
    {}

    Parameters*
    Clone() const override
    {
      return new OpusParameters(*this);
    }

    void
    Serialize(std::ostream& os) const override
    {
      os << "maxplaybackrate=" << maxplaybackrate
         << ";stereo=" << stereo
         << ";useinbandfec=" << useInBandFec;
    }

    unsigned int maxplaybackrate;
    unsigned int stereo;
    unsigned int useInBandFec;
  };

  class TelephoneEventParameters : public Parameters
  {
  public:
    TelephoneEventParameters() :
      Parameters(SdpRtpmapAttributeList::kTelephoneEvent),
      dtmfTones("0-15")
    {}

    virtual Parameters*
    Clone() const override
    {
      return new TelephoneEventParameters(*this);
    }

    void
    Serialize(std::ostream& os) const override
    {
      os << dtmfTones;
    }

    std::string dtmfTones;
  };

  class Fmtp
  {
  public:
    Fmtp(const std::string& aFormat, UniquePtr<Parameters> aParameters)
        : format(aFormat),
          parameters(std::move(aParameters))
    {
    }

    Fmtp(const std::string& aFormat, const Parameters& aParameters)
        : format(aFormat),
          parameters(aParameters.Clone())
    {
    }

    // TODO: Rip all of this out when we have move semantics in the stl.
    Fmtp(const Fmtp& orig) { *this = orig; }

    Fmtp& operator=(const Fmtp& rhs)
    {
      if (this != &rhs) {
        format = rhs.format;
        parameters.reset(rhs.parameters ? rhs.parameters->Clone() : nullptr);
      }
      return *this;
    }

    // The contract around these is as follows:
    // * |parameters| is only set if we recognized the media type and had
    //   a subclass of Parameters to represent that type of parameters
    // * |parameters| is a best-effort representation; it might be missing
    //   stuff
    // * Parameters::codec_type tells you the concrete class, eg
    //   kH264 -> H264Parameters
    std::string format;
    UniquePtr<Parameters> parameters;
  };

  virtual void Serialize(std::ostream& os) const override;

  void
  PushEntry(const std::string& format, UniquePtr<Parameters> parameters)
  {
    mFmtps.push_back(Fmtp(format, std::move(parameters)));
  }

  std::vector<Fmtp> mFmtps;
};

///////////////////////////////////////////////////////////////////////////
// a=sctpmap, draft-ietf-mmusic-sctp-sdp-05
//-------------------------------------------------------------------------
//      sctpmap-attr        =  "a=sctpmap:" sctpmap-number media-subtypes
// [streams]
//      sctpmap-number      =  1*DIGIT
//      protocol            =  labelstring
//        labelstring         =  text
//        text                =  byte-string
//      streams      =  1*DIGIT
//
// We're going to pretend that there are spaces where they make sense.
class SdpSctpmapAttributeList : public SdpAttribute
{
public:
  SdpSctpmapAttributeList() : SdpAttribute(kSctpmapAttribute) {}

  struct Sctpmap {
    std::string pt;
    std::string name;
    uint32_t streams;
  };

  void
  PushEntry(const std::string& pt, const std::string& name,
            uint32_t streams = 0)
  {
    Sctpmap value = { pt, name, streams };
    mSctpmaps.push_back(value);
  }

  virtual void Serialize(std::ostream& os) const override;

  bool
  HasEntry(const std::string& pt) const
  {
    for (auto it = mSctpmaps.begin(); it != mSctpmaps.end(); ++it) {
      if (it->pt == pt) {
        return true;
      }
    }
    return false;
  }

  const Sctpmap&
  GetFirstEntry() const
  {
    return mSctpmaps[0];
  }

  std::vector<Sctpmap> mSctpmaps;
};

///////////////////////////////////////////////////////////////////////////
// a=setup, RFC4145
//-------------------------------------------------------------------------
//       setup-attr           =  "a=setup:" role
//       role                 =  "active" / "passive" / "actpass" / "holdconn"
class SdpSetupAttribute : public SdpAttribute
{
public:
  enum Role { kActive, kPassive, kActpass, kHoldconn };

  explicit SdpSetupAttribute(Role role)
      : SdpAttribute(kSetupAttribute), mRole(role)
  {
  }

  virtual void Serialize(std::ostream& os) const override;

  Role mRole;
};

inline std::ostream& operator<<(std::ostream& os, SdpSetupAttribute::Role r)
{
  switch (r) {
    case SdpSetupAttribute::kActive:
      os << "active";
      break;
    case SdpSetupAttribute::kPassive:
      os << "passive";
      break;
    case SdpSetupAttribute::kActpass:
      os << "actpass";
      break;
    case SdpSetupAttribute::kHoldconn:
      os << "holdconn";
      break;
    default:
      MOZ_ASSERT(false);
      os << "?";
  }
  return os;
}

// sc-attr     = "a=simulcast:" 1*2( WSP sc-str-list ) [WSP sc-pause-list]
// sc-str-list = sc-dir WSP sc-id-type "=" sc-alt-list *( ";" sc-alt-list )
// sc-pause-list = "paused=" sc-alt-list
// sc-dir      = "send" / "recv"
// sc-id-type  = "pt" / "rid" / token
// sc-alt-list = sc-id *( "," sc-id )
// sc-id       = fmt / rid-identifier / token
// ; WSP defined in [RFC5234]
// ; fmt, token defined in [RFC4566]
// ; rid-identifier defined in [I-D.pthatcher-mmusic-rid]
class SdpSimulcastAttribute : public SdpAttribute
{
public:
  SdpSimulcastAttribute() : SdpAttribute(kSimulcastAttribute) {}

  void Serialize(std::ostream& os) const override;
  bool Parse(std::istream& is, std::string* error);

  class Version
  {
    public:
      void Serialize(std::ostream& os) const;
      bool IsSet() const
      {
        return !choices.empty();
      }
      bool Parse(std::istream& is, std::string* error);
      bool GetChoicesAsFormats(std::vector<uint16_t>* formats) const;

      std::vector<std::string> choices;
  };

  class Versions : public std::vector<Version>
  {
    public:
      enum Type {
        kPt,
        kRid
      };

      Versions() : type(kRid) {}
      void Serialize(std::ostream& os) const;
      bool IsSet() const
      {
        if (empty()) {
          return false;
        }

        for (const Version& version : *this) {
          if (version.IsSet()) {
            return true;
          }
        }

        return false;
      }

      bool Parse(std::istream& is, std::string* error);
      Type type;
  };

  Versions sendVersions;
  Versions recvVersions;
};

///////////////////////////////////////////////////////////////////////////
// a=ssrc, RFC5576
//-------------------------------------------------------------------------
// ssrc-attr = "ssrc:" ssrc-id SP attribute
// ; The base definition of "attribute" is in RFC 4566.
// ; (It is the content of "a=" lines.)
//
// ssrc-id = integer ; 0 .. 2**32 - 1
//-------------------------------------------------------------------------
// TODO -- In the future, it might be nice if we ran a parse on the
// attribute section of this so that we could interpret it semantically.
// For WebRTC, the key use case for a=ssrc is assocaiting SSRCs with
// media sections, and we're not really going to care about the attribute
// itself. So we're just going to store it as a string for the time being.
// Issue 187.
class SdpSsrcAttributeList : public SdpAttribute
{
public:
  SdpSsrcAttributeList() : SdpAttribute(kSsrcAttribute) {}

  struct Ssrc {
    uint32_t ssrc;
    std::string attribute;
  };

  void
  PushEntry(uint32_t ssrc, const std::string& attribute)
  {
    Ssrc value = { ssrc, attribute };
    mSsrcs.push_back(value);
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<Ssrc> mSsrcs;
};

///////////////////////////////////////////////////////////////////////////
// a=ssrc-group, RFC5576
//-------------------------------------------------------------------------
// ssrc-group-attr = "ssrc-group:" semantics *(SP ssrc-id)
//
// semantics       = "FEC" / "FID" / token
//
// ssrc-id = integer ; 0 .. 2**32 - 1
class SdpSsrcGroupAttributeList : public SdpAttribute
{
public:
  enum Semantics {
    kFec,   // RFC5576
    kFid,   // RFC5576
    kFecFr, // RFC5956
    kDup    // RFC7104
  };

  struct SsrcGroup {
    Semantics semantics;
    std::vector<uint32_t> ssrcs;
  };

  SdpSsrcGroupAttributeList() : SdpAttribute(kSsrcGroupAttribute) {}

  void
  PushEntry(Semantics semantics, const std::vector<uint32_t>& ssrcs)
  {
    SsrcGroup value = { semantics, ssrcs };
    mSsrcGroups.push_back(value);
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<SsrcGroup> mSsrcGroups;
};

inline std::ostream& operator<<(std::ostream& os,
                                SdpSsrcGroupAttributeList::Semantics s)
{
  switch (s) {
    case SdpSsrcGroupAttributeList::kFec:
      os << "FEC";
      break;
    case SdpSsrcGroupAttributeList::kFid:
      os << "FID";
      break;
    case SdpSsrcGroupAttributeList::kFecFr:
      os << "FEC-FR";
      break;
    case SdpSsrcGroupAttributeList::kDup:
      os << "DUP";
      break;
    default:
      MOZ_ASSERT(false);
      os << "?";
  }
  return os;
}

///////////////////////////////////////////////////////////////////////////
class SdpMultiStringAttribute : public SdpAttribute
{
public:
  explicit SdpMultiStringAttribute(AttributeType type) : SdpAttribute(type) {}

  void
  PushEntry(const std::string& entry)
  {
    mValues.push_back(entry);
  }

  virtual void Serialize(std::ostream& os) const override;

  std::vector<std::string> mValues;
};

// otherwise identical to SdpMultiStringAttribute, this is used for
// ice-options and other places where the value is serialized onto
// a single line with space separating tokens
class SdpOptionsAttribute : public SdpAttribute
{
public:
  explicit SdpOptionsAttribute(AttributeType type) : SdpAttribute(type) {}

  void
  PushEntry(const std::string& entry)
  {
    mValues.push_back(entry);
  }

  void Load(const std::string& value);

  virtual void Serialize(std::ostream& os) const override;

  std::vector<std::string> mValues;
};

// Used for attributes that take no value (eg; a=ice-lite)
class SdpFlagAttribute : public SdpAttribute
{
public:
  explicit SdpFlagAttribute(AttributeType type) : SdpAttribute(type) {}

  virtual void Serialize(std::ostream& os) const override;
};

// Used for any other kind of single-valued attribute not otherwise specialized
class SdpStringAttribute : public SdpAttribute
{
public:
  explicit SdpStringAttribute(AttributeType type, const std::string& value)
      : SdpAttribute(type), mValue(value)
  {
  }

  virtual void Serialize(std::ostream& os) const override;

  std::string mValue;
};

// Used for any purely (non-negative) numeric attribute
class SdpNumberAttribute : public SdpAttribute
{
public:
  explicit SdpNumberAttribute(AttributeType type, uint32_t value = 0)
      : SdpAttribute(type), mValue(value)
  {
  }

  virtual void Serialize(std::ostream& os) const override;

  uint32_t mValue;
};

} // namespace mozilla

#endif
