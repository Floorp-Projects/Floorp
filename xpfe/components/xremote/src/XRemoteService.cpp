/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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
 * Christopher Blizzard <blizzard@mozilla.org>.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "XRemoteService.h"
#include "XRemoteContentListener.h"
#include "nsIBrowserDOMWindow.h"

#include <nsIGenericFactory.h>
#include <nsIWebNavigation.h>
#include <nsPIDOMWindow.h>
#include <nsIDOMChromeWindow.h>
#include <nsIDocShell.h>
#include <nsIBaseWindow.h>
#include <nsIServiceManager.h>
#include <nsString.h>
#include <nsCRT.h>
#include <nsIPref.h>
#include <nsIWindowWatcher.h>
#include <nsXPCOM.h>
#include <nsISupportsPrimitives.h>
#include <nsIInterfaceRequestor.h>
#include <nsIInterfaceRequestorUtils.h>
#include <nsIDocShellTreeItem.h>
#include <nsIDocShellTreeOwner.h>
#include <nsIURILoader.h>
#include <nsCURILoader.h>
#include <nsIURIFixup.h>
#include <nsCDefaultURIFixup.h>
#include <nsIURI.h>
#include <nsNetUtil.h>
#include <nsIWindowMediator.h>
#include <nsCExternalHandlerService.h>
#include <nsIExternalProtocolService.h>
#include <nsIProfile.h>

#include "nsICmdLineHandler.h"

XRemoteService::XRemoteService()
{
}

XRemoteService::~XRemoteService()
{
}

NS_IMPL_ISUPPORTS1(XRemoteService, nsISuiteRemoteService)

NS_IMETHODIMP
XRemoteService::ParseCommand(const char *aCommand, nsIDOMWindow* aWindow)
{
  NS_ASSERTION(aCommand, "Tell me what to do, or shut up!");

  // begin our parse
  nsCString tempString(aCommand);

  // find the () in the command
  PRInt32 begin_arg = tempString.FindChar('(');
  PRInt32 end_arg = tempString.RFindChar(')');

  // make sure that both were found, the string doesn't start with '('
  // and that the ')' follows the '('
  if (begin_arg == kNotFound || end_arg == kNotFound ||
      begin_arg == 0 || end_arg < begin_arg)
    return NS_ERROR_INVALID_ARG;

  // truncate the closing paren and anything following it
  tempString.Truncate(end_arg);

  // save the argument and trim whitespace off of it
  nsCString argument(tempString);
  argument.Cut(0, begin_arg + 1);
  argument.Trim(" ", PR_TRUE, PR_TRUE);

  // remove the argument
  tempString.Truncate(begin_arg);

  // get the action, strip off whitespace and convert to lower case
  nsCString action(tempString);
  action.Trim(" ", PR_TRUE, PR_TRUE);
  ToLowerCase(action);

  // pull off the noraise argument if it's there.
  PRUint32  index = 0;
  PRBool    raiseWindow = PR_TRUE;
  nsCString lastArgument;

  FindLastInList(argument, lastArgument, &index);
  if (lastArgument.LowerCaseEqualsLiteral("noraise")) {
    argument.Truncate(index);
    raiseWindow = PR_FALSE;
  }

  nsresult rv = NS_OK;
  
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
    rv = OpenURL(argument, aWindow, PR_TRUE);
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
      if (lastArgument.LowerCaseEqualsLiteral("html")) {
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
    rv = OpenURL(tempArg, aWindow, PR_FALSE);
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
    rv = NS_ERROR_NOT_IMPLEMENTED;
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
    rv = XfeDoCommand(argument, aWindow);
  }

  // bad command
  else {
    rv = NS_ERROR_FAILURE;
  }

  return rv;
}

