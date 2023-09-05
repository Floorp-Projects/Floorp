/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_AGENT_REGISTRY_H__
#define __DEFAULT_BROWSER_AGENT_REGISTRY_H__

#include <cstdint>
#include <windows.h>

#include "mozilla/Maybe.h"
#include "mozilla/Result.h"
#include "mozilla/WinHeaderOnlyUtils.h"

namespace mozilla::default_agent {

// Indicates whether or not a registry value name is prefixed with the install
// directory path (Prefixed), or not (Unprefixed). Prefixing a registry value
// name with the install directory makes that value specific to this
// installation's default browser agent.
enum class IsPrefixed {
  Prefixed,
  Unprefixed,
};

// The result of an operation only, containing no other data on success.
using VoidResult = mozilla::WindowsErrorResult<mozilla::Ok>;

using MaybeString = mozilla::Maybe<std::string>;
using MaybeStringResult = mozilla::WindowsErrorResult<MaybeString>;
// Get a string from the registry. If necessary, value name prefixing will be
// performed automatically.
// Strings are stored as wide strings, but are converted to narrow UTF-8 before
// being returned.
MaybeStringResult RegistryGetValueString(IsPrefixed isPrefixed,
                                         const wchar_t* registryValueName,
                                         const wchar_t* subKey = nullptr);

// Set a string in the registry. If necessary, value name prefixing will be
// performed automatically.
// Strings are converted to wide strings for registry storage.
VoidResult RegistrySetValueString(IsPrefixed isPrefixed,
                                  const wchar_t* registryValueName,
                                  const char* newValue,
                                  const wchar_t* subKey = nullptr);

using MaybeBoolResult = mozilla::WindowsErrorResult<mozilla::Maybe<bool>>;
// Get a bool from the registry.
// Bools are stored as a single DWORD, with 0 meaning false and any other value
// meaning true.
MaybeBoolResult RegistryGetValueBool(IsPrefixed isPrefixed,
                                     const wchar_t* registryValueName,
                                     const wchar_t* subKey = nullptr);

// Set a bool in the registry. If necessary, value name prefixing will be
// performed automatically.
// Bools are stored as a single DWORD, with 0 meaning false and any other value
// meaning true.
VoidResult RegistrySetValueBool(IsPrefixed isPrefixed,
                                const wchar_t* registryValueName, bool newValue,
                                const wchar_t* subKey = nullptr);

using MaybeQwordResult = mozilla::WindowsErrorResult<mozilla::Maybe<ULONGLONG>>;
// Get a QWORD (ULONGLONG) from the registry. If necessary, value name prefixing
// will be performed automatically.
MaybeQwordResult RegistryGetValueQword(IsPrefixed isPrefixed,
                                       const wchar_t* registryValueName,
                                       const wchar_t* subKey = nullptr);

// Get a QWORD (ULONGLONG) in the registry. If necessary, value name prefixing
// will be performed automatically.
VoidResult RegistrySetValueQword(IsPrefixed isPrefixed,
                                 const wchar_t* registryValueName,
                                 ULONGLONG newValue,
                                 const wchar_t* subKey = nullptr);

using MaybeDword = mozilla::Maybe<uint32_t>;
using MaybeDwordResult = mozilla::WindowsErrorResult<MaybeDword>;
// Get a DWORD (uint32_t) from the registry. If necessary, value name prefixing
// will be performed automatically.
MaybeDwordResult RegistryGetValueDword(IsPrefixed isPrefixed,
                                       const wchar_t* registryValueName,
                                       const wchar_t* subKey = nullptr);

// Get a DWORD (uint32_t) in the registry. If necessary, value name prefixing
// will be performed automatically.
VoidResult RegistrySetValueDword(IsPrefixed isPrefixed,
                                 const wchar_t* registryValueName,
                                 uint32_t newValue,
                                 const wchar_t* subKey = nullptr);

VoidResult RegistryDeleteValue(IsPrefixed isPrefixed,
                               const wchar_t* registryValueName,
                               const wchar_t* subKey = nullptr);

}  // namespace mozilla::default_agent

#endif  // __DEFAULT_BROWSER_AGENT_REGISTRY_H__
