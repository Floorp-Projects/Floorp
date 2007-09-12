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
 *   Dan Mosedale <dmose@mozilla.org>
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
  //**************************************************************************//
  // Constants

  const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].
                     getService(Ci.nsIHandlerService);

  const mimeSvc = Cc["@mozilla.org/uriloader/external-helper-app-service;1"].
                  getService(Ci.nsIMIMEService);

  const prefSvc = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService);
                  
  const rootPrefBranch = prefSvc.getBranch("");
  
  //**************************************************************************//
  // Sample Data

  // It doesn't matter whether or not this nsIFile is actually executable,
  // only that it has a path and exists.  Since we don't know any executable
  // that exists on all platforms (except possibly the application being
  // tested, but there doesn't seem to be a way to get a reference to that
  // from the directory service), we use the temporary directory itself.
  var executable = HandlerServiceTest._dirSvc.get("TmpD", Ci.nsIFile);
  // XXX We could, of course, create an actual executable in the directory:
  //executable.append("localhandler");
  //if (!executable.exists())
  //  executable.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0755);

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
  
  var webHandler = Cc["@mozilla.org/uriloader/web-handler-app;1"].
                   createInstance(Ci.nsIWebHandlerApp);
  webHandler.name = "Web Handler";
  webHandler.uriTemplate = "http://www.example.com/?%s";

  // FIXME: these tests create and manipulate enough variables that it would
  // make sense to move each test into its own scope so we don't run the risk
  // of one test stomping on another's data.


  //**************************************************************************//
  // Test Default Properties

  // Get a handler info for a MIME type that neither the application nor
  // the OS knows about and make sure its properties are set to the proper
  // default values.

  var handlerInfo = mimeSvc.getFromTypeAndExtension("nonexistent/type", null);

  // Make sure it's also an nsIHandlerInfo.
  do_check_true(handlerInfo instanceof Ci.nsIHandlerInfo);

  do_check_eq(handlerInfo.type, "nonexistent/type");

  // Deprecated property, but we should still make sure it's set correctly.
  do_check_eq(handlerInfo.MIMEType, "nonexistent/type");

  // These properties are the ones the handler service knows how to store.
  do_check_eq(handlerInfo.preferredAction, Ci.nsIHandlerInfo.saveToDisk);
  do_check_eq(handlerInfo.preferredApplicationHandler, null);
  do_check_eq(handlerInfo.possibleApplicationHandlers.length, 0);
  do_check_true(handlerInfo.alwaysAskBeforeHandling);

  // These properties are initialized to default values by the service,
  // so we might as well make sure they're initialized to the right defaults.
  do_check_eq(handlerInfo.description, "");
  do_check_eq(handlerInfo.hasDefaultHandler, false);
  do_check_eq(handlerInfo.defaultDescription, "");

  // FIXME: test a default protocol handler.


  //**************************************************************************//
  // Test Round-Trip Data Integrity

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
  do_check_true(preferredHandler instanceof Ci.nsILocalHandlerApp);
  preferredHandler.QueryInterface(Ci.nsILocalHandlerApp);
  do_check_eq(preferredHandler.executable.path, localHandler.executable.path);

  do_check_false(handlerInfo.alwaysAskBeforeHandling);

  // Make sure the handler service's enumerate method lists all known handlers.
  var handlerInfo2 = mimeSvc.getFromTypeAndExtension("nonexistent/type2", null);
  handlerSvc.store(handlerInfo2);
  var handlerTypes = ["nonexistent/type", "nonexistent/type2"];
  try { 
    // If we have a defaultHandlersVersion pref, then assume that we're in the
    // firefox tree and that we'll also have an added webcal handler.
    // Bug 395131 has been filed to make this test work more generically
    // by providing our own prefs for this test rather than this icky
    // special casing.
    rootPrefBranch.getCharPref("gecko.handlerService.defaultHandlersVersion");
    handlerTypes.push("webcal");
  } catch (ex) {}   
  var handlers = handlerSvc.enumerate();
  while (handlers.hasMoreElements()) {
    var handler = handlers.getNext().QueryInterface(Ci.nsIHandlerInfo);
    do_check_neq(handlerTypes.indexOf(handler.type), -1);
    handlerTypes.splice(handlerTypes.indexOf(handler.type), 1);
  }
  do_check_eq(handlerTypes.length, 0);

  // Make sure the handler service's remove method removes a handler record.
  handlerSvc.remove(handlerInfo2);
  handlers = handlerSvc.enumerate();
  while (handlers.hasMoreElements())
    do_check_neq(handlers.getNext().QueryInterface(Ci.nsIHandlerInfo).type,
                 handlerInfo2.type);

  // Make sure we can store and retrieve a handler info object with no preferred
  // handler.
  var noPreferredHandlerInfo =
    mimeSvc.getFromTypeAndExtension("nonexistent/no-preferred-handler", null);
  handlerSvc.store(noPreferredHandlerInfo);
  noPreferredHandlerInfo =
    mimeSvc.getFromTypeAndExtension("nonexistent/no-preferred-handler", null);
  do_check_eq(noPreferredHandlerInfo.preferredApplicationHandler, null);

  // Make sure that the handler service removes an existing handler record
  // if we store a handler info object with no preferred handler.
  var removePreferredHandlerInfo =
    mimeSvc.getFromTypeAndExtension("nonexistent/rem-preferred-handler", null);
  removePreferredHandlerInfo.preferredApplicationHandler = localHandler;
  handlerSvc.store(removePreferredHandlerInfo);
  removePreferredHandlerInfo =
    mimeSvc.getFromTypeAndExtension("nonexistent/rem-preferred-handler", null);
  removePreferredHandlerInfo.preferredApplicationHandler = null;
  handlerSvc.store(removePreferredHandlerInfo);
  removePreferredHandlerInfo =
    mimeSvc.getFromTypeAndExtension("nonexistent/rem-preferred-handler", null);
  do_check_eq(removePreferredHandlerInfo.preferredApplicationHandler, null);

  // Make sure we can store and retrieve a handler info object with possible
  // handlers.  We test both adding and removing handlers.

  // Get a handler info and make sure it has no possible handlers.
  var possibleHandlersInfo =
    mimeSvc.getFromTypeAndExtension("nonexistent/possible-handlers", null);
  do_check_eq(possibleHandlersInfo.possibleApplicationHandlers.length, 0);

  // Store and re-retrieve the handler and make sure it still has no possible
  // handlers.
  handlerSvc.store(possibleHandlersInfo);
  possibleHandlersInfo =
    mimeSvc.getFromTypeAndExtension("nonexistent/possible-handlers", null);
  do_check_eq(possibleHandlersInfo.possibleApplicationHandlers.length, 0);

  // Add two handlers, store the object, re-retrieve it, and make sure it has
  // two handlers.
  possibleHandlersInfo.possibleApplicationHandlers.appendElement(localHandler,
                                                                 false);
  possibleHandlersInfo.possibleApplicationHandlers.appendElement(webHandler,
                                                                 false);
  handlerSvc.store(possibleHandlersInfo);
  possibleHandlersInfo =
    mimeSvc.getFromTypeAndExtension("nonexistent/possible-handlers", null);
  do_check_eq(possibleHandlersInfo.possibleApplicationHandlers.length, 2);

  // Figure out which is the local and which is the web handler and the index
  // in the array of the local handler, which is the one we're going to remove
  // to test removal of a handler.
  var handler1 = possibleHandlersInfo.possibleApplicationHandlers.
                 queryElementAt(0, Ci.nsIHandlerApp);
  var handler2 = possibleHandlersInfo.possibleApplicationHandlers.
                 queryElementAt(1, Ci.nsIHandlerApp);
  var localPossibleHandler, webPossibleHandler, localIndex;
  if (handler1 instanceof Ci.nsILocalHandlerApp)
    [localPossibleHandler, webPossibleHandler, localIndex] = [handler1,
                                                              handler2,
                                                              0];
  else
    [localPossibleHandler, webPossibleHandler, localIndex] = [handler2,
                                                              handler1,
                                                              1];
  localPossibleHandler.QueryInterface(Ci.nsILocalHandlerApp);
  webPossibleHandler.QueryInterface(Ci.nsIWebHandlerApp);

  // Make sure the two handlers are the ones we stored.
  do_check_eq(localPossibleHandler.name, localHandler.name);
  do_check_true(localPossibleHandler.equals(localHandler));
  do_check_eq(webPossibleHandler.name, webHandler.name);
  do_check_true(webPossibleHandler.equals(webHandler));

  // Remove a handler, store the object, re-retrieve it, and make sure
  // it only has one handler.
  possibleHandlersInfo.possibleApplicationHandlers.removeElementAt(localIndex);
  handlerSvc.store(possibleHandlersInfo);
  possibleHandlersInfo =
    mimeSvc.getFromTypeAndExtension("nonexistent/possible-handlers", null);
  do_check_eq(possibleHandlersInfo.possibleApplicationHandlers.length, 1);

  // Make sure the handler is the one we didn't remove.
  webPossibleHandler = possibleHandlersInfo.possibleApplicationHandlers.
                       queryElementAt(0, Ci.nsIWebHandlerApp);
  do_check_eq(webPossibleHandler.name, webHandler.name);
  do_check_true(webPossibleHandler.equals(webHandler));

  // FIXME: test round trip integrity for a protocol.
  // FIXME: test round trip integrity for a handler info with a web handler.
}