void
XRemoteService::FindRestInList(nsCString &aString, nsCString &retString,
                               PRUint32 *aIndexRet)
{
  // init our return
  *aIndexRet = 0;
  nsCString tempString;
  PRInt32   strIndex;
  // find out if there's a comma from the start of the string
  strIndex = aString.FindChar(',');

  // give up now if you can
  if (strIndex == kNotFound)
    return;

  // cut the string down to the first ,
  tempString = Substring(aString, strIndex+1, aString.Length());

  // strip off leading + trailing whitespace
  tempString.Trim(" ", PR_TRUE, PR_TRUE);

  // see if we've reduced it to nothing
  if (tempString.IsEmpty())
    return;

  *aIndexRet = strIndex;

  // otherwise, return it as a new C string
  retString = tempString;

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
  watcher = do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    
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
  *_retval = nsCRT::strdup("chrome://messenger/content/");

  return NS_OK;
  
}

nsresult
XRemoteService::GetComposeLocation(const char **_retval)
{
  // get the Compose chrome URL
  *_retval = "chrome://messenger/content/messengercompose/messengercompose.xul";

  return NS_OK;
}

nsresult
XRemoteService::GetCalendarLocation(char **_retval)
{
  // get the calendar chrome URL
  nsCOMPtr<nsIPref> prefs;
  prefs = do_GetService(NS_PREF_CONTRACTID);
  if (!prefs)
    return NS_ERROR_FAILURE;

  prefs->CopyCharPref("calendar.chromeURL", _retval);

  // fallback
  if (!*_retval)
    *_retval = nsCRT::strdup("chrome://calendar/content/calendar.xul");

  return NS_OK;
}

PRBool
XRemoteService::MayOpenURL(const nsCString &aURL)
{
  // by default, we assume nothing can be loaded.
  PRBool allowURL= PR_FALSE;

  nsCOMPtr<nsIExternalProtocolService> extProtService =
      do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
  if (extProtService) {
    nsCAutoString scheme;

    // empty URLs will be treated as about:blank by OpenURL
    if (aURL.IsEmpty()) {
      scheme.AssignLiteral("about");
    }
    else {
      nsCOMPtr<nsIURIFixup> fixup = do_GetService(NS_URIFIXUP_CONTRACTID);
      if (fixup) {
        nsCOMPtr<nsIURI> uri;
        nsresult rv =
          fixup->CreateFixupURI(aURL, nsIURIFixup::FIXUP_FLAGS_MAKE_ALTERNATE_URI,
                                getter_AddRefs(uri));
        if (NS_SUCCEEDED(rv) && uri) {
          uri->GetScheme(scheme);
        }
      }
    }

    if (!scheme.IsEmpty()) {
      // if the given URL scheme corresponds to an exposed protocol, then we
      // can try to load it.  otherwise, we must not.
      PRBool isExposed;
      nsresult rv = extProtService->IsExposedProtocol(scheme.get(), &isExposed);
      if (NS_SUCCEEDED(rv) && isExposed)
        allowURL = PR_TRUE; // ok, we can load this URL.
    }
  }

  return allowURL;
}

