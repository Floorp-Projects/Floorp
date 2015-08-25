/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/sdp/SdpAttribute.h"
#include "signaling/src/sdp/SdpHelper.h"

#include <iomanip>

#ifdef CRLF
#undef CRLF
#endif
#define CRLF "\r\n"

namespace mozilla
{

void
SdpConnectionAttribute::Serialize(std::ostream& os) const
{
  os << "a=" << mType << ":" << mValue << CRLF;
}

void
SdpDirectionAttribute::Serialize(std::ostream& os) const
{
  os << "a=" << mValue << CRLF;
}

void
SdpExtmapAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mExtmaps.begin(); i != mExtmaps.end(); ++i) {
    os << "a=" << mType << ":" << i->entry;
    if (i->direction_specified) {
      os << "/" << i->direction;
    }
    os << " " << i->extensionname;
    if (i->extensionattributes.length()) {
      os << " " << i->extensionattributes;
    }
    os << CRLF;
  }
}

void
SdpFingerprintAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mFingerprints.begin(); i != mFingerprints.end(); ++i) {
    os << "a=" << mType << ":" << i->hashFunc << " "
       << FormatFingerprint(i->fingerprint) << CRLF;
  }
}

// Format the fingerprint in RFC 4572 Section 5 attribute format
std::string
SdpFingerprintAttributeList::FormatFingerprint(const std::vector<uint8_t>& fp)
{
  if (fp.empty()) {
    MOZ_ASSERT(false, "Cannot format an empty fingerprint.");
    return "";
  }

  std::ostringstream os;
  for (auto i = fp.begin(); i != fp.end(); ++i) {
    os << ":" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
       << static_cast<uint32_t>(*i);
  }
  return os.str().substr(1);
}

static uint8_t
FromUppercaseHex(char ch)
{
  if ((ch >= '0') && (ch <= '9')) {
    return ch - '0';
  }
  if ((ch >= 'A') && (ch <= 'F')) {
    return ch - 'A' + 10;
  }
  return 16; // invalid
}

// Parse the fingerprint from RFC 4572 Section 5 attribute format
std::vector<uint8_t>
SdpFingerprintAttributeList::ParseFingerprint(const std::string& str)
{
  size_t targetSize = (str.length() + 1) / 3;
  std::vector<uint8_t> fp(targetSize);
  size_t fpIndex = 0;

  if (str.length() % 3 != 2) {
    fp.clear();
    return fp;
  }

  for (size_t i = 0; i < str.length(); i += 3) {
    uint8_t high = FromUppercaseHex(str[i]);
    uint8_t low = FromUppercaseHex(str[i + 1]);
    if (high > 0xf || low > 0xf ||
        (i + 2 < str.length() && str[i + 2] != ':')) {
      fp.clear(); // error
      return fp;
    }
    fp[fpIndex++] = high << 4 | low;
  }
  return fp;
}

void
SdpFmtpAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mFmtps.begin(); i != mFmtps.end(); ++i) {
    os << "a=" << mType << ":" << i->format << " ";
    if (i->parameters) {
      i->parameters->Serialize(os);
    } else {
      os << i->parameters_string;
    }
    os << CRLF;
  }
}

void
SdpGroupAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mGroups.begin(); i != mGroups.end(); ++i) {
    os << "a=" << mType << ":" << i->semantics;
    for (auto j = i->tags.begin(); j != i->tags.end(); ++j) {
      os << " " << (*j);
    }
    os << CRLF;
  }
}

// We're just using an SdpStringAttribute for this right now
#if 0
void SdpIdentityAttribute::Serialize(std::ostream& os) const
{
  os << "a=" << mType << ":" << mAssertion;
  for (auto i = mExtensions.begin(); i != mExtensions.end(); i++) {
    os << (i == mExtensions.begin() ? " " : ";") << (*i);
  }
  os << CRLF;
}
#endif

void
SdpImageattrAttributeList::XYRange::Serialize(std::ostream& os) const
{
  if (discreteValues.size() == 0) {
    os << "[" << min << ":";
    if (step != 1) {
      os << step << ":";
    }
    os << max << "]";
  } else if (discreteValues.size() == 1) {
    os << discreteValues.front();
  } else {
    os << "[";
    bool first = true;
    for (auto value : discreteValues) {
      if (!first) {
        os << ",";
      }
      first = false;
      os << value;
    }
    os << "]";
  }
}

