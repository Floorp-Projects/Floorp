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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Christopher Blizzard and Jamie Zawinski.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adrian Mardare ( amardare@qnx.com )
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

#include "PhRemoteClient.h"
#include "prmem.h"
#include "prprf.h"
#include "plstr.h"
#include "prsystem.h"
#include "prlog.h"
#include "prenv.h"
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <Pt.h>

#define MOZ_REMOTE_MSG_TYPE					100

XRemoteClient::XRemoteClient()
{
	mInitialized = PR_FALSE;
}

XRemoteClient::~XRemoteClient()
{
  if (mInitialized)
    Shutdown();
}

nsresult
XRemoteClient::Init (void)
{

  if (mInitialized)
    return NS_OK;

	/* we have to initialize the toolkit since we use PtConnection stuff to send messages */
	PtInit( NULL );

  mInitialized = PR_TRUE;

  return NS_OK;
}

nsresult
XRemoteClient::SendCommand (const char *aProgram, const char *aUsername,
                            const char *aProfile, const char *aCommand,
							const char* aDesktopStartupID,
                            char **aResponse, PRBool *aWindowFound)
{
  *aWindowFound = PR_TRUE;

	char RemoteServerName[128];

	sprintf( RemoteServerName, "%s_RemoteServer", aProgram ? aProgram : "mozilla" );

	PtConnectionClient_t *cnt = PtConnectionFindName( RemoteServerName, 0, 0 );
	if( !cnt ) {
		/* no window has registered for the remote service */
		*aWindowFound = PR_FALSE;
		return NS_OK;
		}

	if( PtConnectionSend( cnt, MOZ_REMOTE_MSG_TYPE, aCommand, NULL, strlen( aCommand ), 0 ) < 0 )
		return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
XRemoteClient::SendCommandLine (const char *aProgram, const char *aUsername,
                                const char *aProfile,
                                PRInt32 argc, char **argv,
								const char* aDesktopStartupID,
                                char **aResponse, PRBool *aWindowFound)
{
  return NS_ERROR_FAILURE;
}

void
XRemoteClient::Shutdown (void)
{

  if (!mInitialized)
    return;

  // shut everything down
  mInitialized = PR_FALSE;

  return;
}
