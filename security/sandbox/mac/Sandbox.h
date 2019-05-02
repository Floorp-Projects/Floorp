/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Sandbox_h
#define mozilla_Sandbox_h

#include <string>

enum MacSandboxType {
  MacSandboxType_Default = 0,
  MacSandboxType_Content,
  MacSandboxType_Flash,
  MacSandboxType_GMP,
  MacSandboxType_Utility,
  MacSandboxType_Invalid
};

typedef struct _MacSandboxInfo {
  _MacSandboxInfo()
      : type(MacSandboxType_Default),
        level(0),
        hasFilePrivileges(false),
        hasSandboxedProfile(false),
        hasAudio(false),
        hasWindowServer(false),
        shouldLog(false) {}
  _MacSandboxInfo(const struct _MacSandboxInfo& other) = default;

  void AppendAsParams(std::vector<std::string>& aParams) const;
  static void AppendFileAccessParam(std::vector<std::string>& aParams,
                                    bool aHasFilePrivileges);

 private:
  void AppendStartupParam(std::vector<std::string>& aParams) const;
  void AppendLoggingParam(std::vector<std::string>& aParams) const;
  void AppendAppPathParam(std::vector<std::string>& aParams) const;
  void AppendLevelParam(std::vector<std::string>& aParams) const;
  void AppendAudioParam(std::vector<std::string>& aParams) const;
  void AppendWindowServerParam(std::vector<std::string>& aParams) const;
  void AppendReadPathParams(std::vector<std::string>& aParams) const;
#ifdef DEBUG
  void AppendDebugWriteDirParam(std::vector<std::string>& aParams) const;
#endif

 public:
  MacSandboxType type;
  int32_t level;
  bool hasFilePrivileges;
  bool hasSandboxedProfile;
  bool hasAudio;
  bool hasWindowServer;

  std::string appPath;
  std::string appBinaryPath;
  std::string appDir;
  std::string profileDir;
  std::string debugWriteDir;

  std::string pluginPath;
  std::string pluginBinaryPath;

  std::string testingReadPath1;
  std::string testingReadPath2;
  std::string testingReadPath3;
  std::string testingReadPath4;

  std::string crashServerPort;

  bool shouldLog;
} MacSandboxInfo;

namespace mozilla {

bool StartMacSandbox(MacSandboxInfo const& aInfo, std::string& aErrorMessage);
bool StartMacSandboxIfEnabled(MacSandboxType aSandboxType, int aArgc,
                              char** aArgv, std::string& aErrorMessage);
#ifdef DEBUG
void AssertMacSandboxEnabled();
#endif /* DEBUG */

}  // namespace mozilla

#endif  // mozilla_Sandbox_h