static unsigned char
PeekChar(std::istream& is, std::string* error)
{
  int next = is.peek();
  if (next == EOF) {
    *error = "Truncated";
    return 0;
  }

  return next;
}

static bool
SkipChar(std::istream& is, unsigned char c, std::string* error)
{
  if (PeekChar(is, error) != c) {
    *error = "Expected \'";
    error->push_back(c);
    error->push_back('\'');
    return false;
  }

  is.get();
  return true;
}

template<typename T>
bool
GetUnsigned(std::istream& is, T min, T max, T* value, std::string* error)
{
  if (PeekChar(is, error) == '-') {
    *error = "Value is less than 0";
    return false;
  }

  is >> std::noskipws >> *value;

  if (is.fail()) {
    *error = "Malformed";
    return false;
  }

  if (*value < min) {
    *error = "Value too small";
    return false;
  }

  if (*value > max) {
    *error = "Value too large";
    return false;
  }

  return true;
}

static bool
GetXYValue(std::istream& is, uint32_t* value, std::string* error)
{
  return GetUnsigned<uint32_t>(is, 1, 999999, value, error);
}

bool
SdpImageattrAttributeList::XYRange::ParseDiscreteValues(std::istream& is,
                                                        std::string* error)
{
  do {
    uint32_t value;
    if (!GetXYValue(is, &value, error)) {
      return false;
    }
    discreteValues.push_back(value);
  } while (SkipChar(is, ',', error));

  return SkipChar(is, ']', error);
}

bool
SdpImageattrAttributeList::XYRange::ParseAfterMin(std::istream& is,
                                                  std::string* error)
{
  // We have already parsed "[320:", and now expect another uint
  uint32_t value;
  if (!GetXYValue(is, &value, error)) {
    return false;
  }

  if (SkipChar(is, ':', error)) {
    // Range with step eg [320:16:640]
    step = value;
    // Now |value| should be the max
    if (!GetXYValue(is, &value, error)) {
      return false;
    }
  }

  max = value;
  if (min >= max) {
    *error = "Min is not smaller than max";
    return false;
  }

  return SkipChar(is, ']', error);
}

bool
SdpImageattrAttributeList::XYRange::ParseAfterBracket(std::istream& is,
                                                      std::string* error)
{
  // Either a range, or a list of discrete values
  // [320:640], [320:16:640], or [320,640]
  uint32_t value;
  if (!GetXYValue(is, &value, error)) {
    return false;
  }

  if (SkipChar(is, ':', error)) {
    // Range - [640:480] or [640:16:480]
    min = value;
    return ParseAfterMin(is, error);
  }

  if (SkipChar(is, ',', error)) {
    discreteValues.push_back(value);
    return ParseDiscreteValues(is, error);
  }

  *error = "Expected \':\' or \',\'";
  return false;
}

bool
SdpImageattrAttributeList::XYRange::Parse(std::istream& is, std::string* error)
{
  if (SkipChar(is, '[', error)) {
    return ParseAfterBracket(is, error);
  }

  // Single discrete value
  uint32_t value;
  if (!GetXYValue(is, &value, error)) {
    return false;
  }
  discreteValues.push_back(value);

  return true;
}

static bool
GetSPValue(std::istream& is, float* value, std::string* error)
{
  return GetUnsigned<float>(is, 0.1f, 9.9999f, value, error);
}

static bool
GetQValue(std::istream& is, float* value, std::string* error)
{
  return GetUnsigned<float>(is, 0.0f, 1.0f, value, error);
}

bool
SdpImageattrAttributeList::SRange::ParseDiscreteValues(std::istream& is,
                                                        std::string* error)
{
  do {
    float value;
    if (!GetSPValue(is, &value, error)) {
      return false;
    }
    discreteValues.push_back(value);
  } while (SkipChar(is, ',', error));

  return SkipChar(is, ']', error);
}

