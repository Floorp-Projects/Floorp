/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"

void xxxNeverCalledXUL()
{
  XRE_main(0, nullptr, nullptr, 0);
  XRE_GetFileFromPath(nullptr, nullptr);
  XRE_LockProfileDirectory(nullptr, nullptr);
  XRE_InitEmbedding2(nullptr, nullptr, nullptr);
  XRE_NotifyProfile();
  XRE_TermEmbedding();
  XRE_CreateAppData(nullptr, nullptr);
  XRE_ParseAppData(nullptr, nullptr);
  XRE_FreeAppData(nullptr);
  XRE_ChildProcessTypeToString(GeckoProcessType_Default);
  XRE_StringToChildProcessType("");
  XRE_GetProcessType();
  XRE_InitChildProcess(0, nullptr, GeckoProcessType_Default);
  XRE_InitParentProcess(0, nullptr, nullptr, nullptr);
  XRE_RunAppShell();
  XRE_ShutdownChildProcess();
  XRE_SendTestShellCommand(nullptr, nullptr, nullptr);
}
