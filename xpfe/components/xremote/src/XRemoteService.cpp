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
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by Christopher Blizzard
 * are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 */

#include "XRemoteService.h"

#include <nsIGenericFactory.h>
#include <nsIWebNavigation.h>
#include <nsIDOMWindowInternal.h>
#include <nsIDocShell.h>
#include <nsIScriptGlobalObject.h>
#include <nsIBaseWindow.h>
#include <nsWidgetsCID.h>
#include <nsIXRemoteWidgetHelper.h>
#include <nsIServiceManager.h>
#include <nsRect.h>
#include <nsString.h>

NS_DEFINE_CID(kWindowCID, NS_WINDOW_CID);

// protocol strings
static char *s200ExecutedCommand     = "200 executed command:";
static char *s500ParseCommand        = "500 command not parsable:";
static char *s501UnrecognizedCommand = "501 unrecognized command:";
// not using this yet...
// static char *s502NoWindow            = "502 no appropriate window for:";
static char *s509InternalError       = "509 internal error";

XRemoteService::XRemoteService()
{
  printf("XRemoteService\n");
  NS_INIT_ISUPPORTS();
  mNumWindows = 0;
}

XRemoteService::~XRemoteService()
{
  printf("~XRemoteService\n");
}

NS_IMPL_ISUPPORTS1(XRemoteService, nsIXRemoteService)

NS_IMETHODIMP
XRemoteService::Startup(void)
{
  if (mNumWindows == 0)
    CreateProxyWindow();
  return NS_OK;
}

NS_IMETHODIMP
XRemoteService::Shutdown(void)
{
  DestroyProxyWindow();
  return NS_OK;
}

NS_IMETHODIMP
XRemoteService::ParseCommand(const char *aCommand, char **aResponse)
{
  printf("ParseCommand\n");
  if (!aCommand || !aResponse)
    return NS_ERROR_INVALID_ARG;

  // is there no command?
  if (aCommand[0] == '\0') {
    *aResponse = nsCRT::strdup(s509InternalError);
    return NS_OK;
  }

  *aResponse = nsnull;

  // begin our parse
  nsCString tempString;
  PRInt32 begin_arg = 0;
  PRInt32 end_arg = 0;

  tempString.Append(aCommand);
  printf("raw data is %s\n", tempString.get());
  
  // find the () in the command
  begin_arg = tempString.FindChar('(');
  end_arg = tempString.RFindChar(')');

  printf("begin_arg is %d, end_arg is %d\n", begin_arg, end_arg);

  // make sure that both were found, the string doesn't start with '('
  // and that the ')' follows the '('
  if (begin_arg == kNotFound || end_arg == kNotFound ||
      begin_arg == 0 || end_arg < begin_arg) {
    *aResponse = BuildResponse(s500ParseCommand, tempString.get());
    return NS_OK;
  }

  // truncate the closing paren and anything following it
  tempString.Truncate(end_arg);

  // save the argument and trim whitespace off of it
  nsCString argument;
  argument.Append(tempString);
  argument.Cut(0, begin_arg + 1);
  argument.Trim(" ", PR_TRUE, PR_TRUE);

  // remove the argument
  tempString.Truncate(begin_arg);

  // get the action, strip off whitespace and convert to lower case
  nsCString action;
  action.Append(tempString);
  action.Trim(" ", PR_TRUE, PR_TRUE);
  action.ToLowerCase();

  printf("action %s argument\n", action.get(), argument.get());

  return NS_OK;
}

