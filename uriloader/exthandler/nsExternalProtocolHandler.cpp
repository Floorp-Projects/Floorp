/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sts=2 sw=2 et cin:
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
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

#include "nsIURI.h"
#include "nsIURL.h"
#include "nsExternalProtocolHandler.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsIServiceManagerUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIStringBundle.h"
#include "nsIPrefService.h"
#include "nsIPrompt.h"
#include "nsEventQueueUtils.h"
#include "nsIChannel.h"
#include "nsNetCID.h"
#include "netCore.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"

static NS_DEFINE_CID(kSimpleURICID, NS_SIMPLEURI_CID);
static const char kExternalProtocolPrefPrefix[] = "network.protocol-handler.external.";



////////////////////////////////////////////////////////////////////////
// a stub channel implemenation which will map calls to AsyncRead and OpenInputStream
// to calls in the OS for loading the url.
////////////////////////////////////////////////////////////////////////

class nsExtProtocolChannel : public nsIChannel
{
public:

    NS_DECL_ISUPPORTS
    NS_DECL_NSICHANNEL
    NS_DECL_NSIREQUEST

    nsExtProtocolChannel();
    virtual ~nsExtProtocolChannel();

    nsresult SetURI(nsIURI*);

    nsresult OpenURL();

private:
    nsCOMPtr<nsIURI> mUrl;
    nsCOMPtr<nsIURI> mOriginalURI;
    nsresult mStatus;

    nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
    PRBool PromptForScheme(nsIURI *aURI);
};

NS_IMPL_THREADSAFE_ADDREF(nsExtProtocolChannel)
NS_IMPL_THREADSAFE_RELEASE(nsExtProtocolChannel)

NS_INTERFACE_MAP_BEGIN(nsExtProtocolChannel)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
   NS_INTERFACE_MAP_ENTRY(nsIChannel)
   NS_INTERFACE_MAP_ENTRY(nsIRequest)
NS_INTERFACE_MAP_END_THREADSAFE

nsExtProtocolChannel::nsExtProtocolChannel() : mStatus(NS_OK)
{
}

