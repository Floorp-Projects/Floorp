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
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include <X11/Xatom.h> // for XA_STRING
#include <stdlib.h>
#include "nsGtkMozRemoteHelper.h"
#include "nsCRT.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"
#include "nsIXULWindow.h"
#include "nsIDocShell.h"
#include "nsProxiedService.h"
#include "nsIInterfaceRequestor.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWindowMediator.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsIPref.h"
#include "nsIAllocator.h"
#include "nsXPIDLString.h"

#ifdef MOZ_MAIL_NEWS

#include "nsMsgCompCID.h"
#include "nsIMsgCompose.h"
#include "nsIMsgComposeService.h"

#endif /* MOZ_MAIL_NEWS */

#define MOZILLA_VERSION_PROP   "_MOZILLA_VERSION"
#define MOZILLA_LOCK_PROP      "_MOZILLA_LOCK"
#define MOZILLA_COMMAND_PROP   "_MOZILLA_COMMAND"
#define MOZILLA_RESPONSE_PROP  "_MOZILLA_RESPONSE"

Atom nsGtkMozRemoteHelper::sMozVersionAtom  = 0;
Atom nsGtkMozRemoteHelper::sMozLockAtom     = 0;
Atom nsGtkMozRemoteHelper::sMozCommandAtom  = 0;
Atom nsGtkMozRemoteHelper::sMozResponseAtom = 0;

// XXX get this dynamically
static char *sRemoteVersion          = "5.0";
// protocol strings
static char *s200ExecutedCommand     = "200 executed command:";
static char *s500ParseCommand        = "500 command not parsable:";
static char *s501UnrecognizedCommand = "501 unrecognized command:";
// not using this yet...
// static char *s502NoWindow            = "502 no appropriate window for:";
static char *s509InternalError       = "509 internal error";

static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);

nsGtkMozRemoteHelper::nsGtkMozRemoteHelper()
{
  EnsureAtoms();
}

void
nsGtkMozRemoteHelper::SetupVersion(GdkWindow *aWindow)
{
  Window window;
  unsigned char *data = (unsigned char *)sRemoteVersion;
  EnsureAtoms();
  window = GDK_WINDOW_XWINDOW(aWindow);

  XChangeProperty(GDK_DISPLAY(), window, sMozVersionAtom, XA_STRING,
		  8, PropModeReplace, data, strlen(sRemoteVersion));

}

gboolean
nsGtkMozRemoteHelper::HandlePropertyChange(GtkWidget *aWidget, GdkEventProperty *aEvent)
{

  EnsureAtoms();

  // see if this is the command atom and it's new
  if (aEvent->state == GDK_PROPERTY_NEW_VALUE && 
      aEvent->window == aWidget->window &&
      aEvent->atom == sMozCommandAtom)
  {
    int result;
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    char *data = 0;

    result = XGetWindowProperty (GDK_DISPLAY(),
				 GDK_WINDOW_XWINDOW(aWidget->window),
				 sMozCommandAtom,
				 0,                        /* long_offset */
				 (65536 / sizeof (long)),  /* long_length */
				 True,                     /* atomic delete after */
				 XA_STRING,                /* req_type */
				 &actual_type,             /* actual_type return */
				 &actual_format,           /* actual_format_return */
				 &nitems,                  /* nitems_return */
				 &bytes_after,             /* bytes_after_return */
				 (unsigned char **)&data); /* prop_return
							      ( we only care about 
							      the first ) */
    if (result != Success)
    {
      // failed to get property off the window
      return FALSE;
    }
    else if (!data || !*data)
    {
      // failed to get the data off the window or it was the wrong
      // type
      return FALSE;
    }
    // cool, we got the property data.
    char *response = NULL;
    // The reason that we are using a conditional free here is that if
    // ParseCommand() fails below it's probably because of failed
    // memory allocations.  If that's the case we want to make sure
    // that we try to get something back to the client instead of just
    // giving up since a lot of clients will just hang forever.  Hence
    // using a static string.
    PRBool freeResponse = PR_TRUE;
    // parse the command
    ParseCommand(data, &response);
    if (!response)
    {
      response = "500 error parsing command";
      freeResponse = PR_FALSE;
    }
    // put the property onto the window as the response
    XChangeProperty (GDK_DISPLAY(), GDK_WINDOW_XWINDOW(aWidget->window),
		     sMozResponseAtom, XA_STRING,
		     8, PropModeReplace, (const unsigned char *)response, strlen (response));
    if (freeResponse)
      nsCRT::free(response);
    XFree(data);
    return TRUE;
  }
  else if (aEvent->state == GDK_PROPERTY_NEW_VALUE && 
	   aEvent->window == aWidget->window &&
	   aEvent->atom == sMozResponseAtom)
  {
    // client accepted the response.  party on wayne.
    return TRUE;
  }
  else if (aEvent->state == GDK_PROPERTY_NEW_VALUE && 
	   aEvent->window == aWidget->window &&
	   aEvent->atom == sMozLockAtom)
  {
    // someone locked the window
    return TRUE;
  }
  return FALSE;
}

