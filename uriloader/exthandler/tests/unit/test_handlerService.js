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
 * The Original Code is Content Preferences (cpref).
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Myk Melez <myk@mozilla.org>
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

function run_test() {
  // It doesn't matter whether or not this executable exists or is executable,
  // only that it'll QI to nsIFile and has a path attribute, which the service
  // expects.
  var executable = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsILocalFile);
  executable.initWithPath("/usr/bin/test");
  
  var localHandler = {
    name: "Local Handler",
    executable: executable,
    interfaces: [Ci.nsIHandlerApp, Ci.nsILocalHandlerApp, Ci.nsISupports],
    QueryInterface: function(iid) {
      if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
        throw Cr.NS_ERROR_NO_INTERFACE;
      return this;
    }
  };
  
  var webHandler = {
    name: "Web Handler",
    uriTemplate: "http://www.example.com/?%s",
    interfaces: [Ci.nsIHandlerApp, Ci.nsIWebHandlerApp, Ci.nsISupports],
    QueryInterface: function(iid) {
      if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
        throw Cr.NS_ERROR_NO_INTERFACE;
      return this;
    }
  };
  
  var handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].
                   getService(Ci.nsIHandlerService);

  var mimeSvc = Cc["@mozilla.org/uriloader/external-helper-app-service;1"].
                getService(Ci.nsIMIMEService);


  //**************************************************************************//
  // Default Properties

  // Get a handler info for a MIME type that neither the application nor
  // the OS knows about and make sure its properties are set to the proper
  // default values.

  var handlerInfo = mimeSvc.getFromTypeAndExtension("nonexistent/type", null);

  do_check_eq(handlerInfo.MIMEType, "nonexistent/type");

  // These three properties are the ones the handler service knows how to store.
  do_check_eq(handlerInfo.preferredAction, Ci.nsIHandlerInfo.saveToDisk);
  do_check_eq(handlerInfo.preferredApplicationHandler, null);
  do_check_true(handlerInfo.alwaysAskBeforeHandling);

  do_check_eq(handlerInfo.description, "");
  do_check_eq(handlerInfo.hasDefaultHandler, false);
  do_check_eq(handlerInfo.defaultDescription, "");


  //**************************************************************************//
  // Round-Trip Data Integrity

  // Test round-trip data integrity by setting the properties of the handler
  // info object to different values, telling the handler service to store the
  // object, and then retrieving a new info object for the same type and making
  // sure its properties are identical.

  handlerInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  handlerInfo.preferredApplicationHandler = localHandler;
  handlerInfo.alwaysAskBeforeHandling = false;

  handlerSvc.store(handlerInfo);

  handlerInfo = mimeSvc.getFromTypeAndExtension("nonexistent/type", null);

  do_check_eq(handlerInfo.preferredAction, Ci.nsIHandlerInfo.useHelperApp);

  do_check_neq(handlerInfo.preferredApplicationHandler, null);
  var preferredHandler = handlerInfo.preferredApplicationHandler;
  do_check_eq(typeof preferredHandler, "object");
  do_check_eq(preferredHandler.name, "Local Handler");
  var localHandler = preferredHandler.QueryInterface(Ci.nsILocalHandlerApp);
  do_check_eq(localHandler.executable.path, "/usr/bin/test");

  do_check_false(handlerInfo.alwaysAskBeforeHandling);

  // FIXME: test round trip integrity for a protocol.
  // FIXME: test round trip integrity for a handler info with a web handler.
}
