/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"

void xxxNeverCalledXUL()
{
  XRE_main(0, nsnull, nsnull, 0);
  XRE_GetFileFromPath(nsnull, nsnull);
  XRE_LockProfileDirectory(nsnull, nsnull);
  XRE_InitEmbedding2(nsnull, nsnull, nsnull);
  XRE_NotifyProfile();
  XRE_TermEmbedding();
  XRE_CreateAppData(nsnull, nsnull);
  XRE_ParseAppData(nsnull, nsnull);
  XRE_FreeAppData(nsnull);
  XRE_ChildProcessTypeToString(GeckoProcessType_Default);
  XRE_StringToChildProcessType("");
  XRE_GetProcessType();
  XRE_InitChildProcess(0, nsnull, GeckoProcessType_Default);
  XRE_InitParentProcess(0, nsnull, nsnull, nsnull);
  XRE_RunAppShell();
  XRE_ShutdownChildProcess();
  XRE_SendTestShellCommand(nsnull, nsnull, nsnull);
}
