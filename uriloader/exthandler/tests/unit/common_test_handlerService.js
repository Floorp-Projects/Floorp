/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This script is loaded by "test_handlerService_json.js" and "test_handlerService_rdf.js"
 * to make sure there is the same behavior when using two different implementations
 * of handlerService (JSON backend and RDF backend).
 */

HandlerServiceTestUtils.handlerService = gHandlerService;

const pdfHandlerInfo =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type");
const gzipHandlerInfo =
      HandlerServiceTestUtils.getHandlerInfo("application/x-gzip");
const ircHandlerInfo =
      HandlerServiceTestUtils.getHandlerInfo("irc");

let executable = Services.dirsvc.get("TmpD", Ci.nsIFile);
let localHandler = {
  name: "Local Handler",
  executable: executable,
  interfaces: [Ci.nsIHandlerApp, Ci.nsILocalHandlerApp, Ci.nsISupports],
  QueryInterface: function(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

let webHandler = {
  name: "Web Handler",
  uriTemplate: "https://www.webhandler.com/?url=%s",
  interfaces: [Ci.nsIHandlerApp, Ci.nsIWebHandlerApp, Ci.nsISupports],
  QueryInterface: function(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

let dBusHandler = {
  name: "DBus Handler",
  service: "DBus Service",
  method: "DBus method",
  objectPath: "/tmp/PATH/DBus",
  dBusInterface: "DBusInterface",
  interfaces: [Ci.nsIHandlerApp, Ci.nsIDBusHandlerApp, Ci.nsISupports],
  QueryInterface: function(iid) {
    if (!this.interfaces.some( function(v) { return iid.equals(v) } ))
      throw Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};

// Loads data from a file in a predefined format. This test verifies that:
//  - The JSON format used in previous versions can be loaded
//  - All the known properties specified in the JSON file are considered
add_task(function* testLoadPredefined() {
  yield prepareImportDB();

  let handlerInfos =
      HandlerServiceTestUtils.getAllHandlerInfos();

  // It's important that the MIME types we check here do not exist at the
  // operating system level, otherwise the list of handlers and file extensions
  // will be merged. The current implementation adds each saved file extension
  // even if one already exists in the system, resulting in duplicate entries.

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "irc",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: true,
    possibleApplicationHandlers: [{
      name: "Mibbit",
      uriTemplate: "https://www.mibbit.com/?url=%s",
    }],
  });

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "ircs",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: true,
    possibleApplicationHandlers: [{
      name: "Mibbit",
      uriTemplate: "https://www.mibbit.com/?url=%s",
    }],
  });

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "mailto",
    preferredAction: Ci.nsIHandlerInfo.useSystemDefault,
    alwaysAskBeforeHandling: false,
    possibleApplicationHandlers: [{
      name: "Yahoo! Mail",
      uriTemplate: "https://compose.mail.yahoo.com/?To=%s",
    },{
      name: "Gmail",
      uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
    }],
  });

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "nonexistent/type",
    preferredAction: Ci.nsIHandlerInfo.handleInternally,
    alwaysAskBeforeHandling: false,
    fileExtensions: ["pdf"],
  });

  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfos.shift(), {
    type: "webcal",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: true,
    preferredApplicationHandler: {
      name: "30 Boxes",
      uriTemplate: "http://30boxes.com/external/widget?refer=ff&url=%s",
    },
    possibleApplicationHandlers: [{
      name: "30 Boxes",
      uriTemplate: "http://30boxes.com/external/widget?refer=ff&url=%s",
    },{
      name: "30 Boxes",
      uriTemplate: "https://30boxes.com/external/widget?refer=ff&url=%s",
    }],
  });

  do_check_eq(handlerInfos.length, 0);
});