bool
SdpImageattrAttributeList::SRange::ParseAfterMin(std::istream& is,
                                                  std::string* error)
{
  if (!GetSPValue(is, &max, error)) {
    return false;
  }

  if (min >= max) {
    *error = "Min is not smaller than max";
    return false;
  }

  return SkipChar(is, ']', error);
}

bool
SdpImageattrAttributeList::SRange::ParseAfterBracket(std::istream& is,
                                                     std::string* error)
{
  // Either a range, or a list of discrete values
  float value;
  if (!GetSPValue(is, &value, error)) {
    return false;
  }

  if (SkipChar(is, '-', error)) {
    min = value;
    return ParseAfterMin(is, error);
  }

  if (SkipChar(is, ',', error)) {
    discreteValues.push_back(value);
    return ParseDiscreteValues(is, error);
  }

  *error = "Expected either \'-\' or \',\'";
  return false;
}

bool
SdpImageattrAttributeList::SRange::Parse(std::istream& is, std::string* error)
{
  if (SkipChar(is, '[', error)) {
    return ParseAfterBracket(is, error);
  }

  // Single discrete value
  float value;
  if (!GetSPValue(is, &value, error)) {
    return false;
  }
  discreteValues.push_back(value);
  return true;
}

bool
SdpImageattrAttributeList::PRange::Parse(std::istream& is, std::string* error)
{
  if (!SkipChar(is, '[', error)) {
    return false;
  }

  if (!GetSPValue(is, &min, error)) {
    return false;
  }

  if (!SkipChar(is, '-', error)) {
    return false;
  }

  if (!GetSPValue(is, &max, error)) {
    return false;
  }

  if (min >= max) {
    *error = "min must be smaller than max";
    return false;
  }

  if (!SkipChar(is, ']', error)) {
    return false;
  }
  return true;
}

void
SdpImageattrAttributeList::SRange::Serialize(std::ostream& os) const
{
  os << std::setprecision(4) << std::fixed;
  if (discreteValues.size() == 0) {
    os << "[" << min << "-" << max << "]";
  } else if (discreteValues.size() == 1) {
    os << discreteValues.front();
  } else {
    os << "[";
    bool first = true;
    for (auto value : discreteValues) {
      if (!first) {
        os << ",";
      }
      first = false;
      os << value;
    }
    os << "]";
  }
}

void
SdpImageattrAttributeList::PRange::Serialize(std::ostream& os) const
{
  os << std::setprecision(4) << std::fixed;
  os << "[" << min << "-" << max << "]";
}

static std::string ParseKey(std::istream& is, std::string* error)
{
  is >> std::ws;
  std::string key;
  while (is && PeekChar(is, error) != '=') {
    key.push_back(std::tolower(is.get()));
  }

  if (!SkipChar(is, '=', error)) {
    return "";
  }

  return key;
}

static bool SkipBraces(std::istream& is, std::string* error)
{
  if (PeekChar(is, error) != '[') {
    *error = "Expected \'[\'";
    return false;
  }

  size_t braceCount = 0;
  do {
    switch (PeekChar(is, error)) {
      case '[':
        ++braceCount;
        break;
      case ']':
        --braceCount;
        break;
      default:
        break;
    }
    is.get();
  } while (braceCount && is);

  if (!is) {
    *error = "Expected closing brace";
    return false;
  }

  return true;
}

// Assumptions:
// 1. If the value contains '[' or ']', they are balanced.
// 2. The value contains no ',' outside of brackets.
static bool SkipValue(std::istream& is, std::string* error)
{
  while (is) {
    switch (PeekChar(is, error)) {
      case ',':
      case ']':
        return true;
      case '[':
        if (!SkipBraces(is, error)) {
          return false;
        }
        break;
      default:
        is.get();
    }
  }

  *error = "No closing \']\' on set";
  return false;
}

