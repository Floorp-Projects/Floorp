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
 * Christopher Blizzard. Portions created by Christopher Blizzard are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adrian Mardare <amardare@qnx.com>
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

#include <stdlib.h>
#include <nsIWidget.h>
#include <nsIXRemoteService.h>
#include <nsCOMPtr.h>
#include "nsPhMozRemoteHelper.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"

#include <Pt.h>


//-------------------------------------------------------------------------
//
// A client has connected and probably will want to xremote control us
//
//-------------------------------------------------------------------------

#define MOZ_REMOTE_MSG_TYPE					100

static void const * RemoteMsgHandler( PtConnectionServer_t *connection, void *user_data,
    		unsigned long type, void const *msg, unsigned len, unsigned *reply_len )
{
	if( type != MOZ_REMOTE_MSG_TYPE ) return NULL;

	/* we are given strings and we reply with strings */
	char *command = ( char * ) msg, *response = NULL;

	// parse the command
	nsCOMPtr<nsIXRemoteService> remoteService;
	remoteService = do_GetService(NS_IXREMOTESERVICE_CONTRACTID);

	if( remoteService ) {
		command[len] = 0;
		/* it seems we can pass any non-null value as the first argument - if this changes, pass a valid nsWidget* and move this code to nsWidget.cpp */
  	remoteService->ParseCommand( (nsIWidget*)0x1, command, &response );
		}

	PtConnectionReply( connection, response ? strlen( response ) : 0, response );

	if( response ) nsCRT::free( response );

	return ( void * ) 1; /* return any non NULL value to indicate we handled the message */
}

static void client_connect( PtConnector_t *cntr, PtConnectionServer_t *csrvr, void *data )
{
	static PtConnectionMsgHandler_t handlers[] = { { 0, RemoteMsgHandler } };
	PtConnectionAddMsgHandlers( csrvr, handlers, sizeof(handlers)/sizeof(handlers[0]) );
}



nsPhXRemoteWidgetHelper::nsPhXRemoteWidgetHelper()
{
}

nsPhXRemoteWidgetHelper::~nsPhXRemoteWidgetHelper()
{
}

NS_IMPL_ISUPPORTS1(nsPhXRemoteWidgetHelper, nsIXRemoteWidgetHelper)

NS_IMETHODIMP
nsPhXRemoteWidgetHelper::EnableXRemoteCommands( nsIWidget *aWidget, const char *aProfile, const char *aProgram )
{
	static PRBool ConnectorCreated = PR_FALSE;

/* ATENTIE */ printf( "aProgram=%s aProfile=%s aWidget=%p\n", aProgram?aProgram:"NULL", aProfile?aProfile:"NULL", aWidget );

	if( !ConnectorCreated ) {
		char RemoteServerName[128];
		sprintf( RemoteServerName, "%s_RemoteServer", aProgram ? aProgram : "mozilla" );
		/* create a connector for the xremote control */
		PtConnectorCreate( RemoteServerName, client_connect, NULL );
		ConnectorCreated = PR_TRUE;
		}

  return NS_OK;
}
