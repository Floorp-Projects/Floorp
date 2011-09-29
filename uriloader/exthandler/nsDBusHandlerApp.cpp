/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:expandtab:shiftwidth=2:tabstop=2:cin:
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is 
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozila.com>
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

#include  <dbus/dbus.h>
#include "nsDBusHandlerApp.h"
#include "nsIURI.h"
#include "nsIClassInfoImpl.h"
#include "nsCOMPtr.h"
#include "nsCExternalHandlerService.h"

#if (MOZ_PLATFORM_MAEMO == 5)
#define APP_LAUNCH_BANNER_SERVICE           "com.nokia.hildon-desktop"
#define APP_LAUNCH_BANNER_METHOD_INTERFACE  "com.nokia.hildon.hdwm.startupnotification"
#define APP_LAUNCH_BANNER_METHOD_PATH       "/com/nokia/hildon/hdwm"
#define APP_LAUNCH_BANNER_METHOD            "starting"
#endif


// XXX why does nsMIMEInfoImpl have a threadsafe nsISupports?  do we need one 
// here too?
NS_IMPL_CLASSINFO(nsDBusHandlerApp, NULL, 0, NS_DBUSHANDLERAPP_CID)
NS_IMPL_ISUPPORTS2_CI(nsDBusHandlerApp, nsIDBusHandlerApp, nsIHandlerApp)

////////////////////////////////////////////////////////////////////////////////
//// nsIHandlerApp

NS_IMETHODIMP nsDBusHandlerApp::GetName(nsAString& aName)
{
  aName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP nsDBusHandlerApp::SetName(const nsAString & aName)
{
  mName.Assign(aName);
  return NS_OK;
}

NS_IMETHODIMP nsDBusHandlerApp::SetDetailedDescription(const nsAString & aDescription)
{
  mDetailedDescription.Assign(aDescription);

  return NS_OK;
}

NS_IMETHODIMP nsDBusHandlerApp::GetDetailedDescription(nsAString& aDescription)
{
  aDescription.Assign(mDetailedDescription);
  
  return NS_OK;
}

NS_IMETHODIMP
nsDBusHandlerApp::Equals(nsIHandlerApp *aHandlerApp, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(aHandlerApp);
  
  // If the handler app isn't a dbus handler app, then it's not the same app.
  nsCOMPtr<nsIDBusHandlerApp> dbusHandlerApp = do_QueryInterface(aHandlerApp);
  if (!dbusHandlerApp) {
    *_retval = PR_FALSE;
    return NS_OK;
  }
  nsCAutoString service;
  nsCAutoString method;
  
  nsresult rv = dbusHandlerApp->GetService(service);
  if (NS_FAILED(rv)) {
    *_retval = PR_FALSE;
    return NS_OK;
  }
  rv = dbusHandlerApp->GetMethod(method);
  if (NS_FAILED(rv)) {
    *_retval = PR_FALSE;
    return NS_OK;
  }
  
  *_retval = service.Equals(mService) && method.Equals(mMethod);
  return NS_OK;
}

NS_IMETHODIMP
nsDBusHandlerApp::LaunchWithURI(nsIURI *aURI,
                                nsIInterfaceRequestor *aWindowContext)
{
  nsCAutoString spec;
  nsresult rv = aURI->GetAsciiSpec(spec);
  NS_ENSURE_SUCCESS(rv,rv);
  const char* uri = spec.get(); 
  
  DBusError err;
  dbus_error_init(&err);
  
  DBusConnection  *connection;
  connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
  if (dbus_error_is_set(&err)) { 
    dbus_error_free(&err); 
    return NS_ERROR_FAILURE;
  }
  if (nsnull == connection) { 
    return NS_ERROR_FAILURE; 
  }
  dbus_connection_set_exit_on_disconnect(connection,false);
  
  DBusMessage* msg;
  msg = dbus_message_new_method_call(mService.get(), 
                                     mObjpath.get(), 
                                     mInterface.get(), 
                                     mMethod.get());
  
  if (!msg) {
    return NS_ERROR_FAILURE;
  }
  dbus_message_set_no_reply(msg, PR_TRUE);
  
  DBusMessageIter iter;
  dbus_message_iter_init_append(msg, &iter);
  dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &uri);
  
  if (dbus_connection_send(connection, msg, NULL)) {
    dbus_connection_flush(connection);
    dbus_message_unref(msg);
#if (MOZ_PLATFORM_MAEMO == 5)
    msg = dbus_message_new_method_call (APP_LAUNCH_BANNER_SERVICE,
                                        APP_LAUNCH_BANNER_METHOD_PATH,
                                        APP_LAUNCH_BANNER_METHOD_INTERFACE,
                                        APP_LAUNCH_BANNER_METHOD);
    
    if (msg) {
      const char* service = mService.get();
      if (dbus_message_append_args(msg,
                                   DBUS_TYPE_STRING, 
                                   &service,
                                   DBUS_TYPE_INVALID)) {
        if (dbus_connection_send(connection, msg, NULL)) {
          dbus_connection_flush(connection);
        }
        dbus_message_unref(msg);
      }
    }
#endif
  } else {
    dbus_message_unref(msg);
    return NS_ERROR_FAILURE;		    
  }
  return NS_OK;
  
}

////////////////////////////////////////////////////////////////////////////////
//// nsIDBusHandlerApp

/* attribute AUTF8String service; */
NS_IMETHODIMP nsDBusHandlerApp::GetService(nsACString & aService)
{
  aService.Assign(mService);
  return NS_OK;
}

NS_IMETHODIMP nsDBusHandlerApp::SetService(const nsACString & aService)
{
  mService.Assign(aService);
  return NS_OK;
}

/* attribute AUTF8String method; */
NS_IMETHODIMP nsDBusHandlerApp::GetMethod(nsACString & aMethod)
{
  aMethod.Assign(mMethod);
  return NS_OK;
}

NS_IMETHODIMP nsDBusHandlerApp::SetMethod(const nsACString & aMethod)
{
  mMethod.Assign(aMethod);
  return NS_OK;
}

/* attribute AUTF8String interface; */
NS_IMETHODIMP nsDBusHandlerApp::GetDBusInterface(nsACString & aInterface)
{
  aInterface.Assign(mInterface);
  return NS_OK;
}

NS_IMETHODIMP nsDBusHandlerApp::SetDBusInterface(const nsACString & aInterface)
{
  mInterface.Assign(aInterface);
  return NS_OK;
}

/* attribute AUTF8String objpath; */
NS_IMETHODIMP nsDBusHandlerApp::GetObjectPath(nsACString & aObjpath)
{
  aObjpath.Assign(mObjpath);
  return NS_OK;
}

NS_IMETHODIMP nsDBusHandlerApp::SetObjectPath(const nsACString & aObjpath)
{
  mObjpath.Assign(aObjpath);
  return NS_OK;
}