bool
SdpImageattrAttributeList::Set::Parse(std::istream& is, std::string* error)
{
  if (!SkipChar(is, '[', error)) {
    return false;
  }

  if (ParseKey(is, error) != "x") {
    *error = "Expected x=";
    return false;
  }

  if (!xRange.Parse(is, error)) {
    return false;
  }

  if (!SkipChar(is, ',', error)) {
    return false;
  }

  if (ParseKey(is, error) != "y") {
    *error = "Expected y=";
    return false;
  }

  if (!yRange.Parse(is, error)) {
    return false;
  }

  qValue = 0.5f; // default

  bool gotSar = false;
  bool gotPar = false;
  bool gotQ = false;

  while (SkipChar(is, ',', error)) {
    std::string key = ParseKey(is, error);
    if (key.empty()) {
      *error = "Expected key-value";
      return false;
    }

    if (key == "sar") {
      if (gotSar) {
        *error = "Extra sar parameter";
        return false;
      }
      gotSar = true;
      if (!sRange.Parse(is, error)) {
        return false;
      }
    } else if (key == "par") {
      if (gotPar) {
        *error = "Extra par parameter";
        return false;
      }
      gotPar = true;
      if (!pRange.Parse(is, error)) {
        return false;
      }
    } else if (key == "q") {
      if (gotQ) {
        *error = "Extra q parameter";
        return false;
      }
      gotQ = true;
      if (!GetQValue(is, &qValue, error)) {
        return false;
      }
    } else {
      if (!SkipValue(is, error)) {
        return false;
      }
    }
  }

  return SkipChar(is, ']', error);
}

void
SdpImageattrAttributeList::Set::Serialize(std::ostream& os) const
{
  os << "[x=";
  xRange.Serialize(os);
  os << ",y=";
  yRange.Serialize(os);
  if (sRange.IsSet()) {
    os << ",sar=";
    sRange.Serialize(os);
  }
  if (pRange.IsSet()) {
    os << ",par=";
    pRange.Serialize(os);
  }
  if (qValue >= 0) {
    os << std::setprecision(2) << std::fixed << ",q=" << qValue;
  }
  os << "]";
}

static std::string
GetLowercaseToken(std::istream& is, std::string* error)
{
  is >> std::ws;
  std::string token;
  while (true) {
    switch (PeekChar(is, error)) {
      case '\0':
      case ' ':
      case '\t':
        return token;
      default:
        token.push_back(std::tolower(is.get()));
    }
  }

  MOZ_ASSERT_UNREACHABLE("Unexpected break in loop");
  return "";
}

bool
SdpImageattrAttributeList::Imageattr::ParseSets(std::istream& is,
                                                std::string* error)
{
  std::string type = GetLowercaseToken(is, error);

  bool* isAll = nullptr;
  std::vector<Set>* sets = nullptr;

  if (type == "send") {
    isAll = &sendAll;
    sets = &sendSets;
  } else if (type == "recv") {
    isAll = &recvAll;
    sets = &recvSets;
  } else {
    *error = "Unknown type, must be either send or recv";
    return false;
  }

  if (*isAll || !sets->empty()) {
    *error = "Multiple send or recv set lists";
    return false;
  }

  is >> std::ws;
  if (SkipChar(is, '*', error)) {
    *isAll = true;
    return true;
  }

  do {
    Set set;
    if (!set.Parse(is, error)) {
      return false;
    }

    sets->push_back(set);
    is >> std::ws;
  } while (PeekChar(is, error) == '[');

  return true;
}

bool
SdpImageattrAttributeList::Imageattr::Parse(std::istream& is,
                                            std::string* error)
{
  if (!SkipChar(is, '*', error)) {
    uint16_t value;
    if (!GetUnsigned<uint16_t>(is, 0, UINT16_MAX, &value, error)) {
      return false;
    }
    pt = Some(value);
  }

  is >> std::ws;
  if (!ParseSets(is, error)) {
    return false;
  }

  // There might be a second one
  is >> std::ws;
  if (is.eof()) {
    return true;
  }

  if (!ParseSets(is, error)) {
    return false;
  }

  is >> std::ws;
  if (!is.eof()) {
    *error = "Trailing characters";
    return false;
  }

  return true;
}

void
SdpImageattrAttributeList::Imageattr::Serialize(std::ostream& os) const
{
  if (pt.isSome()) {
    os << *pt;
  } else {
    os << "*";
  }

  if (sendAll) {
    os << " send *";
  } else if (!sendSets.empty()) {
    os << " send";
    for (auto& set : sendSets) {
      os << " ";
      set.Serialize(os);
    }
  }

  if (recvAll) {
    os << " recv *";
  } else if (!recvSets.empty()) {
    os << " recv";
    for (auto& set : recvSets) {
      os << " ";
      set.Serialize(os);
    }
  }
}