NS_IMETHODIMP
XRemoteService::AddBrowserInstance(nsIDOMWindowInternal *aBrowser)
{
  printf("AddBrowserInstance %p\n", (void *)aBrowser);

  // get the native window for this instance
  nsCOMPtr<nsIScriptGlobalObject> scriptObject;
  scriptObject = do_QueryInterface(aBrowser);
  if (!scriptObject) {
    NS_WARNING("Failed to get script object for browser instance");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell;
  scriptObject->GetDocShell(getter_AddRefs(docShell));
  if (!docShell) {
    NS_WARNING("Failed to get docshell object for browser instance");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIBaseWindow> baseWindow;
  baseWindow = do_QueryInterface(docShell);
  if (!baseWindow) {
    NS_WARNING("Failed to get base window for browser instance");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWidget> mainWidget;
  baseWindow->GetMainWidget(getter_AddRefs(mainWidget));
  if (!mainWidget) {
    NS_WARNING("Failed to get main widget for browser instance");
    return NS_ERROR_FAILURE;
  }

  printf("widget is %p\n", (void *)mainWidget.get());

  // walk up the widget tree and find the toplevel window in the
  // heirarchy

  nsCOMPtr<nsIWidget> tempWidget;

  tempWidget = getter_AddRefs(mainWidget->GetParent());

  while (tempWidget) {
    tempWidget = getter_AddRefs(tempWidget->GetParent());
    if (tempWidget)
      mainWidget = tempWidget;
  }

  // Tell the widget code to set up X remote for this window
  nsCOMPtr<nsIXRemoteWidgetHelper> widgetHelper =
    do_GetService(NS_IXREMOTEWIDGETHELPER_CONTRACTID);
  if (!widgetHelper) {
    NS_WARNING("couldn't get widget helper service");
    return NS_ERROR_FAILURE;
  }

  nsresult rv;
  rv = widgetHelper->EnableXRemoteCommands(mainWidget);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to enable x remote commands for widget");
    return rv;
  }

  // It's assumed that someone will call RemoveBrowserInstance before
  // this DOM window is destroyed so we don't addref or release or
  // keep a weak ptr or anything.
  nsVoidKey *key;
  key = new nsVoidKey (mainWidget.get());
  mWindowList.Put(key, aBrowser);
  delete key;

  // ...and the reverse lookup
  key = new nsVoidKey (aBrowser);
  mBrowserList.Put(key, mainWidget.get());
  delete key;

  // now that we have a real browser window listening to requests
  // destroy the proxy window.
  DestroyProxyWindow();
  mNumWindows++;
  
  return NS_OK;
}

NS_IMETHODIMP
XRemoteService::RemoveBrowserInstance(nsIDOMWindowInternal *aBrowser)
{
  printf("RemoveBrowserInstance\n");
  mNumWindows--;
  if (mNumWindows == 0)
    CreateProxyWindow();

  // remove our keys
  nsVoidKey *key;
  key = new nsVoidKey(aBrowser);
  nsIWidget *widget = NS_STATIC_CAST(nsIWidget *,
				     mBrowserList.Remove(key));
  delete key;

  key = new nsVoidKey(widget);
  mWindowList.Remove(key);
  delete key;

  return NS_OK;
}

char *
XRemoteService::BuildResponse(const char *aError, const char *aMessage)
{
  nsCString retvalString;
  char *retval;
  
  // check to make sure that we have the minimum for allocating this
  // buffer
  if (!aError || !aMessage)
    return nsnull;
  
  retvalString.Append(aError);
  retvalString.Append(" ");
  retvalString.Append(aMessage);

  retval = retvalString.ToNewCString();
  return retval;
}

void
XRemoteService::CreateProxyWindow(void)
{
  if (mProxyWindow)
    return;

  mProxyWindow = do_CreateInstance(kWindowCID);
  if (!mProxyWindow)
    return;

  nsWidgetInitData initData;
  initData.mWindowType = eWindowType_toplevel;

  // create the window as a new toplevel
  nsRect rect(0,0,100,100);
  mProxyWindow->Create(NS_STATIC_CAST(nsIWidget *, nsnull),
		       rect,
		       nsnull, nsnull, nsnull, nsnull,
		       &initData);

  // Tell the widget code to set up X remote for this window
  nsCOMPtr<nsIXRemoteWidgetHelper> widgetHelper =
    do_GetService(NS_IXREMOTEWIDGETHELPER_CONTRACTID);
  if (!widgetHelper) {
    NS_WARNING("couldn't get widget helper service");
    return;
  }

  nsresult rv;
  rv = widgetHelper->EnableXRemoteCommands(mProxyWindow);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to enable x remote commands for proxy window");
    return;
  }

}

void
XRemoteService::DestroyProxyWindow(void)
{
  if (!mProxyWindow)
    return;

  mProxyWindow->Destroy();
  mProxyWindow = nsnull;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(XRemoteService)

static nsModuleComponentInfo components[] = {
  { NS_IXREMOTESERVICE_CLASSNAME,
    NS_XREMOTESERVICE_CID,
    NS_IXREMOTESERVICE_CONTRACTID,
    XRemoteServiceConstructor }
};

NS_IMPL_NSGETMODULE(XRemoteServiceModule, components);