// Verify the load mechansim of hander service by
// - Start the hander service with DB
// - Do some modifications on DB first and reload it
add_task(function* testImportAndReload() {
  // I. Prepare a testing ds first and do reload for handerService
  yield prepareImportDB();
  Assert.deepEqual(HandlerServiceTestUtils.getAllHandlerInfoTypes(),
                   ["irc", "ircs", "mailto", "nonexistent/type", "webcal"]);

  // II. do modifications first and reload the DS again
  gHandlerService.store(gzipHandlerInfo);
  gHandlerService.remove(pdfHandlerInfo);
  yield reloadData();
  Assert.deepEqual(HandlerServiceTestUtils.getAllHandlerInfoTypes(),
                   ["application/x-gzip", "irc", "ircs", "mailto", "webcal"]);
});

// Verify reload without DB
add_task(function* testReloadWithoutDB() {
  yield removeImportDB();
  // If we have a defaultHandlersVersion pref, then assume that we're in the
  // firefox tree and that we'll also have default handlers.
  if (Services.prefs.getPrefType("gecko.handlerService.defaultHandlersVersion")){
    Assert.deepEqual(HandlerServiceTestUtils.getAllHandlerInfoTypes(),
                     ["irc", "ircs", "mailto", "webcal"]);
  }
});

// Do the test for exist() with store() and remove()
add_task(function* testExists() {
  yield prepareImportDB();

  do_check_true(gHandlerService.exists(pdfHandlerInfo));
  do_check_false(gHandlerService.exists(gzipHandlerInfo));

  // Remove the handler of irc first
  let handler = ircHandlerInfo;
  gHandlerService.remove(handler);
  do_check_false(gHandlerService.exists(handler));
  gHandlerService.store(handler);
  do_check_true(gHandlerService.exists(handler));
});

// Do the test for GetTypeFromExtension() with store(), remove() and exist()
add_task(function* testGetTypeFromExtension() {
  yield prepareImportDB();

  let type = gHandlerService.getTypeFromExtension("doc");
  do_check_eq(type, "");
  type = gHandlerService.getTypeFromExtension("pdf");
  do_check_eq(type, "nonexistent/type");

  // Load the "pdf" extension into the nsIHandlerInfo for "nonexistent/type".
  gHandlerService.fillHandlerInfo(pdfHandlerInfo, "");

  gHandlerService.remove(pdfHandlerInfo);
  do_check_false(gHandlerService.exists(pdfHandlerInfo));
  type = gHandlerService.getTypeFromExtension("pdf");
  do_check_eq(type, "");

  gHandlerService.store(pdfHandlerInfo);
  do_check_true(gHandlerService.exists(pdfHandlerInfo));
  type = gHandlerService.getTypeFromExtension("pdf");
  do_check_eq(type, "nonexistent/type");
});

// Test the functionality of fillHandlerInfo :
//   - All the detail of handlerinfo are filled perferectly
//   - The set of possible handlers included the preferred handler
add_task(function* testStoreAndFillHandlerInfo() {
  yield removeImportDB();

  // Get a handler info for a MIME type that neither the application nor
  // the OS knows about and make sure its properties are set to the proper
  // default values.
  let handlerInfo =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type");
  let handlerInfo2 =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type2");
  handlerInfo2.preferredAction = Ci.nsIHandlerInfo.useSystemDefault;
  handlerInfo2.preferredApplicationHandler = localHandler;
  handlerInfo2.alwaysAskBeforeHandling = false;
  handlerInfo2.appendExtension("type2");
  gHandlerService.store(handlerInfo2);

  gHandlerService.fillHandlerInfo(handlerInfo, "nonexistent/type2");
  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfo, {
    type: "nonexistent/type",
    preferredAction: Ci.nsIHandlerInfo.useSystemDefault,
    alwaysAskBeforeHandling: false,
    fileExtensions: ["type2"],
    preferredApplicationHandler: {
      name: "Local Handler",
      executable,
    },
    possibleApplicationHandlers: [{
      name: "Local Handler",
      executable,
    }],
  });
});