void
SdpImageattrAttributeList::Serialize(std::ostream& os) const
{
  for (auto& imageattr : mImageattrs) {
    os << "a=" << mType << ":";
    imageattr.Serialize(os);
    os << CRLF;
  }
}

bool
SdpImageattrAttributeList::PushEntry(const std::string& raw,
                                     std::string* error,
                                     size_t* errorPos)
{
  std::istringstream is(raw);

  Imageattr imageattr;
  if (!imageattr.Parse(is, error)) {
    is.clear();
    *errorPos = is.tellg();
    return false;
  }

  mImageattrs.push_back(imageattr);
  return true;
}

void
SdpMsidAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mMsids.begin(); i != mMsids.end(); ++i) {
    os << "a=" << mType << ":" << i->identifier;
    if (i->appdata.length()) {
      os << " " << i->appdata;
    }
    os << CRLF;
  }
}

void
SdpMsidSemanticAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mMsidSemantics.begin(); i != mMsidSemantics.end(); ++i) {
    os << "a=" << mType << ":" << i->semantic;
    for (auto j = i->msids.begin(); j != i->msids.end(); ++j) {
      os << " " << *j;
    }
    os << CRLF;
  }
}

void
SdpRemoteCandidatesAttribute::Serialize(std::ostream& os) const
{
  if (mCandidates.empty()) {
    return;
  }

  os << "a=" << mType;
  for (auto i = mCandidates.begin(); i != mCandidates.end(); i++) {
    os << (i == mCandidates.begin() ? ":" : " ") << i->id << " " << i->address
       << " " << i->port;
  }
  os << CRLF;
}

void
SdpRtcpAttribute::Serialize(std::ostream& os) const
{
  os << "a=" << mType << ":" << mPort;
  if (!mAddress.empty()) {
    os << " " << mNetType << " " << mAddrType << " " << mAddress;
  }
  os << CRLF;
}

const char* SdpRtcpFbAttributeList::pli = "pli";
const char* SdpRtcpFbAttributeList::sli = "sli";
const char* SdpRtcpFbAttributeList::rpsi = "rpsi";
const char* SdpRtcpFbAttributeList::app = "app";

const char* SdpRtcpFbAttributeList::fir = "fir";
const char* SdpRtcpFbAttributeList::tmmbr = "tmmbr";
const char* SdpRtcpFbAttributeList::tstr = "tstr";
const char* SdpRtcpFbAttributeList::vbcm = "vbcm";

void
SdpRtcpFbAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mFeedbacks.begin(); i != mFeedbacks.end(); ++i) {
    os << "a=" << mType << ":" << i->pt << " " << i->type;
    if (i->parameter.length()) {
      os << " " << i->parameter;
      if (i->extra.length()) {
        os << " " << i->extra;
      }
    }
    os << CRLF;
  }
}

static bool
ShouldSerializeChannels(SdpRtpmapAttributeList::CodecType type)
{
  switch (type) {
    case SdpRtpmapAttributeList::kOpus:
    case SdpRtpmapAttributeList::kG722:
      return true;
    case SdpRtpmapAttributeList::kPCMU:
    case SdpRtpmapAttributeList::kPCMA:
    case SdpRtpmapAttributeList::kVP8:
    case SdpRtpmapAttributeList::kVP9:
    case SdpRtpmapAttributeList::kiLBC:
    case SdpRtpmapAttributeList::kiSAC:
    case SdpRtpmapAttributeList::kH264:
      return false;
    case SdpRtpmapAttributeList::kOtherCodec:
      return true;
  }
  MOZ_CRASH();
}

void
SdpRtpmapAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mRtpmaps.begin(); i != mRtpmaps.end(); ++i) {
    os << "a=" << mType << ":" << i->pt << " " << i->name << "/" << i->clock;
    if (i->channels && ShouldSerializeChannels(i->codec)) {
      os << "/" << i->channels;
    }
    os << CRLF;
  }
}

