/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Policy.h"

#include <windows.h>
#include <shlwapi.h>
#include <fstream>

#include "common.h"
#include "Registry.h"

#include "json/json.h"
#include "mozilla/HelperMacros.h"
#include "mozilla/Maybe.h"
#include "mozilla/WinHeaderOnlyUtils.h"

// There is little logging or error handling in this file, because the file and
// registry values we are reading here are normally absent, so never finding
// anything that we look for at all would not be an error worth generating an
// event log for.

#define AGENT_POLICY_NAME "DisableDefaultBrowserAgent"
#define TELEMETRY_POLICY_NAME "DisableTelemetry"

// The Firefox policy engine hardcodes the string "Mozilla" in its registry
// key accesses rather than using the configured vendor name, so we should do
// the same here to be sure we're compatible with it.
#define POLICY_REGKEY_NAME L"SOFTWARE\\Policies\\Mozilla\\" MOZ_APP_BASENAME

// This enum is the return type for the functions that check policy values.
enum class PolicyState {
  Enabled,   // There is a policy explicitly set to enabled
  Disabled,  // There is a policy explicitly set to disabled
  NoPolicy,  // This policy isn't configured
};

static PolicyState FindPolicyInRegistry(HKEY rootKey,
                                        const wchar_t* policyName) {
  HKEY rawRegKey = nullptr;
  RegOpenKeyExW(rootKey, POLICY_REGKEY_NAME, 0, KEY_READ, &rawRegKey);

  nsAutoRegKey regKey(rawRegKey);

  if (!regKey) {
    return PolicyState::NoPolicy;
  }

  // If this key is empty and doesn't have any actual policies in it,
  // treat that the same as the key not existing and return no result.
  DWORD numSubKeys = 0, numValues = 0;
  LSTATUS ls = RegQueryInfoKeyW(regKey.get(), nullptr, nullptr, nullptr,
                                &numSubKeys, nullptr, nullptr, &numValues,
                                nullptr, nullptr, nullptr, nullptr);
  if (ls != ERROR_SUCCESS) {
    return PolicyState::NoPolicy;
  }

  DWORD policyValue = UINT32_MAX;
  DWORD policyValueSize = sizeof(policyValue);
  ls = RegGetValueW(regKey.get(), nullptr, policyName, RRF_RT_REG_DWORD,
                    nullptr, &policyValue, &policyValueSize);

  if (ls != ERROR_SUCCESS) {
    return PolicyState::NoPolicy;
  }
  return policyValue == 0 ? PolicyState::Disabled : PolicyState::Enabled;
}

static PolicyState FindPolicyInFile(const char* policyName) {
  mozilla::UniquePtr<wchar_t[]> thisBinaryPath = mozilla::GetFullBinaryPath();
  if (!PathRemoveFileSpecW(thisBinaryPath.get())) {
    return PolicyState::NoPolicy;
  }

  wchar_t policiesFilePath[MAX_PATH] = L"";
  if (!PathCombineW(policiesFilePath, thisBinaryPath.get(), L"distribution")) {
    return PolicyState::NoPolicy;
  }

  if (!PathAppendW(policiesFilePath, L"policies.json")) {
    return PolicyState::NoPolicy;
  }

  // We need a narrow string-based std::ifstream because that's all jsoncpp can
  // use; that means we need to supply it the file path as a narrow string.
  int policiesFilePathLen = WideCharToMultiByte(
      CP_UTF8, 0, policiesFilePath, -1, nullptr, 0, nullptr, nullptr);
  if (policiesFilePathLen == 0) {
    return PolicyState::NoPolicy;
  }
  mozilla::UniquePtr<char[]> policiesFilePathA =
      mozilla::MakeUnique<char[]>(policiesFilePathLen);
  policiesFilePathLen = WideCharToMultiByte(
      CP_UTF8, 0, policiesFilePath, -1, policiesFilePathA.get(),
      policiesFilePathLen, nullptr, nullptr);
  if (policiesFilePathLen == 0) {
    return PolicyState::NoPolicy;
  }

  Json::Value jsonRoot;
  std::ifstream stream(policiesFilePathA.get());
  Json::Reader().parse(stream, jsonRoot);

  if (jsonRoot.isObject() && jsonRoot.isMember("Policies") &&
      jsonRoot["Policies"].isObject()) {
    if (jsonRoot["Policies"].isMember(policyName) &&
        jsonRoot["Policies"][policyName].isBool()) {
      return jsonRoot["Policies"][policyName].asBool() ? PolicyState::Enabled
                                                       : PolicyState::Disabled;
    } else {
      return PolicyState::NoPolicy;
    }
  }

  return PolicyState::NoPolicy;
}

static PolicyState IsDisabledByPref(const wchar_t* prefRegValue) {
  auto prefValueResult =
      RegistryGetValueBool(IsPrefixed::Prefixed, prefRegValue);

  if (prefValueResult.isErr()) {
    return PolicyState::NoPolicy;
  }
  auto prefValue = prefValueResult.unwrap();
  if (prefValue.isNothing()) {
    return PolicyState::NoPolicy;
  }
  return prefValue.value() ? PolicyState::Enabled : PolicyState::Disabled;
}

// Everything we call from this function wants wide strings, except for jsoncpp,
// which cannot work with them at all, so at some point we need both formats.
// It's awkward to take both formats as individual arguments, but it would be
// more awkward to take one and runtime convert it to the other, or to turn
// this function into a macro so that the preprocessor can trigger the
// conversion for us, so this is what we've got.
static bool IsThingDisabled(const char* thing, const wchar_t* wideThing) {
  // The logic here is intended to be the same as that used by Firefox's policy
  // engine implementation; they should be kept in sync. We have added the pref
  // check at the end though, since that's our own custom mechanism.
  PolicyState state = FindPolicyInRegistry(HKEY_LOCAL_MACHINE, wideThing);
  if (state == PolicyState::NoPolicy) {
    state = FindPolicyInRegistry(HKEY_CURRENT_USER, wideThing);
  }
  if (state == PolicyState::NoPolicy) {
    state = FindPolicyInFile(thing);
  }
  if (state == PolicyState::NoPolicy) {
    state = IsDisabledByPref(wideThing);
  }
  return state == PolicyState::Enabled ? true : false;
}

bool IsAgentDisabled() {
  return IsThingDisabled(AGENT_POLICY_NAME, L"" AGENT_POLICY_NAME);
}

bool IsTelemetryDisabled() {
  return IsThingDisabled(TELEMETRY_POLICY_NAME, L"" TELEMETRY_POLICY_NAME);
}
