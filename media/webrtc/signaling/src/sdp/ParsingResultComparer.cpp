/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "signaling/src/sdp/Sdp.h"
#include "signaling/src/sdp/ParsingResultComparer.h"

#include <string>
#include <ostream>
#include <regex>

#include "mozilla/Assertions.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Logging.h"

using mozilla::LogLevel;
static mozilla::LazyLogModule sSdpDiffLogger("sdpdiff_logger");

#define LOGD(msg) MOZ_LOG(sSdpDiffLogger, LogLevel::Debug, msg)


namespace mozilla
{

using AttributeType = SdpAttribute::AttributeType;

template<typename T>
std::string ToString(const T& serializable)
{
  std::ostringstream os;

  os << serializable;
  return os.str();
}

bool
ParsingResultComparer::Compare(const Sdp& rsdparsaSdp, const Sdp& sipccSdp,
                               const std::string& originalSdp)
{
  bool result = true;
  mOriginalSdp = originalSdp;

  LOGD(("The original sdp: \n%s", mOriginalSdp.c_str()));

  const std::string sipccSdpStr = sipccSdp.ToString();
  const std::string rsdparsaSdpStr = rsdparsaSdp.ToString();

  if (rsdparsaSdpStr == sipccSdpStr) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                         NS_LITERAL_STRING("serialization_is_equal"), 1);
    LOGD(("Serialization is equal"));
    return true;
  }

  Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                       NS_LITERAL_STRING("serialization_is_not_equal"), 1);
  LOGD(("Serialization is not equal\n"
        " --- Sipcc SDP ---\n"
        "%s\n"
        "--- Rsdparsa SDP ---\n"
        "%s\n",
        sipccSdpStr.c_str(), rsdparsaSdpStr.c_str()));

  const std::string rsdparsaOriginStr = ToString(rsdparsaSdp.GetOrigin());
  const std::string sipccOriginStr = ToString(sipccSdp.GetOrigin());

  // Compare the session level
  if (rsdparsaOriginStr != sipccOriginStr) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                         NS_LITERAL_STRING("o="), 1);
    LOGD(("origin is not equal\nrust origin: %s\nsipcc origin: %s",
          rsdparsaOriginStr.c_str(),
          sipccOriginStr.c_str()));
    result = false;
  }

  if (MOZ_LOG_TEST(sSdpDiffLogger, LogLevel::Debug)) {
    const auto rust_sess_attr_count = rsdparsaSdp.GetAttributeList().Count();
    const auto sipcc_sess_attr_count = sipccSdp.GetAttributeList().Count();

    if (rust_sess_attr_count != sipcc_sess_attr_count) {
      LOGD(("Session level attribute count is NOT equal, rsdparsa: %u, "
            "sipcc: %u\n", rust_sess_attr_count, sipcc_sess_attr_count));
    }
  }

  result &= CompareAttrLists(rsdparsaSdp.GetAttributeList(),
                             sipccSdp.GetAttributeList(),
                             -1);

  const uint32_t sipccMediaSecCount = static_cast<uint32_t>(
                                            sipccSdp.GetMediaSectionCount());
  const uint32_t rsdparsaMediaSecCount = static_cast<uint32_t>(
                                            rsdparsaSdp.GetMediaSectionCount());

  if (sipccMediaSecCount != rsdparsaMediaSecCount) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                         NS_LITERAL_STRING("inequal_msec_count"), 1);
    LOGD(("Media section count is NOT equal, rsdparsa: %d, sipcc: %d \n",
          rsdparsaMediaSecCount, sipccMediaSecCount));
    result = false;
  }

  for (size_t i = 0; i < std::min(sipccMediaSecCount,
                                  rsdparsaMediaSecCount); i++) {
    result &= CompareMediaSections(rsdparsaSdp.GetMediaSection(i),
                                   sipccSdp.GetMediaSection(i));
  }

  return result;
}

bool
ParsingResultComparer::CompareMediaSections(const SdpMediaSection&
                                              rustMediaSection,
                                            const SdpMediaSection&
                                              sipccMediaSection) const
{
  bool result = true;
  auto trackMediaLineMismatch = [&result] (auto rustValue, auto sipccValue,
                                         const nsString& valueDescription) {
    nsString typeStr = NS_LITERAL_STRING("m=");
    typeStr += valueDescription;
    Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                         typeStr, 1);
    LOGD(("The media line values %s are not equal\n"
          "rsdparsa value: %s\n"
          "sipcc value: %s\n",
          NS_LossyConvertUTF16toASCII(valueDescription).get(),
          ToString(rustValue).c_str(),
          ToString(sipccValue).c_str()));
    result = false;
  };

  auto compareMediaLineValue = [trackMediaLineMismatch]
                                (auto rustValue, auto sipccValue,
                                const nsString& valueDescription) {
    if (rustValue != sipccValue) {
      trackMediaLineMismatch(rustValue, sipccValue, valueDescription);
    }
  };

  auto compareSimpleMediaLineValue = [&rustMediaSection, &sipccMediaSection,
                                      compareMediaLineValue]
                                     (auto valGetFuncPtr,
                                      const nsString& valueDescription) {
    compareMediaLineValue((rustMediaSection.*valGetFuncPtr)(),
                          (sipccMediaSection.*valGetFuncPtr)(),
                           valueDescription);
  };

  compareSimpleMediaLineValue(&SdpMediaSection::GetMediaType,
                              NS_LITERAL_STRING("media_type"));
  compareSimpleMediaLineValue(&SdpMediaSection::GetPort,
                              NS_LITERAL_STRING("port"));
  compareSimpleMediaLineValue(&SdpMediaSection::GetPortCount,
                              NS_LITERAL_STRING("port_count"));
  compareSimpleMediaLineValue(&SdpMediaSection::GetProtocol,
                              NS_LITERAL_STRING("protocol"));
  compareSimpleMediaLineValue(&SdpMediaSection::IsReceiving,
                              NS_LITERAL_STRING("is_receiving"));
  compareSimpleMediaLineValue(&SdpMediaSection::IsSending,
                              NS_LITERAL_STRING("is_sending"));
  compareSimpleMediaLineValue(&SdpMediaSection::GetDirection,
                              NS_LITERAL_STRING("direction"));
  compareSimpleMediaLineValue(&SdpMediaSection::GetLevel,
                              NS_LITERAL_STRING("level"));

  compareMediaLineValue(ToString(rustMediaSection.GetConnection()),
                        ToString(sipccMediaSection.GetConnection()),
                        NS_LITERAL_STRING("connection"));

  result &= CompareAttrLists(rustMediaSection.GetAttributeList(),
                             sipccMediaSection.GetAttributeList(),
                             static_cast<int>(rustMediaSection.GetLevel()));
  return result;
}

