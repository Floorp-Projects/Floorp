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
#include "XRemoteContentListener.h"

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
#include <nsIPref.h>
#include <nsIWindowWatcher.h>
#include <nsISupportsPrimitives.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIDocShellTreeItem.h>
#include <nsIDocShellTreeOwner.h>
#include <nsIURILoader.h>
#include <nsCURILoader.h>
#include <nsIURI.h>
#include <nsNetUtil.h>
#include <nsIWindowMediator.h>
#include <rdf.h>

NS_DEFINE_CID(kWindowCID, NS_WINDOW_CID);

// protocol strings
static char *s200ExecutedCommand     = "200 executed command:";
static char *s500ParseCommand        = "500 command not parsable:";
static char *s501UnrecognizedCommand = "501 unrecognized command:";
// not used
//static char *s502NoWindow            = "502 no appropriate window for:";
static char *s509InternalError       = "509 internal error";

XRemoteService::XRemoteService()
{
  NS_INIT_ISUPPORTS();
  mNumWindows = 0;
  mRunning = PR_FALSE;
}

XRemoteService::~XRemoteService()
{
  Shutdown();
}

NS_IMPL_ISUPPORTS1(XRemoteService, nsIXRemoteService)

NS_IMETHODIMP
XRemoteService::Startup(void)
{
  mRunning = PR_TRUE;
  if (mNumWindows == 0)
    CreateProxyWindow();
  return NS_OK;
}

