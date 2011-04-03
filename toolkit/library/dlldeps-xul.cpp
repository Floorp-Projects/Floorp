/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla embedding code.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXULAppAPI.h"

void xxxNeverCalledXUL()
{
  XRE_main(0, nsnull, nsnull);
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
