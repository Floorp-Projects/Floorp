/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  //**************************************************************************//
  // Constants

  const handlerSvc = Cc["@mozilla.org/uriloader/handler-service;1"].
                     getService(Ci.nsIHandlerService);

  const mimeSvc = Cc["@mozilla.org/mime;1"].
                  getService(Ci.nsIMIMEService);

  const protoSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"].
                   getService(Ci.nsIExternalProtocolService);
  
  const prefSvc = Cc["@mozilla.org/preferences-service;1"].
                  getService(Ci.nsIPrefService);
                  
  const ioService = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);

  const env = Cc["@mozilla.org/process/environment;1"].
              getService(Components.interfaces.nsIEnvironment);

  const rootPrefBranch = prefSvc.getBranch("");
  
  let noMailto = false;
  if (mozinfo.os == "win") {
    // Check mailto handler from registry.
    // If registry entry is nothing, no mailto handler
    let regSvc = Cc["@mozilla.org/windows-registry-key;1"].
                 createInstance(Ci.nsIWindowsRegKey);
    try {
      regSvc.open(regSvc.ROOT_KEY_CLASSES_ROOT,
                  "mailto",
                  regSvc.ACCESS_READ);
      noMailto = false;
    } catch (ex) {
      noMailto = true;
    }
    regSvc.close();
  }

  if (mozinfo.os == "linux") {
    // Check mailto handler from GIO
    // If there isn't one, then we have no mailto handler
    let gIOSvc = Cc["@mozilla.org/gio-service;1"].
                 createInstance(Ci.nsIGIOService);
    try {
      gIOSvc.getAppForURIScheme("mailto");
      noMailto = false;
    } catch (ex) {
      noMailto = true;
    }
  }

  //**************************************************************************//
  // Sample Data

  // It doesn't matter whether or not this nsIFile is actually executable,
  // only that it has a path and exists.  Since we don't know any executable
  // that exists on all platforms (except possibly the application being
  // tested, but there doesn't seem to be a way to get a reference to that
  // from the directory service), we use the temporary directory itself.
  var executable = Services.dirsvc.get("TmpD", Ci.nsIFile);
  // XXX We could, of course, create an actual executable in the directory:
  //executable.append("localhandler");
  //if (!executable.exists())
  //  executable.create(Ci.nsIFile.NORMAL_FILE_TYPE, 0o755);

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

  // test some default protocol info properties
  var haveDefaultHandlersVersion = false;
  try { 
    // If we have a defaultHandlersVersion pref, then assume that we're in the
    // firefox tree and that we'll also have default handlers.
    // Bug 395131 has been filed to make this test work more generically
    // by providing our own prefs for this test rather than this icky
    // special casing.
    rootPrefBranch.getCharPref("gecko.handlerService.defaultHandlersVersion");
    haveDefaultHandlersVersion = true;
  } catch (ex) {}

  const kExternalWarningDefault = 
    "network.protocol-handler.warn-external-default";
  prefSvc.setBoolPref(kExternalWarningDefault, true);

  // XXX add more thorough protocol info property checking
  
  // no OS default handler exists
  var protoInfo = protoSvc.getProtocolHandlerInfo("x-moz-rheet");
  do_check_eq(protoInfo.preferredAction, protoInfo.alwaysAsk);
  do_check_true(protoInfo.alwaysAskBeforeHandling);
  
  // OS default exists, injected default does not exist, 
  // explicit warning pref: false
  const kExternalWarningPrefPrefix = "network.protocol-handler.warn-external.";
  prefSvc.setBoolPref(kExternalWarningPrefPrefix + "http", false);
  protoInfo = protoSvc.getProtocolHandlerInfo("http");
  do_check_eq(0, protoInfo.possibleApplicationHandlers.length);
  do_check_false(protoInfo.alwaysAskBeforeHandling);
  
  // OS default exists, injected default does not exist, 
  // explicit warning pref: true
  prefSvc.setBoolPref(kExternalWarningPrefPrefix + "http", true);
  protoInfo = protoSvc.getProtocolHandlerInfo("http");
  // OS handler isn't included in possibleApplicationHandlers, so length is 0
  // Once they become instances of nsILocalHandlerApp, this number will need
  // to change.
  do_check_eq(0, protoInfo.possibleApplicationHandlers.length);
  do_check_true(protoInfo.alwaysAskBeforeHandling);

  // OS default exists, injected default exists, explicit warning pref: false
  prefSvc.setBoolPref(kExternalWarningPrefPrefix + "mailto", false);
  protoInfo = protoSvc.getProtocolHandlerInfo("mailto");
  if (haveDefaultHandlersVersion)
    do_check_eq(2, protoInfo.possibleApplicationHandlers.length);
  else
    do_check_eq(0, protoInfo.possibleApplicationHandlers.length);

  // Win7+ or Linux's GIO might not have a default mailto: handler
  if (noMailto)
    do_check_true(protoInfo.alwaysAskBeforeHandling);
  else
    do_check_false(protoInfo.alwaysAskBeforeHandling);

  // OS default exists, injected default exists, explicit warning pref: true
  prefSvc.setBoolPref(kExternalWarningPrefPrefix + "mailto", true);
  protoInfo = protoSvc.getProtocolHandlerInfo("mailto");
  if (haveDefaultHandlersVersion) {
    do_check_eq(2, protoInfo.possibleApplicationHandlers.length);
    // Win7+ or Linux's GIO may have no default mailto: handler. Otherwise
    // alwaysAskBeforeHandling is expected to be false here, because although
    // the pref is true, the value in RDF is false. The injected mailto handler
    // carried over the default pref value, and so when we set the pref above
    // to true it's ignored.
    if (noMailto)
      do_check_true(protoInfo.alwaysAskBeforeHandling);
    else
      do_check_false(protoInfo.alwaysAskBeforeHandling);

  } else {
    do_check_eq(0, protoInfo.possibleApplicationHandlers.length);
    do_check_true(protoInfo.alwaysAskBeforeHandling);
  }

  if (haveDefaultHandlersVersion) {
    // Now set the value stored in RDF to true, and the pref to false, to make
    // sure we still get the right value. (Basically, same thing as above but
    // with the values reversed.)
    prefSvc.setBoolPref(kExternalWarningPrefPrefix + "mailto", false);
    protoInfo.alwaysAskBeforeHandling = true;
    handlerSvc.store(protoInfo);
    protoInfo = protoSvc.getProtocolHandlerInfo("mailto");
    do_check_eq(2, protoInfo.possibleApplicationHandlers.length);
    do_check_true(protoInfo.alwaysAskBeforeHandling);
  }


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
  if (haveDefaultHandlersVersion) {
    handlerTypes.push("webcal");
    handlerTypes.push("mailto");
    handlerTypes.push("irc");
    handlerTypes.push("ircs");
  }
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
  possibleHandlersInfo.possibleApplicationHandlers.appendElement(localHandler);
  possibleHandlersInfo.possibleApplicationHandlers.appendElement(webHandler);
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

  //////////////////////////////////////////////////////
  // handler info command line parameters and equality
  var localApp = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                 createInstance(Ci.nsILocalHandlerApp);
  var handlerApp = localApp.QueryInterface(Ci.nsIHandlerApp);

  do_check_true(handlerApp.equals(localApp));

  localApp.executable = executable;

  do_check_eq(0, localApp.parameterCount);
  localApp.appendParameter("-test1");
  do_check_eq(1, localApp.parameterCount);
  localApp.appendParameter("-test2");
  do_check_eq(2, localApp.parameterCount);
  do_check_true(localApp.parameterExists("-test1"));
  do_check_true(localApp.parameterExists("-test2"));
  do_check_false(localApp.parameterExists("-false"));
  localApp.clearParameters();
  do_check_eq(0, localApp.parameterCount);

  var localApp2 = Cc["@mozilla.org/uriloader/local-handler-app;1"].
                  createInstance(Ci.nsILocalHandlerApp);
  
  localApp2.executable = executable;

  localApp.clearParameters();
  do_check_true(localApp.equals(localApp2));

  // equal:
  // cut -d 1 -f 2
  // cut -d 1 -f 2

  localApp.appendParameter("-test1");
  localApp.appendParameter("-test2");
  localApp.appendParameter("-test3");
  localApp2.appendParameter("-test1");
  localApp2.appendParameter("-test2");
  localApp2.appendParameter("-test3");
  do_check_true(localApp.equals(localApp2));

  // not equal:
  // cut -d 1 -f 2
  // cut -f 1 -d 2

  localApp.clearParameters();
  localApp2.clearParameters();

  localApp.appendParameter("-test1");
  localApp.appendParameter("-test2");
  localApp.appendParameter("-test3");
  localApp2.appendParameter("-test2");
  localApp2.appendParameter("-test1");
  localApp2.appendParameter("-test3");
  do_check_false(localApp2.equals(localApp));

  var str;
  str = localApp.getParameter(0)
  do_check_eq(str, "-test1");
  str = localApp.getParameter(1)
  do_check_eq(str, "-test2");
  str = localApp.getParameter(2)
  do_check_eq(str, "-test3");

  // FIXME: test round trip integrity for a protocol.
  // FIXME: test round trip integrity for a handler info with a web handler.

  //**************************************************************************//
  // getTypeFromExtension tests

  // test nonexistent extension
  var lolType = handlerSvc.getTypeFromExtension("lolcat");
  do_check_eq(lolType, "");


  // add a handler for the extension
  var lolHandler = mimeSvc.getFromTypeAndExtension("application/lolcat", null);

  do_check_false(lolHandler.extensionExists("lolcat"));
  lolHandler.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  lolHandler.preferredApplicationHandler = localHandler;
  lolHandler.alwaysAskBeforeHandling = false;

  // store the handler
  do_check_false(handlerSvc.exists(lolHandler));
  handlerSvc.store(lolHandler);
  do_check_true(handlerSvc.exists(lolHandler));

  // Get a file:// string pointing to mimeTypes.rdf
  var fileHandler = ioService.getProtocolHandler("file").QueryInterface(Ci.nsIFileProtocolHandler);
  var rdfFileURI = fileHandler.getURLSpecFromFile(rdfFile);

  // Assign a file extenstion to the handler. handlerSvc.store() doesn't
  // actually store any file extensions added with setFileExtensions(), you
  // have to wade into RDF muck to do so.

  // Based on toolkit/mozapps/downloads/content/helperApps.js :: addExtension()
  var gRDF = Cc["@mozilla.org/rdf/rdf-service;1"].getService(Ci.nsIRDFService);
  var mimeSource    = gRDF.GetUnicodeResource("urn:mimetype:application/lolcat");
  var valueProperty = gRDF.GetUnicodeResource("http://home.netscape.com/NC-rdf#fileExtensions");
  var mimeLiteral   = gRDF.GetLiteral("lolcat");

  var DS = gRDF.GetDataSourceBlocking(rdfFileURI);
  DS.Assert(mimeSource, valueProperty, mimeLiteral, true);


  // test now-existent extension
  lolType = handlerSvc.getTypeFromExtension("lolcat");
  do_check_eq(lolType, "application/lolcat");

  // test mailcap entries with needsterminal are ignored on non-Windows non-Mac.
  if (mozinfo.os != "win" && mozinfo.os != "mac") {
    env.set('PERSONAL_MAILCAP', do_get_file('mailcap').path);
    handlerInfo = mimeSvc.getFromTypeAndExtension("text/plain", null);
    do_check_eq(handlerInfo.preferredAction, Ci.nsIHandlerInfo.useSystemDefault);
    do_check_eq(handlerInfo.defaultDescription, "sed");
  }
}
