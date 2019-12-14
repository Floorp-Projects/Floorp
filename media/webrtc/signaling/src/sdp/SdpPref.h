/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDPPREF_H_
#define _SDPPREF_H_

#include "mozilla/Maybe.h"
#include "mozilla/Preferences.h"

#include <string>
#include <unordered_map>

namespace mozilla {

class SdpParser;

// Interprets about:config SDP parsing preferences
class SdpPref {
 private:
  static const std::string PRIMARY_PREF;
  static const std::string ALTERNATE_PREF;
  static const std::string STRICT_SUCCESS_PREF;
  static const std::string DEFAULT;

 public:
  // Supported Parsers
  enum class Parsers {
    Sipcc,
    WebRtcSdp,
  };
  static auto ToString(const Parsers& aParser) -> std::string;

  // How is the alternate used
  enum class AlternateParseModes {
    Parallel,  // Alternate is always run, if A succedes it is used, otherwise B
               // is used
    Failover,  // Alternate is only run on failure of the primary to parse
    Never,     // Alternate is never run; this is effectively a kill switch
  };
  static auto ToString(const AlternateParseModes& aMode) -> std::string;

 private:
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
  static auto Parser() -> Parsers;

  // The value of the alternate parse mode pref
  static auto AlternateParseMode() -> AlternateParseModes;

 public:
  // Do non-fatal parsing errors count as failure
  static auto StrictSuccess() -> bool;
  // Functions to create the primary, secondary and failover parsers.

  // Reads about:config to choose the primary Parser
  static auto Primary() -> UniquePtr<SdpParser>;
  static auto Secondary() -> Maybe<UniquePtr<SdpParser>>;
  static auto Failover() -> Maybe<UniquePtr<SdpParser>>;
};

}  // namespace mozilla

#endif