NS_IMETHODIMP
XRemoteService::Shutdown(void)
{
  DestroyProxyWindow();
  mRunning = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
XRemoteService::ParseCommand(nsIWidget *aWidget,
			     const char *aCommand, char **aResponse)
{
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
  
  // find the () in the command
  begin_arg = tempString.FindChar('(');
  end_arg = tempString.RFindChar(')');

  // make sure that both were found, the string doesn't start with '('
  // and that the ')' follows the '('
  if (begin_arg == kNotFound || end_arg == kNotFound ||
      begin_arg == 0 || end_arg < begin_arg) {
    *aResponse = BuildResponse(s500ParseCommand, aCommand);
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

  // pull off the noraise argument if it's there.
  PRUint32  index = 0;
  PRBool    raiseWindow = PR_TRUE;
  nsCString lastArgument;

  FindLastInList(argument, lastArgument, &index);
  if (lastArgument.EqualsIgnoreCase("noraise")) {
    argument.Truncate(index);
    raiseWindow = PR_FALSE;
  }

  nsresult rv = NS_OK;
  
  // find the DOM window for the passed in parameter
  nsVoidKey *key;
  key = new nsVoidKey(aWidget);
  if (!key)
    return NS_ERROR_FAILURE;
  // If this fails it's OK since it just means that we got a request
  // on an unknown window.  We can handle that case.
  nsIDOMWindowInternal *domWindow = NS_STATIC_CAST(nsIDOMWindowInternal *,
						   mWindowList.Get(key));
  delete key;

  /*   
      openURL ( ) 
            Prompts for a URL with a dialog box. 
      openURL (URL) 
            Opens the specified document without prompting. 
      openURL (URL, new-window) 
            Create a new window displaying the the specified document. 
  */

  /*
      openFile ( ) 
            Prompts for a file with a dialog box. 
      openFile (File) 
            Opens the specified file without prompting. 

  */

  if (action.Equals("openurl") || action.Equals("openfile")) {
    if (argument.IsEmpty())
      rv = OpenURLDialog(domWindow);
    else
      rv = OpenURL(argument, domWindow, PR_TRUE);
  }

  /*
      saveAs ( ) 
            Prompts for a file with a dialog box (like the menu item). 
      saveAs (Output-File) 
            Writes HTML to the specified file without prompting. 
      saveAs (Output-File, Type) 
            Writes to the specified file with the type specified -
	    the type may be HTML, Text, or PostScript. 

  */

  else if (action.Equals("saveas")) {
    if (argument.IsEmpty()) {
      rv = NS_ERROR_NOT_IMPLEMENTED;
    }
    else {
      // check to see if it has a type on it
      index = 0;
      FindLastInList(argument, lastArgument, &index);
      if (lastArgument.EqualsIgnoreCase("html")) {
	argument.Truncate(index);
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
      else if (lastArgument.EqualsIgnoreCase("text", PR_TRUE)) {
	argument.Truncate(index);
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
      else if (lastArgument.EqualsIgnoreCase("postscript", PR_TRUE)) {
	argument.Truncate(index);
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
      else {
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
    }
   
  }

  /*
      mailto ( ) 
            pops up the mail dialog with the To: field empty. 
      mailto (a, b, c) 
            Puts the addresses "a, b, c" in the default To: field. 

  */

  else if (action.Equals("mailto")) {
    // if you prepend mailto: to the string it will be a mailto: url
    // and openurl should work fine.
    nsCString tempArg("mailto:");
    tempArg.Append(argument);
    rv = OpenURL(tempArg, domWindow, PR_FALSE);
  }

  /*
      addBookmark ( ) 
            Adds the current document to the bookmark list. 
      addBookmark (URL) 
            Adds the given document to the bookmark list. 
      addBookmark (URL, Title) 
            Adds the given document to the bookmark list,
	    with the given title. 
  */

  else if (action.Equals("addbookmark")) {
    if (argument.IsEmpty()) {
      rv = NS_ERROR_NOT_IMPLEMENTED;
    }
    else {
      index = 0;
      FindLastInList(argument, lastArgument, &index);
      if (!lastArgument.IsEmpty()) {
	nsCString title(lastArgument);
	argument.Truncate(index);
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
      else {
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
    }
  }

  /* some extensions! */
  
  /*
      ping()
         Just responds with an OK to let a client know that we are here.
  */

  else if (action.Equals("ping")) {
    // the 200 will get filled in below
    rv = NS_OK;
  }

  /*
      xfeDoCommand()
       This is an interface to make the xfe "do stuff."  Lifted from
       the old Netscape 4.x interface.
  */

  else if (action.Equals("xfedocommand")) {
    rv = XfeDoCommand(argument, domWindow);
  }

  // bad command
  else {
    rv = NS_ERROR_FAILURE;
    *aResponse = BuildResponse(s501UnrecognizedCommand, aCommand);
  }

  // if we failed and *aResponse isn't already filled in, fill it in
  // with a generic internal error message.
  if (NS_FAILED(rv)) {
    if (!*aResponse) {
      if (rv == NS_ERROR_NOT_IMPLEMENTED)
	*aResponse = BuildResponse(s501UnrecognizedCommand, aCommand);
      else
	*aResponse = nsCRT::strdup(s509InternalError);
    }
  }

  // if we got this far then everything worked.
  if (!*aResponse)
    *aResponse = BuildResponse(s200ExecutedCommand, aCommand);

  return rv;
}

NS_IMETHODIMP
XRemoteService::AddBrowserInstance(nsIDOMWindowInternal *aBrowser)
{

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
  if (!key)
    return NS_ERROR_FAILURE;
  mWindowList.Put(key, aBrowser);
  delete key;

  // ...and the reverse lookup
  key = new nsVoidKey (aBrowser);
  if (!key)
    return NS_ERROR_FAILURE;
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
  mNumWindows--;
  if (mNumWindows == 0 && mRunning)
    CreateProxyWindow();

  // remove our keys
  nsVoidKey *key;
  key = new nsVoidKey(aBrowser);
  if (!key)
    return NS_ERROR_FAILURE;
  nsIWidget *widget = NS_STATIC_CAST(nsIWidget *,
				     mBrowserList.Remove(key));
  delete key;

  key = new nsVoidKey(widget);
  if (!key)
    return NS_ERROR_FAILURE;
  mWindowList.Remove(key);
  delete key;

  return NS_OK;
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
  nsresult rv;
  rv = mProxyWindow->Create(NS_STATIC_CAST(nsIWidget *, nsnull),
			    rect,
			    nsnull, nsnull, nsnull, nsnull,
			    &initData);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create proxy window");
    return;
  }

  // Tell the widget code to set up X remote for this window
  nsCOMPtr<nsIXRemoteWidgetHelper> widgetHelper =
    do_GetService(NS_IXREMOTEWIDGETHELPER_CONTRACTID);
  if (!widgetHelper) {
    NS_WARNING("couldn't get widget helper service");
    return;
  }

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

  retval = ToNewCString(retvalString);
  return retval;
}

void
XRemoteService::FindLastInList(nsCString &aString, nsCString &retString,
			       PRUint32 *aIndexRet)
{
  // init our return
  *aIndexRet = 0;
  // make a copy to work with
  nsCString tempString = aString;
  PRInt32   strIndex;
  // find out of there's a , at the end of the string
  strIndex = tempString.RFindChar(',');

  // give up now if you can
  if (strIndex == kNotFound)
    return;

  // cut the string down to the first ,
  tempString.Cut(0, strIndex + 1);

  // strip off leading + trailing whitespace
  tempString.Trim(" ", PR_TRUE, PR_TRUE);

  // see if we've reduced it to nothing
  if (tempString.IsEmpty())
    return;

  *aIndexRet = strIndex;

  // otherwise, return it as a new C string
  retString = tempString;

}

nsresult
XRemoteService::OpenChromeWindow(nsIDOMWindow *aParent,
				 const char *aUrl, const char *aFeatures,
				 nsISupports *aArguments,
				 nsIDOMWindow **_retval)
{
  nsCOMPtr<nsIWindowWatcher> watcher;
  watcher = do_GetService("@mozilla.org/embedcomp/window-watcher;1");
    
  if (!watcher)
    return NS_ERROR_FAILURE;

  return watcher->OpenWindow(aParent, aUrl, "_blank",
			     aFeatures, aArguments, _retval);
}

nsresult
XRemoteService::GetBrowserLocation(char **_retval)
{
  // get the browser chrome URL
  nsCOMPtr<nsIPref> prefs;
  prefs = do_GetService(NS_PREF_CONTRACTID);
  if (!prefs)
    return NS_ERROR_FAILURE;
  
  prefs->CopyCharPref("browser.chromeURL", _retval);

  // fallback
  if (!*_retval)
    *_retval = nsCRT::strdup("chrome://navigator/content/navigator.xul");

  return NS_OK;
}

nsresult
XRemoteService::GetMailLocation(char **_retval)
{
  // get the mail chrome URL
  nsCOMPtr<nsIPref> prefs;
  prefs = do_GetService(NS_PREF_CONTRACTID);
  if (!prefs)
    return NS_ERROR_FAILURE;
  
  PRInt32 retval = 0;
  nsresult rv;
  rv = prefs->GetIntPref("mail.pane_config", &retval);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  if (!retval)
    *_retval = nsCRT::strdup("chrome://messenger/content/messenger.xul");
  else
    *_retval = nsCRT::strdup("chrome://messenger/content/mail3PaneWindowVertLayout.xul");

  return NS_OK;
  
}

nsresult
XRemoteService::OpenURL(nsCString &aArgument,
			nsIDOMWindowInternal *aParent,
			PRBool aOpenBrowser)
{
  // the eventual toplevel target of the load
  nsCOMPtr<nsIDOMWindowInternal> finalWindow = aParent;

  // see if there's a new window argument on the end
  nsCString lastArgument;
  PRBool    newWindow = PR_FALSE;
  PRUint32  index = 0;
  FindLastInList(aArgument, lastArgument, &index);
  if (lastArgument.EqualsIgnoreCase("new-window")) {
    aArgument.Truncate(index);
    // only open new windows if it's OK to do so
    if (aOpenBrowser)
      newWindow = PR_TRUE;
    // recheck for a possible noraise argument since it might have
    // been before the new-window argument
    FindLastInList(aArgument, lastArgument, &index);
    if (lastArgument.EqualsIgnoreCase("noraise"))
      aArgument.Truncate(index);
  }

  // If it's OK to open a new browser window and a new window flag
  // wasn't passed in then try to find a current window.  If that's
  // not found then go ahead and open a new window/
  if (aOpenBrowser && !newWindow) {
    nsCOMPtr<nsIDOMWindowInternal> lastUsedWindow;
    FindWindow(NS_LITERAL_STRING("navigator:browser").get(),
	       getter_AddRefs(lastUsedWindow));

    if (lastUsedWindow)
      finalWindow = lastUsedWindow;
    else
      newWindow = PR_TRUE;
  }

  // try to fixup the argument passed in

  nsString url;
  url.AssignWithConversion(aArgument);

  nsresult rv = NS_OK;

  if (newWindow) {
    nsXPIDLCString urlString;
    GetBrowserLocation(getter_Copies(urlString));
    if (!urlString)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupportsWString> arg;
    arg = do_CreateInstance(NS_SUPPORTS_WSTRING_CONTRACTID);
    if (!arg)
      return NS_ERROR_FAILURE;
    
    // save the url into the wstring
    arg->SetData(url.get());
    
    nsCOMPtr<nsIDOMWindow> window;
    rv = OpenChromeWindow(finalWindow, urlString, "chrome,all,dialog=no",
			  arg, getter_AddRefs(window));
  }

  // if no new window flag was set but there's no parent then we have
  // to pass everything off to the uri loader
  else if (!finalWindow) {
    nsCOMPtr<nsIURILoader> loader;
    loader = do_GetService(NS_URI_LOADER_CONTRACTID);
    if (!loader)
      return NS_ERROR_FAILURE;
    
    XRemoteContentListener *listener;
    listener = new XRemoteContentListener();
    if (!listener)
      return NS_ERROR_FAILURE;

    // we own it
    NS_ADDREF(listener);
    nsCOMPtr<nsISupports> listenerRef;
    listenerRef = do_QueryInterface(NS_STATIC_CAST(nsIURIContentListener *,
						   listener));
    // now the listenerref is the only reference
    NS_RELEASE(listener);

    // create our uri object
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), url);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

    // open a channel
    nsCOMPtr<nsIChannel> channel;
    rv = NS_OpenURI(getter_AddRefs(channel), uri);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

    // load it
    rv = loader->OpenURI(channel, PR_TRUE, listenerRef);
  }

  else {
    // find the primary content shell for the window that we've been
    // asked to load into.
    nsCOMPtr<nsIScriptGlobalObject> scriptObject;
    scriptObject = do_QueryInterface(finalWindow);
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

    nsCOMPtr<nsIDocShellTreeItem> item;
    item = do_QueryInterface(docShell);
    if (!item) {
      NS_WARNING("failed to get doc shell tree item for browser instance");
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    item->GetTreeOwner(getter_AddRefs(treeOwner));
    if (!treeOwner) {
      NS_WARNING("failed to get tree owner");
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDocShellTreeItem> primaryContent;
    treeOwner->GetPrimaryContentShell(getter_AddRefs(primaryContent));

    docShell = do_QueryInterface(primaryContent);
    if (!docShell) {
      NS_WARNING("failed to get docshell from primary content item");
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIWebNavigation> webNav;
    webNav = do_GetInterface(docShell);
    if (!webNav) {
      NS_WARNING("failed to get web nav from inner docshell");
      return NS_ERROR_FAILURE;
    }

    rv = webNav->LoadURI(url.get(), nsIWebNavigation::LOAD_FLAGS_NONE);
      
  }
  
  return rv;
}

nsresult
XRemoteService::OpenURLDialog(nsIDOMWindowInternal *aParent)
{
  nsresult rv;

  nsIDOMWindow *finalParent = aParent;
  nsCOMPtr<nsIDOMWindow> window;

  // if there's no parent then create a new browser window to be the
  // parent.
  if (!finalParent) {
    nsXPIDLCString urlString;
    GetBrowserLocation(getter_Copies(urlString));
    if (!urlString)
      return NS_ERROR_FAILURE;
    
    rv = OpenChromeWindow(nsnull, urlString, "chrome,all,dialog=no",
			  nsnull, getter_AddRefs(window));
    if (NS_FAILED(rv))
      return rv;

    finalParent = window.get();

  }

  nsCOMPtr<nsIDOMWindow> newWindow;
  rv = OpenChromeWindow(finalParent,
			"chrome://communicator/content/openLocation.xul",
			"chrome,all",
			finalParent,
			getter_AddRefs(newWindow));
  
  return rv;
}

nsresult
XRemoteService::XfeDoCommand(nsCString &aArgument,
			     nsIDOMWindowInternal *aParent)
{
  nsresult rv = NS_OK;

  // someone requested opening mail/news
  if (aArgument.EqualsIgnoreCase("openinbox")) {

    // check to see if it's already running
    nsCOMPtr<nsIDOMWindowInternal> domWindow;

    rv = FindWindow(NS_LITERAL_STRING("mail:3pane").get(),
		    getter_AddRefs(domWindow));

    if (NS_FAILED(rv))
      return rv;

    // focus the window if it was found
    if (domWindow) {
      domWindow->Focus();
    }

    // otherwise open a new mail/news window
    else {
      // get the mail chrome location
      nsXPIDLCString mailLocation;
      GetMailLocation(getter_Copies(mailLocation));
      if (!mailLocation)
	return NS_ERROR_FAILURE;

      nsCOMPtr<nsIDOMWindow> newWindow;
      rv = OpenChromeWindow(0, mailLocation, "chrome,all,dialog=no",
			    nsnull, getter_AddRefs(newWindow));
    }
  }

  // open a new browser window
  else if (aArgument.EqualsIgnoreCase("openbrowser")) {
    nsXPIDLCString browserLocation;
    GetBrowserLocation(getter_Copies(browserLocation));
    if (!browserLocation)
      return NS_ERROR_FAILURE;
    
    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = OpenChromeWindow(0, browserLocation, "chrome,all,dialog=np",
			  nsnull, getter_AddRefs(newWindow));
  }

  // open a new compose window
  else if (aArgument.EqualsIgnoreCase("composemessage")) {
    nsCString tempString("mailto:");
    rv = OpenURL(tempString, nsnull, PR_FALSE);
  }

  return rv;
}

nsresult
XRemoteService::FindWindow(const PRUnichar *aType,
			   nsIDOMWindowInternal **_retval)
{
  nsCOMPtr<nsIWindowMediator> mediator;
  mediator = do_GetService(NS_RDF_DATASOURCE_CONTRACTID_PREFIX
			   "window-mediator");

  if (!mediator)
    return NS_ERROR_FAILURE;

  return mediator->GetMostRecentWindow(aType, _retval);
}

NS_GENERIC_FACTORY_CONSTRUCTOR(XRemoteService)

static nsModuleComponentInfo components[] = {
  { NS_IXREMOTESERVICE_CLASSNAME,
    NS_XREMOTESERVICE_CID,
    NS_IXREMOTESERVICE_CONTRACTID,
    XRemoteServiceConstructor }
};

NS_IMPL_NSGETMODULE(XRemoteServiceModule, components);
