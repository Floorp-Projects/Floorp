/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "signaling/src/sdp/HybridSdpParser.h"
#include "signaling/src/sdp/SdpLog.h"
#include "signaling/src/sdp/SipccSdpParser.h"
#include "signaling/src/sdp/RsdparsaSdpParser.h"
#include "signaling/src/sdp/ParsingResultComparer.h"

#include "mozilla/Logging.h"
#include "mozilla/Preferences.h"

#include <unordered_map>

namespace mozilla {

using mozilla::LogLevel;

// Interprets about:config SDP parsing preferences
class SdpPref {
 private:
  static const std::string PRIMARY_PREF;
  static const std::string ALTERNATE_PREF;
  static const std::string DEFAULT;

  // Supported Parsers
  enum class Parsers {
    Sipcc,
    WebRtcSdp,
  };

  // How is the alternate used
  enum class AlternateParseModes {
    Parallel,  // Alternate is always run, if A succedes it is used, otherwise B
               // is used
    Failover,  // Alternate is only run on failure of the primary to parse
    Never,     // Alternate is never run; this is effectively a kill switch
  };

  // Finds the mapping between a pref string and pref value, if none exists the
  // default is used
  template <class T>
  static auto Pref(const std::string& aPrefName,
                   const std::unordered_map<std::string, T>& aMap) -> T {
    MOZ_ASSERT(aMap.find(DEFAULT) != aMap.end());

    nsCString value;
    if (NS_FAILED(Preferences::GetCString(aPrefName.c_str(), value))) {
      return aMap.at(DEFAULT);
    }
    const auto found = aMap.find(value.get());
    if (found != aMap.end()) {
      return found->second;
    }
    return aMap.at(DEFAULT);
  }

  // The value of the parser pref
  static auto Parser() -> Parsers {
    static const auto values = std::unordered_map<std::string, Parsers>{
        {"legacy", Parsers::Sipcc},
        {"webrtc-sdp", Parsers::WebRtcSdp},
        {DEFAULT, Parsers::Sipcc},
    };
    return Pref(PRIMARY_PREF, values);
  }

  // The value of the alternate parse mode pref
  static auto AlternateParseMode() -> AlternateParseModes {
    static const auto values =
        std::unordered_map<std::string, AlternateParseModes>{
            {"parallel", AlternateParseModes::Parallel},
            {"failover", AlternateParseModes::Failover},
            {"never", AlternateParseModes::Never},
            {DEFAULT, AlternateParseModes::Parallel},
        };
    return Pref(ALTERNATE_PREF, values);
  }

 public:
  // Functions to get the primary, secondary and failover parsers.
  // These exist as they do so that the coresponding fields in HybridSdpParser
  // can be const initialized.

  // Reads about:config to choose the primary Parser
  static auto Primary() -> UniquePtr<SdpParser> {
    switch (Parser()) {
      case Parsers::Sipcc:
        return UniquePtr<SdpParser>(new SipccSdpParser());
      case Parsers::WebRtcSdp:
        return UniquePtr<SdpParser>(new RsdparsaSdpParser());
    }
  }

  static auto Secondary() -> Maybe<UniquePtr<SdpParser>> {
    if (AlternateParseMode() != AlternateParseModes::Parallel) {
      return Nothing();
    }
    switch (Parser()) {  // Choose whatever the primary parser isn't
      case Parsers::Sipcc:
        return Some(UniquePtr<SdpParser>(new RsdparsaSdpParser()));
      case Parsers::WebRtcSdp:
        return Some(UniquePtr<SdpParser>(new SipccSdpParser()));
    }
  }

  static auto Failover() -> Maybe<UniquePtr<SdpParser>> {
    if (AlternateParseMode() != AlternateParseModes::Failover) {
      return Nothing();
    }
    switch (Parser()) {
      case Parsers::Sipcc:
        return Some(UniquePtr<SdpParser>(new RsdparsaSdpParser()));
      case Parsers::WebRtcSdp:
        return Some(UniquePtr<SdpParser>(new SipccSdpParser()));
    }
  }
};

const std::string SdpPref::PRIMARY_PREF = "media.peerconnection.sdp.parser";
const std::string SdpPref::ALTERNATE_PREF =
    "media.peerconnection.sdp.alternate_parse_mode";
const std::string SdpPref::DEFAULT = "default";

HybridSdpParser::HybridSdpParser()
    : mPrimary(SdpPref::Primary()),
      mSecondary(SdpPref::Secondary()),
      mFailover(SdpPref::Failover()) {
  MOZ_ASSERT(!(mSecondary && mFailover),
             "Can not have both a secondary and failover parser!");
  MOZ_LOG(SdpLog, LogLevel::Info,
          ("Primary SDP Parser: %s", mPrimary->Name().c_str()));
  mSecondary.apply([](auto& parser) {
    MOZ_LOG(SdpLog, LogLevel::Info,
            ("Secondary SDP Logger: %s", parser->Name().c_str()));
  });
  mFailover.apply([](auto& parser) {
    MOZ_LOG(SdpLog, LogLevel::Info,
            ("Failover SDP Logger: %s", parser->Name().c_str()));
  });
}

auto HybridSdpParser::Parse(const std::string& aText)
    -> UniquePtr<SdpParser::Results> {
  using Results = UniquePtr<SdpParser::Results>;
  auto results = mPrimary->Parse(aText);
  // Pass results on for comparison and return A if it was a success and B
  // otherwise.
  auto compare = [&results, &aText](Results&& aResB) -> Results {
    ParsingResultComparer::Compare(results, aResB, aText);
    return std::move(results->Ok() ? results : aResB);
  };
  // Run secondary parser, if there is one, and update selected results.
  mSecondary.apply(
      [&](auto& sec) { results = compare(std::move(sec->Parse(aText))); });
  // Run failover parser, if there is one, and update selected results.
  mFailover.apply([&](auto& failover) {  // Only run if primary parser failed
    if (!results->Ok()) {
      results = compare(std::move(failover->Parse(aText)));
    }
  });
  return results;
}

const std::string HybridSdpParser::PARSER_NAME = "hybrid";

}  // namespace mozilla