// Test the functionality of fillHandlerInfo :
// - Check the failure case by requesting a non-existent handler type
add_task(function* testFillHandlerInfoWithError() {
  yield removeImportDB();

  let handlerInfo =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type");

  Assert.throws(
    () => gHandlerService.fillHandlerInfo(handlerInfo, "nonexistent/type2"),
    ex => ex.result == Cr.NS_ERROR_NOT_AVAILABLE);
});

// Test the functionality of fillHandlerInfo :
// - Prefer handler is the first one of possibleHandlers and with only one instance
add_task(function* testPreferHandlerIsTheFirstOrder() {
  yield removeImportDB();

  let handlerInfo =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type");
  let handlerInfo2 =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type2");
  handlerInfo2.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  handlerInfo2.preferredApplicationHandler = webHandler;
  handlerInfo2.possibleApplicationHandlers.appendElement(localHandler, false);
  handlerInfo2.possibleApplicationHandlers.appendElement(webHandler, false);
  handlerInfo2.alwaysAskBeforeHandling = false;
  gHandlerService.store(handlerInfo2);

  gHandlerService.fillHandlerInfo(handlerInfo, "nonexistent/type2");
  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfo, {
    type: "nonexistent/type",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: false,
    preferredApplicationHandler: {
      name: webHandler.name,
      uriTemplate: webHandler.uriTemplate,
    },
    possibleApplicationHandlers: [{
      name: webHandler.name,
      uriTemplate: webHandler.uriTemplate,
    },{
      name: "Local Handler",
      executable,
    }],
  });
});

// Verify the handling of app handler: web handler
add_task(function* testStoreForWebHandler() {
  yield removeImportDB();

  let handlerInfo =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type");
  let handlerInfo2 =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type2");
  handlerInfo2.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  handlerInfo2.preferredApplicationHandler = webHandler;
  handlerInfo2.alwaysAskBeforeHandling = false;
  gHandlerService.store(handlerInfo2);

  gHandlerService.fillHandlerInfo(handlerInfo, "nonexistent/type2");
  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfo, {
    type: "nonexistent/type",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: false,
    preferredApplicationHandler: {
      name: webHandler.name,
      uriTemplate: webHandler.uriTemplate,
    },
    possibleApplicationHandlers: [{
      name: webHandler.name,
      uriTemplate: webHandler.uriTemplate,
    }],
  });
});

// Verify the handling of app handler: DBus handler
add_task(function* testStoreForDBusHandler() {
  if (!("@mozilla.org/uriloader/dbus-handler-app;1" in Cc)) {
    do_print("Skipping test because it does not apply to this platform.");
    return;
  }

  yield removeImportDB();

  let handlerInfo =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type");
  let handlerInfo2 =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type2");
  handlerInfo2.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  handlerInfo2.preferredApplicationHandler = dBusHandler;
  handlerInfo2.alwaysAskBeforeHandling = false;
  gHandlerService.store(handlerInfo2);

  gHandlerService.fillHandlerInfo(handlerInfo, "nonexistent/type2");
  let expectedHandler = {
    name: dBusHandler.name,
    service: dBusHandler.service,
    method: dBusHandler.method,
    dBusInterface: dBusHandler.dBusInterface,
    objectPath: dBusHandler.objectPath,
  };
  HandlerServiceTestUtils.assertHandlerInfoMatches(handlerInfo, {
    type: "nonexistent/type",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: false,
    preferredApplicationHandler: expectedHandler,
    possibleApplicationHandlers: [expectedHandler],
  });
});