nsresult
XRemoteService::OpenURL(nsCString &aArgument,
			nsIDOMWindow *aParent,
			PRBool aOpenBrowser)
{
  // the eventual toplevel target of the load
  nsCOMPtr<nsIDOMWindowInternal> finalWindow = do_QueryInterface(aParent);

  // see if there's a new-window or new-tab argument on the end
  nsCString lastArgument;
  PRBool    newWindow = PR_FALSE, newTab = PR_FALSE;
  PRUint32  index = 0;
  FindLastInList(aArgument, lastArgument, &index);

  newTab = lastArgument.LowerCaseEqualsLiteral("new-tab");

  if (newTab || lastArgument.LowerCaseEqualsLiteral("new-window")) {
    aArgument.Truncate(index);
    // only open new windows if it's OK to do so
    if (!newTab && aOpenBrowser)
      newWindow = PR_TRUE;
    // recheck for a possible noraise argument since it might have
    // been before the new-window argument
    FindLastInList(aArgument, lastArgument, &index);
    if (lastArgument.LowerCaseEqualsLiteral("noraise"))
      aArgument.Truncate(index);
  }

  nsCOMPtr<nsIBrowserDOMWindow> bwin;

  // If it's OK to open a new browser window and a new window flag
  // wasn't passed in then try to find a current window.  If that's
  // not found then go ahead and open a new window.
  // If we're trying to open a new tab, we'll fall back to opening
  // a new window if there's no browser window open, so look for it
  // here.
  if (aOpenBrowser && (!newWindow || newTab)) {
    nsCOMPtr<nsIDOMWindowInternal> lastUsedWindow;
    FindWindow(NS_LITERAL_STRING("navigator:browser").get(),
	       getter_AddRefs(lastUsedWindow));

    if (lastUsedWindow) {
      finalWindow = lastUsedWindow;
      nsCOMPtr<nsIWebNavigation> navNav(do_GetInterface(finalWindow));
      nsCOMPtr<nsIDocShellTreeItem> navItem(do_QueryInterface(navNav));
      if (navItem) {
        nsCOMPtr<nsIDocShellTreeItem> rootItem;
        navItem->GetRootTreeItem(getter_AddRefs(rootItem));
        nsCOMPtr<nsIDOMWindow> rootWin(do_GetInterface(rootItem));
        nsCOMPtr<nsIDOMChromeWindow> chromeWin(do_QueryInterface(rootWin));
        if (chromeWin)
          chromeWin->GetBrowserDOMWindow(getter_AddRefs(bwin));
      }
    }
    if (!finalWindow || !bwin)
      newWindow = PR_TRUE;
  }

  // check if we can handle this type of URL
  if (!MayOpenURL(aArgument))
    return NS_ERROR_ABORT;

  nsresult rv = NS_OK;

  // try to fixup the argument passed in
  nsString url;
  url.AssignWithConversion(aArgument.get());

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), url);

  if (newWindow) {
    nsXPIDLCString urlString;
    GetBrowserLocation(getter_Copies(urlString));
    if (!urlString)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsISupportsString> arg;
    arg = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
    if (!arg)
      return NS_ERROR_FAILURE;
    
    // save the url into the string object
    arg->SetData(url);
    
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
    listenerRef = do_QueryInterface(static_cast<nsIURIContentListener *>
                                               (listener));
    // now the listenerref is the only reference
    NS_RELEASE(listener);

    // double-check our uri object
    if (!uri)
      return NS_ERROR_FAILURE;

    // open a channel
    nsCOMPtr<nsIChannel> channel;
    rv = NS_NewChannel(getter_AddRefs(channel), uri);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

    // load it
    rv = loader->OpenURI(channel, PR_TRUE, listener);
  }

  else if (newTab && aOpenBrowser) {
    if (bwin && uri) {
      nsCOMPtr<nsIDOMWindow> container;
      rv = bwin->OpenURI(uri, 0,
                         nsIBrowserDOMWindow::OPEN_NEWTAB,
                         nsIBrowserDOMWindow::OPEN_EXTERNAL,
                         getter_AddRefs(container));
    }
    else {
      NS_ERROR("failed to open remote URL in new tab");
      return NS_ERROR_FAILURE;
    }
  }

  else if (bwin && uri) { // unspecified new browser URL; use prefs
    nsCOMPtr<nsIDOMWindow> container;
    rv = bwin->OpenURI(uri, 0,
                       nsIBrowserDOMWindow::OPEN_DEFAULTWINDOW,
                       nsIBrowserDOMWindow::OPEN_EXTERNAL,
                       getter_AddRefs(container));
    if (NS_SUCCEEDED(rv))
      return NS_OK;
  }

  else { // non-browser URLs
    // find the primary content shell for the window that we've been
    // asked to load into.
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(finalWindow));
    if (!win) {
      NS_WARNING("Failed to get script object for browser instance");
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDocShell> docShell = win->GetDocShell();
    if (!docShell) {
      NS_WARNING("Failed to get docshell object for browser instance");
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIDocShellTreeItem> item(do_QueryInterface(docShell));
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

    rv = webNav->LoadURI(url.get(),
                         nsIWebNavigation::LOAD_FLAGS_NONE,
                         nsnull,
                         nsnull,
                         nsnull);

  }

  return rv;
}