void
SdpSctpmapAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mSctpmaps.begin(); i != mSctpmaps.end(); ++i) {
    os << "a=" << mType << ":" << i->pt << " " << i->name;
    if (i->streams) {
      os << " " << i->streams;
    }
    os << CRLF;
  }
}

void
SdpSetupAttribute::Serialize(std::ostream& os) const
{
  os << "a=" << mType << ":" << mRole << CRLF;
}

void
SdpSimulcastAttribute::Version::Serialize(std::ostream& os) const
{
  bool first = true;
  for (uint16_t format : choices) {
    if (first) {
      first = false;
    } else {
      os << ",";
    }

    os << format;
  }
}

bool
SdpSimulcastAttribute::Version::Parse(std::istream& is, std::string* error)
{
  do {
    uint16_t value;
    if (!GetUnsigned<uint16_t>(is, 0, UINT16_MAX, &value, error)) {
      return false;
    }
    choices.push_back(value);
  } while (SkipChar(is, ',', error));

  return true;
}

void
SdpSimulcastAttribute::Version::AppendAsStrings(
    std::vector<std::string>* formats) const
{
  for (uint16_t pt : choices) {
    std::ostringstream os;
    os << pt;
    formats->push_back(os.str());
  }
}

void
SdpSimulcastAttribute::Version::AddChoice(const std::string& pt)
{
  uint16_t ptAsInt;
  if (!SdpHelper::GetPtAsInt(pt, &ptAsInt)) {
    MOZ_ASSERT(false);
    return;
  }

  choices.push_back(ptAsInt);
}

void
SdpSimulcastAttribute::Versions::Serialize(std::ostream& os) const
{
  bool first = true;
  for (const Version& version : *this) {
    if (!version.IsSet()) {
      continue;
    }

    if (first) {
      first = false;
    } else {
      os << ";";
    }

    version.Serialize(os);
  }
}

bool
SdpSimulcastAttribute::Versions::Parse(std::istream& is, std::string* error)
{
  do {
    Version version;
    if (!version.Parse(is, error)) {
      return false;
    }
    push_back(version);
  } while(SkipChar(is, ';', error));

  return true;
}

void
SdpSimulcastAttribute::Serialize(std::ostream& os) const
{
  MOZ_ASSERT(sendVersions.IsSet() ||
             recvVersions.IsSet() ||
             sendrecvVersions.IsSet());

  os << "a=" << mType << ":";

  if (sendVersions.IsSet()) {
    os << " send ";
    sendVersions.Serialize(os);
  }

  if (recvVersions.IsSet()) {
    os << " recv ";
    recvVersions.Serialize(os);
  }

  if (sendrecvVersions.IsSet()) {
    os << " sendrecv ";
    sendrecvVersions.Serialize(os);
  }

  os << CRLF;
}

bool
SdpSimulcastAttribute::Parse(std::istream& is, std::string* error)
{
  bool gotRecv = false;
  bool gotSend = false;
  bool gotSendrecv = false;

  while (true) {
    std::string token = GetLowercaseToken(is, error);
    if (token.empty()) {
      break;
    }

    if (token == "send") {
      if (gotSend) {
        *error = "Already got a send list";
        return false;
      }
      gotSend = true;

      is >> std::ws;
      if (!sendVersions.Parse(is, error)) {
        return false;
      }
    } else if (token == "recv") {
      if (gotRecv) {
        *error = "Already got a recv list";
        return false;
      }
      gotRecv = true;

      is >> std::ws;
      if (!recvVersions.Parse(is, error)) {
        return false;
      }
    } else if (token == "sendrecv") {
      if (gotSendrecv) {
        *error = "Already got a sendrecv list";
        return false;
      }
      gotSendrecv = true;

      is >> std::ws;
      if (!sendrecvVersions.Parse(is, error)) {
        return false;
      }
    } else {
      *error = "Type must be either 'send', 'recv', or 'sendrecv'";
      return false;
    }
  }

  if (!gotSend && !gotRecv && !gotSendrecv) {
    *error = "Empty simulcast attribute";
    return false;
  }

  return true;
}

