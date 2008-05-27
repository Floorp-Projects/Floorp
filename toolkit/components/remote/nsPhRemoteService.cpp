/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Adrian Mardare <amardare@qnx.com>
 *   Max Feil <mfeil@qnx.com>
 */

#include <stdlib.h>
#include <nsIWidget.h>
#include <nsCOMPtr.h>
#include "nsIGenericFactory.h"
#include "nsPhRemoteService.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"

#ifdef MOZ_XUL_APP
#include "nsICommandLineRunner.h"
#include "nsXULAppAPI.h"
#else
#include "nsISuiteRemoteService.h"
#endif

#include <Pt.h>

NS_IMPL_QUERY_INTERFACE2(nsPhRemoteService,
                         nsIRemoteService,
                         nsIObserver)

NS_IMETHODIMP_(nsrefcnt)
nsPhRemoteService::AddRef()
{
  return 1;
}

NS_IMETHODIMP_(nsrefcnt)
nsPhRemoteService::Release()
{
  return 1;
}

NS_IMETHODIMP
nsPhRemoteService::Startup(const char* aAppName, const char* aProfileName)
{
  NS_ASSERTION(aAppName, "Don't pass a null appname!");

  if (mIsInitialized)
    return NS_ERROR_ALREADY_INITIALIZED;

  mIsInitialized = PR_TRUE;
  mAppName = aAppName;
  ToLowerCase(mAppName);

  HandleCommandsFor(nsnull, nsnull);

  return NS_OK;
}

NS_IMETHODIMP
nsPhRemoteService::RegisterWindow(nsIDOMWindow* aWindow)
{
  return NS_OK;
}

NS_IMETHODIMP
nsPhRemoteService::Shutdown()
{
  if (!mIsInitialized)
    return NS_ERROR_NOT_INITIALIZED;

  mIsInitialized = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsPhRemoteService::Observe(nsISupports* aSubject,
                            const char *aTopic,
                            const PRUnichar *aData)
{
  // This can be xpcom-shutdown or quit-application, but it's the same either
  // way.
  Shutdown();
  return NS_OK;
}

#define MOZ_REMOTE_MSG_TYPE					100

static void const * RemoteMsgHandler( PtConnectionServer_t *connection, void *user_data,
    		unsigned long type, void const *msg, unsigned len, unsigned *reply_len )
{
    nsresult rv;

	if( type != MOZ_REMOTE_MSG_TYPE ) return NULL;

	/* we are given strings and we reply with strings */
	char *response = NULL;

	// parse the command
    nsCOMPtr<nsICommandLineRunner> cmdline
      (do_CreateInstance("@mozilla.org/toolkit/command-line;1", &rv));
    if (!NS_FAILED(rv)) {

      // 1) Make sure that it looks remotely valid with parens
      // 2) Treat ping() immediately and specially

      nsCAutoString command((char *)msg);
      PRInt32 p1, p2;
      p1 = command.FindChar('(');
      p2 = command.FindChar(')');

      if (p1 != kNotFound && p2 != kNotFound && p1 != 0 && p2 >= p1) {
        command.Truncate(p1);
        command.Trim(" ", PR_TRUE, PR_TRUE);
        ToLowerCase(command);

        //printf("Processing xremote command: %s\n", command.get());

        if (!command.EqualsLiteral("ping")) {
          char* argv[3] = {"dummyappname", "-remote", (char *)msg};
          rv = cmdline->Init(3, argv, nsnull, nsICommandLine::STATE_REMOTE_EXPLICIT);
          if (!NS_FAILED(rv)) {
            rv = cmdline->Run();
            if (NS_ERROR_ABORT == rv)
              response = "500 command not parseable";
            if (NS_FAILED(rv))
              response = "509 internal error";
	      } else
            response = "509 internal error";
        }
      } else
        response = "500 command not parseable";
	} else
      response = "509 internal error";

	PtConnectionReply( connection, response ? strlen(response) : 0, response );

	return ( void * ) 1; /* return any non NULL value to indicate we handled the message */
}

static void client_connect( PtConnector_t *cntr, PtConnectionServer_t *csrvr, void *data )
{
	static PtConnectionMsgHandler_t handlers[] = { { 0, RemoteMsgHandler } };
	PtConnectionAddMsgHandlers( csrvr, handlers, sizeof(handlers)/sizeof(handlers[0]) );
}


void
nsPhRemoteService::HandleCommandsFor( nsIWidget *aWidget, nsIWeakReference* aWindow )
{
	static PRBool ConnectorCreated = PR_FALSE;

///* ATENTIE */ printf( "aProgram=%s aProfile=%s aWidget=%p\n", aProgram?aProgram:"NULL", aProfile?aProfile:"NULL", aWidget );

	if( !ConnectorCreated ) {
		char RemoteServerName[128];
		sprintf( RemoteServerName, "%s_RemoteServer", (char *) mAppName.get() );
		/* create a connector for the remote control */
		PtConnectorCreate( RemoteServerName, client_connect, NULL );
		ConnectorCreated = PR_TRUE;
		}
  return;
}

// {C0773E90-5799-4eff-AD03-3EBCD85624AC}
#define NS_REMOTESERVICE_CID \
  { 0xc0773e90, 0x5799, 0x4eff, { 0xad, 0x3, 0x3e, 0xbc, 0xd8, 0x56, 0x24, 0xac } }

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPhRemoteService)

static const nsModuleComponentInfo components[] =
{
  { "Remote Service",
    NS_REMOTESERVICE_CID,
    "@mozilla.org/toolkit/remote-service;1",
    nsPhRemoteServiceConstructor
  }
};

NS_IMPL_NSGETMODULE(RemoteServiceModule, components)
