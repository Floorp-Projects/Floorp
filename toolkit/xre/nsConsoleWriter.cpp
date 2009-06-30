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
 * The Original Code is the Mozilla Toolkit.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
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

#ifdef NO_NSPR_10_SUPPORT
#undef NO_NSPR_10_SUPPORT
#endif

#include "nsAppRunner.h"

#include "prio.h"
#include "prprf.h"
#include "prtime.h"
#include "prenv.h"

#include "nsCRT.h"
#include "nsNativeCharsetUtils.h"
#include "nsString.h"
#include "nsXREDirProvider.h"
#include "nsXULAppAPI.h"

#include "nsIConsoleService.h"
#include "nsIConsoleMessage.h"

void
WriteConsoleLog()
{
  nsresult rv;

  nsCOMPtr<nsILocalFile> lfile;

  char* logFileEnv = PR_GetEnv("XRE_CONSOLE_LOG");
  if (logFileEnv && *logFileEnv) {
    rv = XRE_GetFileFromPath(logFileEnv, getter_AddRefs(lfile));
    if (NS_FAILED(rv))
      return;
  }
  else {
    if (!gLogConsoleErrors)
      return;

    rv = gDirServiceProvider->GetUserAppDataDirectory(getter_AddRefs(lfile));
    if (NS_FAILED(rv))
      return;

    lfile->AppendNative(NS_LITERAL_CSTRING("console.log"));
  }

  PRFileDesc *file;
  rv = lfile->OpenNSPRFileDesc(PR_WRONLY | PR_APPEND | PR_CREATE_FILE,
                               0660, &file);
  if (NS_FAILED(rv))
    return;

  nsCOMPtr<nsIConsoleService> csrv
    (do_GetService(NS_CONSOLESERVICE_CONTRACTID));
  if (!csrv) {
    PR_Close(file);
    return;
  }

  nsIConsoleMessage** messages;
  PRUint32 mcount;

  rv = csrv->GetMessageArray(&messages, &mcount);
  if (NS_FAILED(rv)) {
    PR_Close(file);
    return;
  }

  if (mcount) {
    PRExplodedTime etime;
    PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &etime);
    char datetime[512];
    PR_FormatTimeUSEnglish(datetime, sizeof(datetime),
                           "%Y-%m-%d %H:%M:%S", &etime);

    PR_fprintf(file, NS_LINEBREAK
                     "*** Console log: %s ***" NS_LINEBREAK,
               datetime);
  }

  // From this point on, we have to release all the messages, and free
  // the memory allocated for the messages array. XPCOM arrays suck.

  nsXPIDLString msg;
  nsCAutoString nativemsg;

  for (PRUint32 i = 0; i < mcount; ++i) {
    rv = messages[i]->GetMessageMoz(getter_Copies(msg));
    if (NS_SUCCEEDED(rv)) {
      NS_CopyUnicodeToNative(msg, nativemsg);
      PR_fprintf(file, "%s" NS_LINEBREAK, nativemsg.get());
    }
    NS_IF_RELEASE(messages[i]);
  }

  PR_Close(file);
  NS_Free(messages);
}