nsExtProtocolChannel::~nsExtProtocolChannel()
{}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadGroup(nsILoadGroup * *aLoadGroup)
{
    *aLoadGroup = nsnull;
    return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadGroup(nsILoadGroup * aLoadGroup)
{
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetNotificationCallbacks(nsIInterfaceRequestor* *aNotificationCallbacks)
{
  NS_PRECONDITION(aNotificationCallbacks, "No out param?");
  *aNotificationCallbacks = mCallbacks;
  NS_IF_ADDREF(*aNotificationCallbacks);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetNotificationCallbacks(nsIInterfaceRequestor* aNotificationCallbacks)
{
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP 
nsExtProtocolChannel::GetSecurityInfo(nsISupports * *aSecurityInfo)
{
  *aSecurityInfo = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetOriginalURI(nsIURI* *aURI)
{
  NS_IF_ADDREF(*aURI = mOriginalURI);
  return NS_OK; 
}
 
NS_IMETHODIMP nsExtProtocolChannel::SetOriginalURI(nsIURI* aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}
 
NS_IMETHODIMP nsExtProtocolChannel::GetURI(nsIURI* *aURI)
{
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK; 
}
 
nsresult nsExtProtocolChannel::SetURI(nsIURI* aURI)
{
  mUrl = aURI;
  return NS_OK; 
}
 
PRBool nsExtProtocolChannel::PromptForScheme(nsIURI* aURI)
{
  // deny the load if we aren't able to ask but prefs say we should
  nsCAutoString scheme;
  nsresult rv = aURI->GetScheme(scheme);
  if (!mCallbacks) {
    NS_WARNING("Notification Callbacks not set!");
    return PR_FALSE;
  }

  nsCOMPtr<nsIPrompt> prompt = do_GetInterface(mCallbacks);
  if (!prompt) {
    NS_WARNING("No prompt interface on channel");
    return PR_FALSE;
  }

  nsCOMPtr<nsIStringBundleService> sbSvc(do_GetService(NS_STRINGBUNDLE_CONTRACTID));
  if (!sbSvc) {
    NS_WARNING("Couldn't load StringBundleService");
    return PR_FALSE;
  }

  nsXPIDLString desc;
  nsCOMPtr<nsIExternalProtocolService> extProtService (do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
  if (extProtService)
    extProtService->GetApplicationDescription(scheme, desc);

  nsCOMPtr<nsIStringBundle> appstrings;
  rv = sbSvc->CreateBundle("chrome://global/locale/appstrings.properties",
                           getter_AddRefs(appstrings));
  if (NS_FAILED(rv) || !appstrings) {
    NS_WARNING("Failed to create appstrings.properties bundle");
    return PR_FALSE;
  }

  nsCAutoString spec;
  aURI->GetSpec(spec);
  NS_ConvertUTF8toUTF16 uri(spec);
  NS_ConvertUTF8toUTF16 utf16scheme(scheme);

  nsXPIDLString title;
  appstrings->GetStringFromName(NS_LITERAL_STRING("externalProtocolTitle").get(),
                                getter_Copies(title));
  nsXPIDLString checkMsg;
  appstrings->GetStringFromName(NS_LITERAL_STRING("externalProtocolChkMsg").get(),
                                getter_Copies(checkMsg));
  nsXPIDLString launchBtn;
  appstrings->GetStringFromName(NS_LITERAL_STRING("externalProtocolLaunchBtn").get(),
                                getter_Copies(launchBtn));

  if (desc.IsEmpty())
    appstrings->GetStringFromName(NS_LITERAL_STRING("externalProtocolUnknown").get(),
                                  getter_Copies(desc));


  nsXPIDLString message;
  const PRUnichar* msgArgs[] = { utf16scheme.get(), uri.get(), desc.get() };
  appstrings->FormatStringFromName(NS_LITERAL_STRING("externalProtocolPrompt").get(),
                                   msgArgs,
                                   NS_ARRAY_LENGTH(msgArgs),
                                   getter_Copies(message));

  if (utf16scheme.IsEmpty() || uri.IsEmpty() || title.IsEmpty() ||
      checkMsg.IsEmpty() || launchBtn.IsEmpty() || message.IsEmpty() ||
      desc.IsEmpty())
    return PR_FALSE;

  // all pieces assembled, now we can pose the dialog
  PRBool allowLoad = PR_FALSE;
  PRBool remember = PR_FALSE;
  PRInt32 choice = 1; // assume "cancel" in case of failure
  rv = prompt->ConfirmEx(title.get(), message.get(),
                         nsIPrompt::BUTTON_DELAY_ENABLE +
                         nsIPrompt::BUTTON_POS_1_DEFAULT +
                         (nsIPrompt::BUTTON_TITLE_IS_STRING * nsIPrompt::BUTTON_POS_0) +
                         (nsIPrompt::BUTTON_TITLE_CANCEL * nsIPrompt::BUTTON_POS_1),
                         launchBtn.get(), 0, 0, checkMsg.get(),
                         &remember, &choice);
  if (NS_SUCCEEDED(rv))
  {
    if (choice == 0)
      allowLoad = PR_TRUE;

    if (remember)
    {
      nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
      if (prefs)
      {
          nsCAutoString prefname(kExternalProtocolPrefPrefix);
          prefname += scheme;
          prefs->SetBoolPref(prefname.get(), allowLoad);
      }
    }
  }

  return allowLoad;
}

nsresult nsExtProtocolChannel::OpenURL()
{
  nsCOMPtr<nsIExternalProtocolService> extProtService (do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
  nsCAutoString urlScheme;
  mUrl->GetScheme(urlScheme);

  if (extProtService)
  {
#ifdef DEBUG
    PRBool haveHandler = PR_FALSE;
    extProtService->ExternalProtocolHandlerExists(urlScheme.get(), &haveHandler);
    NS_ASSERTION(haveHandler, "Why do we have a channel for this url if we don't support the protocol?");
#endif

    // Check that it's OK to hand this scheme off to the OS

    PRBool allowLoad    = PR_FALSE;
    nsCOMPtr<nsIPrefBranch> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));

    if (prefs)
    {
      // check whether it's explicitly approved or denied in prefs
      nsCAutoString schemePref(kExternalProtocolPrefPrefix);
      schemePref += urlScheme;

      nsresult rv = prefs->GetBoolPref(schemePref.get(), &allowLoad);

      if (NS_FAILED(rv))
      {
        // scheme not explicitly listed, what is the default action?
        const PRInt32 kExternalProtocolNever  = 0;
        const PRInt32 kExternalProtocolAlways = 1;
        const PRInt32 kExternalProtocolAsk    = 2;

        PRInt32 externalDefault = kExternalProtocolAsk;
        prefs->GetIntPref("network.protocol-handler.external-default",
                          &externalDefault);

        if (externalDefault == kExternalProtocolAlways)
        {
          // original behavior -- just do it
          allowLoad = PR_TRUE;
        }
        else if (externalDefault == kExternalProtocolAsk)
        {
          allowLoad = PromptForScheme(mUrl);
        }
      }
    }

    if (allowLoad)
      return extProtService->LoadUrl(mUrl);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsExtProtocolChannel::Open(nsIInputStream **_retval)
{
  OpenURL();
  return NS_ERROR_NO_CONTENT; // force caller to abort.
}

static void *PR_CALLBACK handleExtProtoEvent(PLEvent *event)
{
  nsExtProtocolChannel *channel =
      NS_STATIC_CAST(nsExtProtocolChannel*, PL_GetEventOwner(event));

  NS_ASSERTION(channel, "Where has the channel gone?");
  channel->OpenURL();

  return nsnull;
}

static void PR_CALLBACK destroyExtProtoEvent(PLEvent *event)
{
  nsExtProtocolChannel *channel =
      NS_STATIC_CAST(nsExtProtocolChannel*, PL_GetEventOwner(event));
  NS_ASSERTION(channel, "Where has the channel gone?");
  NS_RELEASE(channel);
  delete event;
}

NS_IMETHODIMP nsExtProtocolChannel::AsyncOpen(nsIStreamListener *listener, nsISupports *ctxt)
{
  nsCOMPtr<nsIEventQueue> eventQ;
  nsresult rv = NS_GetCurrentEventQ(getter_AddRefs(eventQ));
  if (NS_FAILED(rv))
    return rv;

  PLEvent *event = new PLEvent;
  if (event)
  {
    NS_ADDREF_THIS();
    PL_InitEvent(event, this, handleExtProtoEvent, destroyExtProtoEvent);

    rv = eventQ->PostEvent(event);
    if (NS_FAILED(rv))
      PL_DestroyEvent(event);
  }

  return NS_ERROR_NO_CONTENT; // force caller to abort.
}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadFlags(nsLoadFlags *aLoadFlags)
{
  *aLoadFlags = 0;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadFlags(nsLoadFlags aLoadFlags)
{
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentType(nsACString &aContentType)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentType(const nsACString &aContentType)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentCharset(nsACString &aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentCharset(const nsACString &aContentCharset)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentLength(PRInt32 * aContentLength)
{
  *aContentLength = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsExtProtocolChannel::SetContentLength(PRInt32 aContentLength)
{
  NS_NOTREACHED("SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::GetOwner(nsISupports * *aPrincipal)
{
  NS_NOTREACHED("GetOwner");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetOwner(nsISupports * aPrincipal)
{
  NS_NOTREACHED("SetOwner");
  return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsExtProtocolChannel::GetName(nsACString &result)
{
  NS_NOTREACHED("nsExtProtocolChannel::GetName");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::IsPending(PRBool *result)
{
  *result = PR_TRUE;
  return NS_OK; 
}

NS_IMETHODIMP nsExtProtocolChannel::GetStatus(nsresult *status)
{
  *status = mStatus;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::Cancel(nsresult status)
{
  mStatus = status;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::Suspend()
{
  NS_NOTREACHED("Suspend");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::Resume()
{
  NS_NOTREACHED("Resume");
  return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////
// the default protocol handler implementation
//////////////////////////////////////////////////////////////////////

nsExternalProtocolHandler::nsExternalProtocolHandler()
{
  m_schemeName = "default";
  m_extProtService = do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID);
}


nsExternalProtocolHandler::~nsExternalProtocolHandler()
{}

NS_IMPL_THREADSAFE_ADDREF(nsExternalProtocolHandler)
NS_IMPL_THREADSAFE_RELEASE(nsExternalProtocolHandler)

NS_INTERFACE_MAP_BEGIN(nsExternalProtocolHandler)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIProtocolHandler)
   NS_INTERFACE_MAP_ENTRY(nsIProtocolHandler)
   NS_INTERFACE_MAP_ENTRY(nsIExternalProtocolHandler)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMETHODIMP nsExternalProtocolHandler::GetScheme(nsACString &aScheme)
{
  aScheme = m_schemeName;
  return NS_OK;
}

NS_IMETHODIMP nsExternalProtocolHandler::GetDefaultPort(PRInt32 *aDefaultPort)
{
  *aDefaultPort = 0;
    return NS_OK;
}

NS_IMETHODIMP 
nsExternalProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}
// returns TRUE if the OS can handle this protocol scheme and false otherwise.
PRBool nsExternalProtocolHandler::HaveProtocolHandler(nsIURI * aURI)
{
  PRBool haveHandler = PR_FALSE;
  if (aURI)
  {
    nsCAutoString scheme;
    aURI->GetScheme(scheme);
    if (m_extProtService)
      m_extProtService->ExternalProtocolHandlerExists(scheme.get(), &haveHandler);
  }

  return haveHandler;
}

NS_IMETHODIMP nsExternalProtocolHandler::GetProtocolFlags(PRUint32 *aUritype)
{
    // Make it norelative since it is a simple uri
    *aUritype = URI_NORELATIVE;
    return NS_OK;
}

NS_IMETHODIMP nsExternalProtocolHandler::NewURI(const nsACString &aSpec,
                                                const char *aCharset, // ignore charset info
                                                nsIURI *aBaseURI,
                                                nsIURI **_retval)
{
  nsresult rv;
  nsCOMPtr<nsIURI> uri = do_CreateInstance(kSimpleURICID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = uri->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*_retval = uri);
  return NS_OK;
}

NS_IMETHODIMP nsExternalProtocolHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
  // only try to return a channel if we have a protocol handler for the url

  PRBool haveHandler = HaveProtocolHandler(aURI);
  if (haveHandler)
  {
    nsCOMPtr<nsIChannel> channel;
    NS_NEWXPCOM(channel, nsExtProtocolChannel);
    if (!channel) return NS_ERROR_OUT_OF_MEMORY;

    ((nsExtProtocolChannel*) channel.get())->SetURI(aURI);
    channel->SetOriginalURI(aURI);

    if (_retval)
    {
      *_retval = channel;
      NS_IF_ADDREF(*_retval);
      return NS_OK;
    }
  }

  return NS_ERROR_UNKNOWN_PROTOCOL;
}

///////////////////////////////////////////////////////////////////////
// External protocol handler interface implementation
//////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsExternalProtocolHandler::ExternalAppExistsForScheme(const nsACString& aScheme, PRBool *_retval)
{
  if (m_extProtService)
    return m_extProtService->ExternalProtocolHandlerExists(PromiseFlatCString(aScheme).get(), _retval);

  // In case we don't have external protocol service.
  *_retval = PR_FALSE;
  return NS_OK;
}

nsBlockedExternalProtocolHandler::nsBlockedExternalProtocolHandler()
{
    m_schemeName = "default-blocked";
}

NS_IMETHODIMP
nsBlockedExternalProtocolHandler::NewChannel(nsIURI *aURI,
                                             nsIChannel **_retval)
{
    *_retval = nsnull;
    return NS_ERROR_UNKNOWN_PROTOCOL;
}