nsresult
XRemoteService::XfeDoCommand(nsCString &aArgument,
                             nsIDOMWindow *aParent)
{
  nsresult rv = NS_OK;
  
  // see if there are any arguments on the end
  nsCString restArgument;
  PRUint32  index;
  FindRestInList(aArgument, restArgument, &index);

  if (!restArgument.IsEmpty())
    aArgument.Truncate(index);
  nsCOMPtr<nsISupportsString> arg;
  arg = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  
  if (NS_FAILED(rv))
    return rv;
  
  // pass the second argument as parameter
  arg->SetData(NS_ConvertUTF8toUTF16(restArgument));

  // someone requested opening mail/news
  if (aArgument.LowerCaseEqualsLiteral("openinbox")) {

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
                            arg, getter_AddRefs(newWindow));
    }
  }

  // open a new browser window
  else if (aArgument.LowerCaseEqualsLiteral("openbrowser")) {
    // Get the browser URL and the default start page URL.
    nsCOMPtr<nsICmdLineHandler> browserHandler =
        do_GetService("@mozilla.org/commandlinehandler/general-startup;1?type=browser");

    if (!browserHandler)
        return NS_ERROR_FAILURE;

    nsXPIDLCString browserLocation;
    browserHandler->GetChromeUrlForTask(getter_Copies(browserLocation));

    nsXPIDLString startPage;
    browserHandler->GetDefaultArgs(getter_Copies(startPage));

    arg->SetData(startPage);

    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = OpenChromeWindow(0, browserLocation, "chrome,all,dialog=no",
                          arg, getter_AddRefs(newWindow));
  }

  // open a new compose window
  else if (aArgument.LowerCaseEqualsLiteral("composemessage")) {
    /*
     *  Here we change to OpenChromeWindow instead of OpenURL so as to
     *  pass argument values to the compose window, especially attachments
     */
    const char * composeLocation;
    rv = GetComposeLocation(&composeLocation);
    if (rv != NS_OK)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMWindow> newWindow;
    rv = OpenChromeWindow(0, composeLocation, "chrome,all,dialog=no",
                          arg, getter_AddRefs(newWindow));
  }

  // open a new calendar window
  else if (aArgument.LowerCaseEqualsLiteral("opencalendar")) {

    // check to see if it's already running
    nsCOMPtr<nsIDOMWindowInternal> aWindow;

    rv = FindWindow(NS_LITERAL_STRING("calendarMainWindow").get(),
		    getter_AddRefs(aWindow));

    if (NS_FAILED(rv))
      return rv;

    // focus the window if it was found
    if (aWindow) {
      aWindow->Focus();
    }

    // otherwise open a new calendar window
    else {
      nsXPIDLCString calendarChrome;
      rv = GetCalendarLocation(getter_Copies(calendarChrome));
      if (NS_FAILED(rv))
        return rv;

      nsCOMPtr<nsIDOMWindow> newWindow;
      rv = OpenChromeWindow(0, calendarChrome, "chrome,all,dialog=no",
                            arg, getter_AddRefs(newWindow));
    }
  }

  return rv;
}

nsresult
XRemoteService::FindWindow(const PRUnichar *aType,
			   nsIDOMWindowInternal **_retval)
{
  nsCOMPtr<nsIWindowMediator> mediator;
  mediator = do_GetService(NS_WINDOWMEDIATOR_CONTRACTID);

  if (!mediator)
    return NS_ERROR_FAILURE;

  return mediator->GetMostRecentWindow(aType, _retval);
}

NS_GENERIC_FACTORY_CONSTRUCTOR(XRemoteService)

static const nsModuleComponentInfo components[] = {
  { "XRemoteService",
    NS_XREMOTESERVICE_CID,
    "@mozilla.org/browser/xremoteservice;2",
    XRemoteServiceConstructor }
};

NS_IMPL_NSGETMODULE(XRemoteServiceModule, components)