void
nsGtkMozRemoteHelper::EnsureAtoms(void)
{
 // init our atoms if we need to
  if (!sMozVersionAtom)
    sMozVersionAtom = XInternAtom(GDK_DISPLAY(), MOZILLA_VERSION_PROP, False);
  if (!sMozLockAtom)
    sMozLockAtom = XInternAtom(GDK_DISPLAY(), MOZILLA_LOCK_PROP, False);
  if (!sMozCommandAtom)
    sMozCommandAtom = XInternAtom(GDK_DISPLAY(), MOZILLA_COMMAND_PROP, False);
  if (!sMozResponseAtom)
    sMozResponseAtom = XInternAtom(GDK_DISPLAY(), MOZILLA_RESPONSE_PROP, False);

}


nsGtkMozRemoteHelper::~nsGtkMozRemoteHelper()
{
}

void
nsGtkMozRemoteHelper::ParseCommand(const char *aCommand, char **aResponse)
{
  PRBool         newWindow = PR_FALSE;
  PRBool         raiseWindow = PR_TRUE;
  nsCString      actionString;
  nsCString      commandString;
  nsCString      origString;
  nsCString      lastCommand;
  PRInt32        begin_command = 0;
  PRInt32        end_command = 0;
  PRInt32        commandLen = 0;
  PRUint32       indexRet = 0;

  if (!aResponse)
  {
    // have to just return silently
    return;
  }

  // make sure that we return at least a null string
  *aResponse = 0;
  
  // check to make sure that we have our command
  if (!aCommand)
  {
    *aResponse = nsCRT::strdup(s509InternalError);
    return;
  }
  // make a copy of the string to work with
  origString = aCommand;
  
  // check to make sure that was allocated properly
  if (origString.IsEmpty())
  {
    // hey, this might fail to.  but that's ok, the caller will handle
    // it if it is null anyway.
    *aResponse = nsCRT::strdup(s509InternalError);
    return;
  }

  // should start with a '(' char
  begin_command = origString.FindChar('(');
  // make sure that it's there and it's not the first character
  if (begin_command == kNotFound || begin_command == 0)
  {
    *aResponse = BuildResponse(s500ParseCommand, origString.GetBuffer());
    return;
  }

  // should end with a ')' char after the '(' char
  end_command = origString.RFindChar(')');
  if (end_command == kNotFound || end_command < begin_command)
  {
    *aResponse = BuildResponse(s500ParseCommand, origString.GetBuffer());
    return;
  }

  // trunc the end of command
  origString.Truncate(end_command);
  
  // get the action type
  actionString.Append(origString, begin_command);
  if (origString.Length() > 0)
    origString.Cut(0, begin_command + 1);
  commandString.Append(origString);

  // convert the action to lower case and remove whitespace
  actionString.Trim(" ", PR_TRUE, PR_TRUE);
  actionString.ToLowerCase();

  // strip off whitespace from the command
  commandString.Trim(" ", PR_TRUE, PR_TRUE);

  commandLen = commandString.Length();

  // pop off the "noraise" argument if it exists.

  FindLastInList(commandString, lastCommand, &indexRet);
  if (lastCommand.EqualsIgnoreCase("noraise"))
  {
    commandString.Truncate(indexRet);
    raiseWindow = PR_FALSE;
  }

  /*   
      openURL ( ) 
            Prompts for a URL with a dialog box. 
      openURL (URL) 
            Opens the specified document without prompting. 
      openURL (URL, new-window) 
            Create a new window displaying the the specified document. 
  */

  nsresult rv;

  if (actionString.Equals("openurl"))
  {
    if (commandLen == 0)
    {
      rv = OpenURLDialog();
    }
    else
    {
      // check to see if we need to open a new window
      FindLastInList (commandString, lastCommand, &indexRet);
      if (lastCommand.EqualsIgnoreCase("new-window"))
      {
	commandString.Truncate(indexRet);
	newWindow = PR_TRUE;
      }
      // ok, do it
	  rv = OpenURL(commandString.GetBuffer(), newWindow, raiseWindow);
    }
  }

  /*
      openFile ( ) 
            Prompts for a file with a dialog box. 
      openFile (File) 
            Opens the specified file without prompting. 

  */

  else if (actionString.Equals("openfile"))
  {
    if (commandLen == 0)
    {
      rv = NS_ERROR_NOT_IMPLEMENTED;
    }
    else
    {
      rv = OpenFile(commandString.GetBuffer(), raiseWindow);
    }
  }

  /*
      saveAs ( ) 
            Prompts for a file with a dialog box (like the menu item). 
      saveAs (Output-File) 
            Writes HTML to the specified file without prompting. 
      saveAs (Output-File, Type) 
            Writes to the specified file with the type specified - the type may be HTML, Text, or PostScript. 

  */

  else if (actionString.Equals("saveas"))
  {
    if (commandLen == 0)
    {
      rv = NS_ERROR_NOT_IMPLEMENTED;
    }
    else
    {
      // check to see if it has a type on it
      FindLastInList(commandString, lastCommand, &indexRet);
      if (lastCommand.EqualsIgnoreCase("html"))
      {
	commandString.Truncate(indexRet);
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
      else if (lastCommand.EqualsIgnoreCase("text", PR_TRUE))
      {
	commandString.Truncate(indexRet);
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
      else if (lastCommand.EqualsIgnoreCase("postscript", PR_TRUE))
      {
	commandString.Truncate(indexRet);
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
      else
      {
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

  else if (actionString.Equals("mailto"))
  {
    // convert it to unicode
    nsString toList;
    if (commandLen == 0)
    {
      rv = MailTo(nsnull);
    }
    else
    {
      // convert it to unicode
      toList.AppendWithConversion(commandString);
      rv = MailTo(toList.GetUnicode());
    }
  }

  /*
      addBookmark ( ) 
            Adds the current document to the bookmark list. 
      addBookmark (URL) 
            Adds the given document to the bookmark list. 
      addBookmark (URL, Title) 
            Adds the given document to the bookmark list, with the given title. 
  */

  else if (actionString.Equals("addbookmark"))
  {
    if (commandLen == 0)
    {
      rv = NS_ERROR_NOT_IMPLEMENTED;
    }
    else
    {
      FindLastInList(commandString, lastCommand, &indexRet);
      if (!lastCommand.IsEmpty())
      {
	nsCAutoString title(lastCommand);
	commandString.Truncate(indexRet);
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
      else
      {
	rv = NS_ERROR_NOT_IMPLEMENTED;
      }
    }
  }
  
  else
  {
    rv = NS_ERROR_FAILURE;
    *aResponse = BuildResponse(s501UnrecognizedCommand, aCommand);
  }

  // if we failed and *aResponse isn't already filled in, fill it in
  // with the generic internal error message
  if (NS_FAILED(rv))
  {
    if (!*aResponse)
    {
      if (rv == NS_ERROR_NOT_IMPLEMENTED)
	*aResponse = BuildResponse(s501UnrecognizedCommand, aCommand);
      else
	*aResponse = nsCRT::strdup(s509InternalError);
    }
  }
  
  // if we got this far everything worked.  make sure to use aCommand
  // since origString might have been messed with.
  if (!*aResponse)
    *aResponse = BuildResponse(s200ExecutedCommand, aCommand);
  return;
}

void
nsGtkMozRemoteHelper::FindLastInList(nsCString &aString, nsCString &retString, PRUint32 *aIndexRet)
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

char *
nsGtkMozRemoteHelper::BuildResponse   (const char *aError, const char *aMessage)
				   {
  nsCString retvalString;
  char *retval;

  // check to make sure that we have the minimum for allocating this
  // buffer
  if (!aError || !aMessage)
    return NULL;

  retvalString.Append(aError);
  retvalString.Append(" ");
  retvalString.Append(aMessage);

  retval = retvalString.ToNewCString();
  return retval;
}

NS_METHOD
nsGtkMozRemoteHelper::OpenURLDialog  (void)
{
  nsresult rv;
  nsCOMPtr<nsIDOMWindowInternal> lastWindow;
  nsString name;
  nsString url;
  
  name.AssignWithConversion("_blank");
  // get the last used browser window
  rv = GetLastBrowserWindow(getter_AddRefs(lastWindow));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  // use it to open the open location dialog
  rv = OpenXULWindow ("chrome://navigator/content/openLocation.xul",
		      lastWindow,
		      "chrome,modal",
		      name.GetUnicode(),
		      url.GetUnicode());
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_METHOD
nsGtkMozRemoteHelper::OpenURL        (const char *aURL, PRBool aNewWindow, PRBool raiseWindow)
{
  nsresult rv;
  nsString newURL;
  nsString name;
  nsCString navChromeURL;
  nsXPIDLCString tempString;
  NS_WITH_SERVICE(nsIPref, prefs, "component://netscape/preferences", &rv);
  if (NS_SUCCEEDED(rv))
    prefs->CopyCharPref("browser.chromeURL", getter_Copies(tempString));
  // make a copy for the auto string
  if (tempString)
    navChromeURL = tempString;
  // default to this
  if (navChromeURL.IsEmpty())
    navChromeURL = "chrome://navigator/content/navigator.xul";

  newURL.AssignWithConversion(aURL);

  if (aNewWindow)
  {
    nsCOMPtr<nsIDOMWindowInternal> parentWindow;
    rv = GetHiddenWindow(getter_AddRefs(parentWindow));
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;

    rv = OpenXULWindow(navChromeURL, parentWindow,
		       "chrome,all,dialog=no",
		       name.GetUnicode(), newURL.GetUnicode());
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
  }
  else
  {
    nsCOMPtr<nsIDOMWindowInternal>          lastUsedWindow;
    nsCOMPtr<nsIDOMWindowInternal>          innerWindow;
    nsCOMPtr<nsIScriptGlobalObject> globalObject;
    nsCOMPtr<nsIDocShell>           docShell;
    nsCOMPtr<nsIURI>                uri;
    // get the most recently used window
    rv = GetLastBrowserWindow(getter_AddRefs(lastUsedWindow));
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    // get the content area for that window
    rv = lastUsedWindow->Get_content(getter_AddRefs(innerWindow));
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    // get the script global object for that window
    globalObject = do_QueryInterface(innerWindow);
    if (!globalObject)
      return NS_ERROR_FAILURE;
    // get the docshell for that window
    globalObject->GetDocShell(getter_AddRefs(docShell));
    if (!docShell)
      return NS_ERROR_FAILURE;
    // load the url
    rv = NS_NewURI(getter_AddRefs(uri), aURL);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    // go, baby, go!
    rv = docShell->LoadURI(uri, nsnull);
    if (NS_FAILED(rv))
      return NS_ERROR_FAILURE;
    // raise the window, if requested
    if (raiseWindow)
    {
      rv = lastUsedWindow->GetAttention();
      if (NS_FAILED(rv))
        return NS_ERROR_FAILURE;
    }
  }
  return NS_OK;
}

NS_METHOD
nsGtkMozRemoteHelper::OpenFileDialog (void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsGtkMozRemoteHelper::OpenFile       (const char *aURL, PRBool raiseWindow)
{
  nsCString newURL;
  nsresult rv;
  // check to see if the url starts with a file:/// string
  if (PL_strncasecmp(aURL, "file:///", 8) != 0)
  {
    // ok, we don't have a file:/// url yet.  Munge it to where it
    // looks right.
    // check for file:/<filename>
    if (PL_strncasecmp(aURL, "file:/", 6) == 0)
    {
      // netlib seems to handle this case properly
      newURL = aURL;
    }
    // ooook.  let it fail.
    else if (PL_strncasecmp(aURL, "file:", 5) == 0)
    {
      newURL = aURL;
    }
    // doesn't match anything.  slap a file: extension on the front
    // and hope for the best
    else
    {
      newURL.Append("file:");
      newURL.Append(aURL);
    }
  }
  else
  {
    newURL = aURL;
  }
  // open with the new url
  rv = OpenURL(newURL, PR_FALSE, raiseWindow);
  return rv;
}

NS_METHOD
nsGtkMozRemoteHelper::SaveAsDialog   (void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsGtkMozRemoteHelper::SaveAs         (const char *aFileName, const char *aType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsGtkMozRemoteHelper::MailTo         (const PRUnichar *aToList)
{

#ifdef MOZ_MAIL_NEWS
  nsresult rv;
  // get the messenger compose service
  NS_WITH_SERVICE(nsIMsgComposeService, composeService,
		  "component://netscape/messengercompose", &rv);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  if (aToList)
  {
    rv = composeService->OpenComposeWindowWithValues(nsnull,
						     /* msg compose window url */
						     nsIMsgCompType::New,
						     /* new message */
						     nsIMsgCompFormat::Default,
						     /* default of html/plain text */
						     aToList,
						     /* the to: list */
						     nsnull,
						     /* cc: */
						     nsnull,
						     /* bcc: */
						     nsnull,
						     /* newsgroups */
						     nsnull,
						     /* subject */
						     nsnull,
						     /* body */
						     nsnull,
						     /* attachment */
						     nsnull); /* identity */
  }
  else
  {
    rv = composeService->OpenComposeWindow(nsnull,
					   /* msg compose window url */
					   nsnull,
					   /* original msg uri */
					   nsIMsgCompType::New,
					   /* new msg */
					   nsIMsgCompFormat::Default,
					   /* default of html/plain text */
					   nsnull); /* identity */
  }
					   
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif /* MOZ_MAIL_NEWS */

}

NS_METHOD
nsGtkMozRemoteHelper::AddBookmark    (const char *aURL, const char *aTitle)
{
  return NS_OK;
}

NS_METHOD
nsGtkMozRemoteHelper::GetHiddenWindow (nsIDOMWindowInternal **_retval)
{
  nsresult rv;
  NS_WITH_PROXIED_SERVICE(nsIAppShellService, appShell,
			  kAppShellServiceCID, nsnull, &rv);
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIXULWindow> xulWindow;
  rv = appShell->GetHiddenWindow(getter_AddRefs(xulWindow));
  if(NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDocShell> docShell;
  rv = xulWindow->GetDocShell(getter_AddRefs(docShell));
  if(NS_FAILED(rv))
    return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMWindowInternal> domWindow(do_GetInterface(docShell));
  *_retval = domWindow;
  NS_IF_ADDREF(*_retval);

  return NS_OK;
}

NS_METHOD nsGtkMozRemoteHelper::GetLastBrowserWindow (nsIDOMWindowInternal **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  nsresult rv;
  nsString browserString;
  nsCOMPtr<nsIDOMWindowInternal> outerWindow;
  browserString.AssignWithConversion("navigator:browser");
  
  // get the window mediator service
  NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  // find the most recently used window
  rv = windowMediator->GetMostRecentWindow(browserString.GetUnicode(), getter_AddRefs(outerWindow));
  if (NS_FAILED(rv))
    return rv;
  *_retval = outerWindow.get();
  NS_ADDREF(*_retval);
  return NS_OK;
}

NS_METHOD nsGtkMozRemoteHelper::OpenXULWindow (const char *aChromeURL, nsIDOMWindowInternal *aParent,
					       const char *aWindowFeatures,
					       const PRUnichar *aName, const PRUnichar *aURL)
{
  NS_ENSURE_ARG_POINTER(aChromeURL);
  NS_ENSURE_ARG_POINTER(aWindowFeatures);
  NS_ENSURE_ARG_POINTER(aParent);

  nsCOMPtr<nsIDOMWindowInternal> returnedWindow;
  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObj = do_QueryInterface(aParent);
  JSContext *jsContext = nsnull;

  if (!scriptGlobalObj)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptContext> scriptcx;
  scriptGlobalObj->GetContext(getter_AddRefs(scriptcx));
  if (scriptcx)
    jsContext = (JSContext *)scriptcx->GetNativeContext();

  if (!jsContext)
    return NS_ERROR_FAILURE;
  
  // make sure that we pass a valid name even if there isn't one
  // passed in.
  const PRUnichar *name;
  const PRUnichar *url;

  static PRUnichar kEmpty[] = { PRUnichar(0) };
  name = aName ? aName : kEmpty;
  url = aURL ? aURL : kEmpty;

  jsval *jsargv;
  void *mark;

  jsargv = JS_PushArguments(jsContext, &mark, "sWsW", aChromeURL, name,
			    aWindowFeatures, url);

  if (!jsargv)
    return NS_ERROR_FAILURE;
  
  nsresult rv;

  rv = aParent->OpenDialog(jsContext, jsargv, 4, getter_AddRefs(returnedWindow));
  JS_PopArguments(jsContext, mark);

  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  return NS_OK;
}
