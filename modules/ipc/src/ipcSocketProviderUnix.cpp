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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "private/pprio.h"
#include "plstr.h"
#include "nsReadableUtils.h"
#include "nsMemory.h"

#include "ipcSocketProviderUnix.h"
#include "ipcLog.h"

static PRDescIdentity ipcIOLayerIdentity;
static PRIOMethods    ipcIOLayerMethods;
static char          *ipcIOSocketPath;

static PRStatus PR_CALLBACK
ipcIOLayerConnect(PRFileDesc* fd, const PRNetAddr* a, PRIntervalTime timeout)
{ 
    if (!fd || !fd->lower)
        return PR_FAILURE;

    PRStatus status;

    if (ipcIOSocketPath == NULL || ipcIOSocketPath[0] == '\0') {
        NS_ERROR("not initialized");
        return PR_FAILURE;
    }

    //
    // ignore passed in address.
    //
    PRNetAddr addr;
    addr.local.family = PR_AF_LOCAL;
    PL_strncpyz(addr.local.path, ipcIOSocketPath, sizeof(addr.local.path));

    status = fd->lower->methods->connect(fd->lower, &addr, timeout);
    if (status != PR_SUCCESS)
        return status;

    //
    // now that we have a connected socket; do some security checks on the
    // file descriptor.
    //
    //   (1) make sure owner matches
    //   (2) make sure permissions match expected permissions
    //
    // if these conditions aren't met then bail.
    //
    int unix_fd = PR_FileDesc2NativeHandle(fd->lower);  

    struct stat st;
    if (fstat(unix_fd, &st) == -1) {
        NS_ERROR("stat failed");
        return PR_FAILURE;
    }

    if (st.st_uid != getuid() && st.st_uid != geteuid()) {
        NS_ERROR("userid check failed");
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}
    
static void InitIPCMethods()
{
    ipcIOLayerIdentity = PR_GetUniqueIdentity("moz:ipc-layer");

    ipcIOLayerMethods = *PR_GetDefaultIOMethods();
    ipcIOLayerMethods.connect = ipcIOLayerConnect;
}

NS_IMETHODIMP
ipcSocketProviderUnix::NewSocket(const char *host,
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc **fd,
                                 nsISupports **securityInfo)
{
    static PRBool firstTime = PR_TRUE;
    if (firstTime) {
        InitIPCMethods();
        firstTime = PR_FALSE;
    }

    PRFileDesc *sock = PR_OpenTCPSocket(PR_AF_LOCAL);
    if (!sock) return NS_ERROR_OUT_OF_MEMORY;

    PRFileDesc *layer = PR_CreateIOLayerStub(ipcIOLayerIdentity, &ipcIOLayerMethods);
    if (!layer)
        goto loser;
    layer->secret = NULL;

    if (PR_PushIOLayer(sock, PR_GetLayersIdentity(sock), layer) != PR_SUCCESS)
        goto loser;

    *fd = sock;
    return NS_OK;

loser:
    if (layer)
        layer->dtor(layer);
    if (sock)
        PR_Close(sock);
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
ipcSocketProviderUnix::AddToSocket(const char *host,
                                   PRInt32 port,
                                   const char *proxyHost,
                                   PRInt32 proxyPort,
                                   PRFileDesc *fd,
                                   nsISupports **securityInfo)
{
    NS_NOTREACHED("unexpected");
    return NS_ERROR_UNEXPECTED;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(ipcSocketProviderUnix, nsISocketProvider)


void
ipcSocketProviderUnix::SetSocketPath(const nsACString &socketPath)
{
    if (ipcIOSocketPath)
        nsMemory::Free(ipcIOSocketPath);
    ipcIOSocketPath = ToNewCString(socketPath);
}