// Test the functionality of _IsInHandlerArray() by injecting default handler again
// Since we don't have defaultHandlersVersion pref on Android, skip this test.
// Also skip for applications like Thunderbird which don't have all the prefs.
add_task(function* testIsInHandlerArray() {
  if (AppConstants.platform == "android") {
    do_print("Skipping test because it does not apply to this platform.");
    return;
  }
  if (!Services.prefs.getPrefType("gecko.handlerService.defaultHandlersVersion")) {
    do_print("Skipping test: No pref gecko.handlerService.defaultHandlersVersion.");
    return;
  }

  yield removeImportDB();

  let protoInfo = HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent");
  do_check_eq(protoInfo.possibleApplicationHandlers.length, 0);
  gHandlerService.fillHandlerInfo(protoInfo, "ircs");
  do_check_eq(protoInfo.possibleApplicationHandlers.length, 1);

  // Remove the handler of irc first
  let ircInfo = HandlerServiceTestUtils.getHandlerInfo("irc");
  gHandlerService.remove(ircInfo);
  do_check_false(gHandlerService.exists(ircInfo));

  let origPrefs = Services.prefs.getComplexValue(
    "gecko.handlerService.defaultHandlersVersion", Ci.nsIPrefLocalizedString);

  // Set preference as an arbitrarily high number to force injecting
  let string = Cc["@mozilla.org/pref-localizedstring;1"]
               .createInstance(Ci.nsIPrefLocalizedString);
  string.data = "999";
  Services.prefs.setComplexValue("gecko.handlerService.defaultHandlersVersion",
                                 Ci.nsIPrefLocalizedString, string);

  // do reloading
  yield reloadData();

  // check "irc" exists again to make sure that injection actually happened
  do_check_true(gHandlerService.exists(ircInfo));

  // test "ircs" has only one handler to know the _IsInHandlerArray was invoked
  protoInfo = HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent");
  do_check_false(gHandlerService.exists(protoInfo));
  gHandlerService.fillHandlerInfo(protoInfo, "ircs");
  do_check_eq(protoInfo.possibleApplicationHandlers.length, 1);

  // reset the preference after the test
  Services.prefs.setComplexValue("gecko.handlerService.defaultHandlersVersion",
                                 Ci.nsIPrefLocalizedString, origPrefs);
});

// Test the basic functionality of FillHandlerInfo() for protocol
// Since Android use mimeInfo to deal with mimeTypes and protocol, skip this test.
// Also skip for applications like Thunderbird which don't have all the prefs.
add_task(function* testFillHandlerInfoForProtocol() {
  if (AppConstants.platform == "android") {
    do_print("Skipping test because it does not apply to this platform.");
    return;
  }
  if (!Services.prefs.getPrefType("gecko.handlerService.defaultHandlersVersion")) {
    do_print("Skipping test: No pref gecko.handlerService.defaultHandlersVersion.");
    return;
  }

  yield removeImportDB();

  let protoInfo = HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent");

  let ircInfo = HandlerServiceTestUtils.getHandlerInfo("irc");
  do_check_true(gHandlerService.exists(ircInfo));

  gHandlerService.fillHandlerInfo(protoInfo, "irc");
  HandlerServiceTestUtils.assertHandlerInfoMatches(protoInfo, {
    type: "nonexistent",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: true,
    possibleApplicationHandlers: [{
      name: "Mibbit",
      uriTemplate: "https://www.mibbit.com/?url=%s",
    }],
  });
});


// Test the functionality of store() and fillHandlerInfo for protocol
add_task(function* testStoreForProtocol() {
  yield removeImportDB();

  let protoInfo = HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent");
  let protoInfo2 = HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent2");
  protoInfo2.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  protoInfo2.alwaysAskBeforeHandling = false;
  protoInfo2.preferredApplicationHandler = webHandler;
  gHandlerService.store(protoInfo2);

  yield reloadData();
  do_check_true(gHandlerService.exists(protoInfo2));

  gHandlerService.fillHandlerInfo(protoInfo, "nonexistent2");
  HandlerServiceTestUtils.assertHandlerInfoMatches(protoInfo, {
    type: "nonexistent",
    preferredAction: Ci.nsIHandlerInfo.useHelperApp,
    alwaysAskBeforeHandling: false,
    preferredApplicationHandler: {
      name: webHandler.name,
      uriTemplate: webHandler.uriTemplate,
    },
    possibleApplicationHandlers: [{
      name: webHandler.name,
      uriTemplate: webHandler.uriTemplate,
    }],
  });
});

