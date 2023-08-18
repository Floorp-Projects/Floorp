/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_GeckoArgs_h
#define mozilla_GeckoArgs_h

#include "mozilla/CmdLineAndEnvUtils.h"
#include "mozilla/Maybe.h"

#include <array>
#include <cctype>
#include <climits>
#include <string>
#include <vector>

namespace mozilla {

namespace geckoargs {

template <typename T>
struct CommandLineArg {
  Maybe<T> Get(int& aArgc, char** aArgv,
               const CheckArgFlag aFlags = CheckArgFlag::RemoveArg);

  const char* Name() { return sName; };

  void Put(std::vector<std::string>& aArgs);
  void Put(T aValue, std::vector<std::string>& aArgs);

  const char* sName;
  const char* sMatch;
};

/// Get()

template <>
inline Maybe<const char*> CommandLineArg<const char*>::Get(
    int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  MOZ_ASSERT(aArgv, "aArgv must be initialized before CheckArg()");
  const char* rv = nullptr;
  if (ARG_FOUND == CheckArg(aArgc, aArgv, sMatch, &rv, aFlags)) {
    return Some(rv);
  }
  return Nothing();
}

template <>
inline Maybe<bool> CommandLineArg<bool>::Get(int& aArgc, char** aArgv,
                                             const CheckArgFlag aFlags) {
  MOZ_ASSERT(aArgv, "aArgv must be initialized before CheckArg()");
  if (ARG_FOUND ==
      CheckArg(aArgc, aArgv, sMatch, (const char**)nullptr, aFlags)) {
    return Some(true);
  }
  return Nothing();
}

template <>
inline Maybe<uint64_t> CommandLineArg<uint64_t>::Get(
    int& aArgc, char** aArgv, const CheckArgFlag aFlags) {
  MOZ_ASSERT(aArgv, "aArgv must be initialized before CheckArg()");
  const char* rv = nullptr;
  if (ARG_FOUND == CheckArg(aArgc, aArgv, sMatch, &rv, aFlags)) {
    errno = 0;
    char* endptr = nullptr;
    uint64_t conv = std::strtoull(rv, &endptr, 10);
    if (errno == 0 && endptr && *endptr == '\0') {
      return Some(conv);
    }
  }
  return Nothing();
}

/// Put()

template <>
inline void CommandLineArg<const char*>::Put(const char* aValue,
                                             std::vector<std::string>& aArgs) {
  aArgs.push_back(Name());
  aArgs.push_back(aValue);
}

template <>
inline void CommandLineArg<bool>::Put(bool aValue,
                                      std::vector<std::string>& aArgs) {
  if (aValue) {
    aArgs.push_back(Name());
  }
}

template <>
inline void CommandLineArg<bool>::Put(std::vector<std::string>& aArgs) {
  Put(true, aArgs);
}

template <>
inline void CommandLineArg<uint64_t>::Put(uint64_t aValue,
                                          std::vector<std::string>& aArgs) {
  aArgs.push_back(Name());
  aArgs.push_back(std::to_string(aValue));
}

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static CommandLineArg<const char*> sParentBuildID{"-parentBuildID",
                                                  "parentbuildid"};
static CommandLineArg<const char*> sAppDir{"-appDir", "appdir"};
static CommandLineArg<const char*> sGREOmni{"-greomni", "greomni"};
static CommandLineArg<const char*> sAppOmni{"-appomni", "appomni"};
static CommandLineArg<const char*> sProfile{"-profile", "profile"};

static CommandLineArg<uint64_t> sJsInitHandle{"-jsInitHandle", "jsinithandle"};
static CommandLineArg<uint64_t> sJsInitLen{"-jsInitLen", "jsinitlen"};
static CommandLineArg<uint64_t> sPrefsHandle{"-prefsHandle", "prefshandle"};
static CommandLineArg<uint64_t> sPrefsLen{"-prefsLen", "prefslen"};
static CommandLineArg<uint64_t> sPrefMapHandle{"-prefMapHandle",
                                               "prefmaphandle"};
static CommandLineArg<uint64_t> sPrefMapSize{"-prefMapSize", "prefmapsize"};

static CommandLineArg<uint64_t> sChildID{"-childID", "childid"};

static CommandLineArg<uint64_t> sSandboxingKind{"-sandboxingKind",
                                                "sandboxingkind"};

static CommandLineArg<bool> sSafeMode{"-safeMode", "safemode"};

static CommandLineArg<bool> sIsForBrowser{"-isForBrowser", "isforbrowser"};
static CommandLineArg<bool> sNotForBrowser{"-notForBrowser", "notforbrowser"};

static CommandLineArg<const char*> sPluginPath{"-pluginPath", "pluginpath"};
static CommandLineArg<bool> sPluginNativeEvent{"-pluginNativeEvent",
                                               "pluginnativeevent"};

#if defined(XP_WIN)
#  if defined(MOZ_SANDBOX)
static CommandLineArg<bool> sWin32kLockedDown{"-win32kLockedDown",
                                              "win32klockeddown"};
#  endif  // defined(MOZ_SANDBOX)
static CommandLineArg<bool> sDisableDynamicDllBlocklist{
    "-disableDynamicBlocklist", "disabledynamicblocklist"};
#endif  // defined(XP_WIN)

#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif

}  // namespace geckoargs

}  // namespace mozilla

#endif  // mozilla_GeckoArgs_h