void
SdpSsrcAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mSsrcs.begin(); i != mSsrcs.end(); ++i) {
    os << "a=" << mType << ":" << i->ssrc << " " << i->attribute << CRLF;
  }
}

void
SdpSsrcGroupAttributeList::Serialize(std::ostream& os) const
{
  for (auto i = mSsrcGroups.begin(); i != mSsrcGroups.end(); ++i) {
    os << "a=" << mType << ":" << i->semantics;
    for (auto j = i->ssrcs.begin(); j != i->ssrcs.end(); ++j) {
      os << " " << (*j);
    }
    os << CRLF;
  }
}

void
SdpMultiStringAttribute::Serialize(std::ostream& os) const
{
  for (auto i = mValues.begin(); i != mValues.end(); ++i) {
    os << "a=" << mType << ":" << *i << CRLF;
  }
}

void
SdpOptionsAttribute::Serialize(std::ostream& os) const
{
  if (mValues.empty()) {
    return;
  }

  os << "a=" << mType << ":";

  for (auto i = mValues.begin(); i != mValues.end(); ++i) {
    if (i != mValues.begin()) {
      os << " ";
    }
    os << *i;
  }
  os << CRLF;
}

void
SdpOptionsAttribute::Load(const std::string& value)
{
  size_t start = 0;
  size_t end = value.find(' ');
  while (end != std::string::npos) {
    PushEntry(value.substr(start, end));
    start = end + 1;
    end = value.find(' ', start);
  }
  PushEntry(value.substr(start));
}

void
SdpFlagAttribute::Serialize(std::ostream& os) const
{
  os << "a=" << mType << CRLF;
}

void
SdpStringAttribute::Serialize(std::ostream& os) const
{
  os << "a=" << mType << ":" << mValue << CRLF;
}

void
SdpNumberAttribute::Serialize(std::ostream& os) const
{
  os << "a=" << mType << ":" << mValue << CRLF;
}

bool
SdpAttribute::IsAllowedAtMediaLevel(AttributeType type)
{
  switch (type) {
    case kBundleOnlyAttribute:
      return true;
    case kCandidateAttribute:
      return true;
    case kConnectionAttribute:
      return true;
    case kDirectionAttribute:
      return true;
    case kEndOfCandidatesAttribute:
      return true;
    case kExtmapAttribute:
      return true;
    case kFingerprintAttribute:
      return true;
    case kFmtpAttribute:
      return true;
    case kGroupAttribute:
      return false;
    case kIceLiteAttribute:
      return false;
    case kIceMismatchAttribute:
      return true;
    // RFC 5245 says this is session-level only, but
    // draft-ietf-mmusic-ice-sip-sdp-03 updates this to allow at the media
    // level.
    case kIceOptionsAttribute:
      return true;
    case kIcePwdAttribute:
      return true;
    case kIceUfragAttribute:
      return true;
    case kIdentityAttribute:
      return false;
    case kImageattrAttribute:
      return true;
    case kInactiveAttribute:
      return true;
    case kLabelAttribute:
      return true;
    case kMaxptimeAttribute:
      return true;
    case kMidAttribute:
      return true;
    case kMsidAttribute:
      return true;
    case kMsidSemanticAttribute:
      return false;
    case kPtimeAttribute:
      return true;
    case kRecvonlyAttribute:
      return true;
    case kRemoteCandidatesAttribute:
      return true;
    case kRtcpAttribute:
      return true;
    case kRtcpFbAttribute:
      return true;
    case kRtcpMuxAttribute:
      return true;
    case kRtcpRsizeAttribute:
      return true;
    case kRtpmapAttribute:
      return true;
    case kSctpmapAttribute:
      return true;
    case kSendonlyAttribute:
      return true;
    case kSendrecvAttribute:
      return true;
    case kSetupAttribute:
      return true;
    case kSimulcastAttribute:
      return true;
    case kSsrcAttribute:
      return true;
    case kSsrcGroupAttribute:
      return true;
  }
  MOZ_CRASH("Unknown attribute type");
}