// Test the functionality of fillHandlerInfo when there is no overrideType
add_task(function* testFillHandlerInfoWithoutOverrideType() {
  yield removeImportDB();

  // mimeType
  let mimeInfo =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type");
  let storedHandlerInfo =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent/type");
  storedHandlerInfo.preferredAction = Ci.nsIHandlerInfo.useSystemDefault;
  storedHandlerInfo.preferredApplicationHandler = webHandler;
  storedHandlerInfo.alwaysAskBeforeHandling = false;
  gHandlerService.store(storedHandlerInfo);

  // protocol type
  let protoInfo = HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent");
  let storedProtoInfo =
      HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent");
  storedProtoInfo.preferredAction = Ci.nsIHandlerInfo.useHelperApp;
  storedProtoInfo.alwaysAskBeforeHandling = false;
  storedProtoInfo.preferredApplicationHandler = webHandler;
  gHandlerService.store(storedProtoInfo);

  // Get handlerInfo by fillHandlerInfo without overrideType
  for (let handlerInfo of [mimeInfo, protoInfo]) {
    let handlerInfo2 = storedProtoInfo;
    if (handlerInfo.type == "nonexistent/type") {
      handlerInfo2 = storedHandlerInfo;
    }
    gHandlerService.fillHandlerInfo(handlerInfo, null);
    do_check_eq(handlerInfo.preferredAction, handlerInfo2.preferredAction);
    do_check_eq(handlerInfo.alwaysAskBeforeHandling,
                handlerInfo2.alwaysAskBeforeHandling);
    do_check_eq(handlerInfo.preferredApplicationHandler.name,
                handlerInfo2.preferredApplicationHandler.name);
    let apps = handlerInfo.possibleApplicationHandlers.enumerate();
    let app;
    if (AppConstants.platform == "android") {
      app = apps.getNext().QueryInterface(Ci.nsIHandlerApp);
      do_check_eq(app.name, "Android chooser");
    }
    app = apps.getNext().QueryInterface(Ci.nsIWebHandlerApp);
    do_check_eq(app.name, webHandler.name);
    do_check_eq(app.uriTemplate, webHandler.uriTemplate);
  }
});

// Test the functionality of fillHandlerInfo() :
// - Use "nsIHandlerInfo.useHelperApp" to replace "nsIHandlerInfo.alwaysAsk" for handlerInfo.preferredAction
// - Use "nsIHandlerInfo.useHelperApp" to replace unknow action for handlerInfo.preferredAction
add_task(function* testPreferredActionHandling() {
  yield removeImportDB();

  let protoInfo = HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent");
  let protoInfo2 = HandlerServiceTestUtils.getBlankHandlerInfo("nonexistent2");

  for (let preferredAction of [
    Ci.nsIHandlerInfo.saveToDisk,
    Ci.nsIHandlerInfo.useHelperApp,
    Ci.nsIHandlerInfo.handleInternally,
    Ci.nsIHandlerInfo.useSystemDefault
  ]) {
    protoInfo2.preferredAction = preferredAction;
    gHandlerService.store(protoInfo2);
    gHandlerService.fillHandlerInfo(protoInfo, "nonexistent2");
    do_check_eq(protoInfo.preferredAction, preferredAction);
  }

  for (let preferredAction of [
    Ci.nsIHandlerInfo.alwaysAsk,
    999
  ]) {
    protoInfo2.preferredAction = preferredAction;
    gHandlerService.store(protoInfo2);
    gHandlerService.fillHandlerInfo(protoInfo, "nonexistent2");
    do_check_eq(protoInfo.preferredAction, Ci.nsIHandlerInfo.useHelperApp);
  }
});