bool
ParsingResultComparer::CompareAttrLists(const SdpAttributeList& rustAttrlist,
                                        const SdpAttributeList& sipccAttrlist,
                                        int level) const
{
  bool result = true;

  for (size_t i = AttributeType::kFirstAttribute; i <=
       static_cast<size_t>(AttributeType::kLastAttribute); i++) {
    const AttributeType type = static_cast<AttributeType>(i);
    std::string attrStr;
    if (type != AttributeType::kDirectionAttribute) {
      attrStr = "a=" + SdpAttribute::GetAttributeTypeString(type);
    } else {
      attrStr = "a=_direction_attribute_";
    }

    if (sipccAttrlist.HasAttribute(type, false)) {
      auto sipccAttrStr = ToString(*sipccAttrlist.GetAttribute(type, false));

      if (!rustAttrlist.HasAttribute(type, false)) {
        nsString typeStr;
        typeStr.AssignASCII(attrStr.c_str());
        typeStr += NS_LITERAL_STRING("_missing");
        Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                             typeStr, 1);
        LOGD(("Rust is missing the attribute: %s\n", attrStr.c_str()));
        LOGD(("Rust is missing: %s\n",sipccAttrStr.c_str()));

        result = false;
        continue;
      }

      auto rustAttrStr = ToString(*rustAttrlist.GetAttribute(type, false));

      if (rustAttrStr != sipccAttrStr) {

        if (type == AttributeType::kFmtpAttribute) {
          if (rustAttrlist.GetFmtp() == sipccAttrlist.GetFmtp()) {
            continue;
          }
        }

        std::string originalAttrStr = GetAttributeLines(attrStr, level);
        if (rustAttrStr != originalAttrStr) {
          nsString typeStr;
          typeStr.AssignASCII(attrStr.c_str());
          typeStr += NS_LITERAL_STRING("_inequal");
          Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                               typeStr, 1);
          LOGD(("%s is neither equal to sipcc nor to the orginal sdp\n"
                "--------------rsdparsa attribute---------------\n"
                "%s"
                "--------------sipcc attribute---------------\n"
                "%s"
                "--------------original attribute---------------\n"
                "%s\n",
                attrStr.c_str(), rustAttrStr.c_str(), sipccAttrStr.c_str(),
                originalAttrStr.c_str()));
          result = false;
        } else {
          LOGD(("But the rust serialization is equal to the orignal sdp\n"));
        }
      }
    } else {
      if (rustAttrlist.HasAttribute(type, false)) {
        nsString typeStr;
        typeStr.AssignASCII(attrStr.c_str());
        typeStr += NS_LITERAL_STRING("_unexpected");
        Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                             typeStr, 1);
      }
    }
  }

  return result;
}

void
ParsingResultComparer::TrackRustParsingFailed(size_t sipccErrorCount) const {

  if (sipccErrorCount) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                         NS_LITERAL_STRING("rsdparsa_failed__sipcc_has_errors"),
                         1);
  } else {
    Telemetry::ScalarAdd(Telemetry::ScalarID::WEBRTC_SDP_PARSER_DIFF,
                         NS_LITERAL_STRING("rsdparsa_failed__sipcc_succeeded"),
                         1);
  }
}

std::vector<std::string>
SplitLines(const std::string& sdp)
{
    std::stringstream ss(sdp);
    std::string to;
    std::vector<std::string> lines;

    while(std::getline(ss, to, '\n')) {
      lines.push_back(to);
    }

    return lines;
}

std::string
ParsingResultComparer::GetAttributeLines(const std::string& attrType,
                                         int level) const
{
  std::vector<std::string> lines = SplitLines(mOriginalSdp);
  std::string attrToFind = attrType + ":";
  std::string attrLines;
  int currentLevel = -1;
  // Filters rtcp-fb lines that contain "x-..." types
  // This is because every SDP from Edge contains these rtcp-fb x- types
  // for example: a=rtcp-fb:121 x-foo
  std::regex customRtcpFbLines("a\\=rtcp\\-fb\\:(\\d+|\\*).* x\\-.*");

  for (auto line : lines) {

    if (line.find("m=") == 0) {
      if (level > currentLevel) {
        attrLines.clear();
        currentLevel++;
      } else {
        break;
      }
    } else if (line.find(attrToFind) == 0) {

      if (std::regex_match(line, customRtcpFbLines)) {
        continue;
      }

      attrLines += (line + '\n');
    }
  }

  return attrLines;
}

}