bool
SdpAttribute::IsAllowedAtSessionLevel(AttributeType type)
{
  switch (type) {
    case kBundleOnlyAttribute:
      return false;
    case kCandidateAttribute:
      return false;
    case kConnectionAttribute:
      return true;
    case kDirectionAttribute:
      return true;
    case kEndOfCandidatesAttribute:
      return true;
    case kExtmapAttribute:
      return true;
    case kFingerprintAttribute:
      return true;
    case kFmtpAttribute:
      return false;
    case kGroupAttribute:
      return true;
    case kIceLiteAttribute:
      return true;
    case kIceMismatchAttribute:
      return false;
    case kIceOptionsAttribute:
      return true;
    case kIcePwdAttribute:
      return true;
    case kIceUfragAttribute:
      return true;
    case kIdentityAttribute:
      return true;
    case kImageattrAttribute:
      return false;
    case kInactiveAttribute:
      return true;
    case kLabelAttribute:
      return false;
    case kMaxptimeAttribute:
      return false;
    case kMidAttribute:
      return false;
    case kMsidSemanticAttribute:
      return true;
    case kMsidAttribute:
      return false;
    case kPtimeAttribute:
      return false;
    case kRecvonlyAttribute:
      return true;
    case kRemoteCandidatesAttribute:
      return false;
    case kRtcpAttribute:
      return false;
    case kRtcpFbAttribute:
      return false;
    case kRtcpMuxAttribute:
      return false;
    case kRtcpRsizeAttribute:
      return false;
    case kRtpmapAttribute:
      return false;
    case kSctpmapAttribute:
      return false;
    case kSendonlyAttribute:
      return true;
    case kSendrecvAttribute:
      return true;
    case kSetupAttribute:
      return true;
    case kSimulcastAttribute:
      return false;
    case kSsrcAttribute:
      return false;
    case kSsrcGroupAttribute:
      return false;
  }
  MOZ_CRASH("Unknown attribute type");
}

const std::string
SdpAttribute::GetAttributeTypeString(AttributeType type)
{
  switch (type) {
    case kBundleOnlyAttribute:
      return "bundle-only";
    case kCandidateAttribute:
      return "candidate";
    case kConnectionAttribute:
      return "connection";
    case kEndOfCandidatesAttribute:
      return "end-of-candidates";
    case kExtmapAttribute:
      return "extmap";
    case kFingerprintAttribute:
      return "fingerprint";
    case kFmtpAttribute:
      return "fmtp";
    case kGroupAttribute:
      return "group";
    case kIceLiteAttribute:
      return "ice-lite";
    case kIceMismatchAttribute:
      return "ice-mismatch";
    case kIceOptionsAttribute:
      return "ice-options";
    case kIcePwdAttribute:
      return "ice-pwd";
    case kIceUfragAttribute:
      return "ice-ufrag";
    case kIdentityAttribute:
      return "identity";
    case kImageattrAttribute:
      return "imageattr";
    case kInactiveAttribute:
      return "inactive";
    case kLabelAttribute:
      return "label";
    case kMaxptimeAttribute:
      return "maxptime";
    case kMidAttribute:
      return "mid";
    case kMsidAttribute:
      return "msid";
    case kMsidSemanticAttribute:
      return "msid-semantic";
    case kPtimeAttribute:
      return "ptime";
    case kRecvonlyAttribute:
      return "recvonly";
    case kRemoteCandidatesAttribute:
      return "remote-candidates";
    case kRtcpAttribute:
      return "rtcp";
    case kRtcpFbAttribute:
      return "rtcp-fb";
    case kRtcpMuxAttribute:
      return "rtcp-mux";
    case kRtcpRsizeAttribute:
      return "rtcp-rsize";
    case kRtpmapAttribute:
      return "rtpmap";
    case kSctpmapAttribute:
      return "sctpmap";
    case kSendonlyAttribute:
      return "sendonly";
    case kSendrecvAttribute:
      return "sendrecv";
    case kSetupAttribute:
      return "setup";
    case kSimulcastAttribute:
      return "simulcast";
    case kSsrcAttribute:
      return "ssrc";
    case kSsrcGroupAttribute:
      return "ssrc-group";
    case kDirectionAttribute:
      MOZ_CRASH("kDirectionAttribute not valid here");
  }
  MOZ_CRASH("Unknown attribute type");
}

} // namespace mozilla
