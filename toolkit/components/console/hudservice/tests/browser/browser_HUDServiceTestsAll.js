/* vim:set ts=2 sw=2 sts=2 et: */
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
 * The Original Code is DevTools test code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  David Dahl <ddahl@mozilla.com>
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "HUDService", function () {
  Cu.import("resource://gre/modules/HUDService.jsm");
  try {
    return HUDService;
  }
  catch (ex) {
    dump(ex + "\n");
  }
});

let log = function _log(msg) {
  dump("*** HUD Browser Test Log: " + msg + "\n");
};

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";

const TEST_HTTP_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-observe-http-ajax.html";

const TEST_NETWORK_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-network.html";

const TEST_FILTER_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-filter.html";

const TEST_MUTATION_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-mutation.html";

function noCacheUriSpec(aUriSpec) {
  return aUriSpec + "?_=" + Date.now();
}

content.location.href = TEST_URI;

function testRegistries() {
  var displaysIdx = HUDService.displaysIndex();
  ok(displaysIdx.length == 1, "one display id found");

  var display = displaysIdx[0];
  var registry = HUDService.displayRegistry;
  var uri = registry[display];
  ok(registry[display], "we have a URI: " + registry[display]);

  var uriRegistry = HUDService.uriRegistry;
  ok(uriRegistry[uri].length == 1, "uri registry is working");
}

function testGetDisplayByURISpec() {
  var outputNode = HUDService.getDisplayByURISpec(TEST_URI);
  hudId = outputNode.getAttribute("id");
  ok(hudId == HUDService.displaysIndex()[0], "outputNode fetched by URIspec");
}

function introspectLogNodes() {
  var console =
    tab.linkedBrowser.contentWindow.wrappedJSObject.console;
  ok(console, "console exists");
  console.log("I am a log message");
  console.error("I am an error");
  console.info("I am an info message");
  console.warn("I am a warning  message");

  var len = HUDService.displaysIndex().length;
  let id = HUDService.displaysIndex()[len - 1];
  let hudBox = tab.ownerDocument.getElementById(id);

  let outputNode =
    hudBox.querySelectorAll(".hud-output-node")[0];
  ok(outputNode.childElementCount > 0, "more than 1 child node");

  let domLogEntries =
    outputNode.childNodes;

  let count = outputNode.childNodes.length;
  ok(count > 0, "LogCount: " + count);

  let klasses = ["hud-msg-node hud-log",
                 "hud-msg-node hud-warn",
                 "hud-msg-node hud-info",
                 "hud-msg-node hud-error",
                 "hud-msg-node hud-exception",
                 "hud-msg-node hud-network"];

  function verifyClass(klass) {
    let len = klasses.length;
    for (var i = 0; i < len; i++) {
      if (klass == klasses[i]) {
        return true;
      }
    }
    return false;
  }

  for (var i = 0; i < count; i++) {
    let klass = domLogEntries[i].getAttribute("class");
    ok(verifyClass(klass),
       "Log Node class verified: " + klass);
  }
}

function getAllHUDS() {
  var allHuds = HUDService.displays();
  ok(typeof allHuds == "object", "allHuds is an object");
  var idx = HUDService.displaysIndex();

  hudId = idx[0];

  ok(typeof idx == "object", "displays is an object");
  ok(typeof idx.push == "function", "displaysIndex is an array");

  var len = idx.length;
  ok(idx.length > 0, "idx.length > 0: " + len);
}

function testGetDisplayByLoadGroup() {
  var outputNode = HUDService.getDisplayByURISpec(TEST_URI);
  var hudId = outputNode.getAttribute("id");
  var loadGroup = HUDService.getLoadGroup(hudId);
  var display = HUDService.getDisplayByLoadGroup(loadGroup);
  ok(display.getAttribute("id") == hudId, "got display by loadGroup");

  content.location = TEST_HTTP_URI;

  executeSoon(function () {
                let id = HUDService.displaysIndex()[0];

                let domLogEntries =
                  outputNode.childNodes;

                let count = outputNode.childNodes.length;
                ok(count > 0, "LogCount: " + count);

                let klasses = ["hud-network"];

                function verifyClass(klass) {
                  let len = klasses.length;
                  for (var i = 0; i < len; i++) {
                    if (klass == klasses[i]) {
                      return true;
                    }
                  }
                  return false;
                }

                for (var i = 0; i < count; i++) {
                  let klass = domLogEntries[i].getAttribute("class");
                  if (klass != "hud-network") {
                    continue;
                  }
                  ok(verifyClass(klass),
                     "Log Node network class verified");
                }

              });
}

function testUnregister()  {
  HUDService.deactivateHUDForContext(tab);
  ok(HUDService.displays()[0] == undefined,
     "No heads up displays are registered");
  HUDService.shutdown();
}

function getHUDById() {
  let hud = HUDService.getHeadsUpDisplay(hudId);
  ok(hud.getAttribute("id") == hudId, "found HUD node by Id.");
}

function testGetContentWindowFromHUDId() {
  let window = HUDService.getContentWindowFromHUDId(hudId);
  ok(window.document, "we have a contentWindow");
}

function testConsoleLoggingAPI(aMethod)
{
  filterBox.value = "foo";
  browser.contentWindow.wrappedJSObject.console[aMethod]("foo-bar-baz");
  browser.contentWindow.wrappedJSObject.console[aMethod]("bar-baz");
  var count = outputNode.querySelectorAll(".hud-hidden").length;
  ok(count == 1, "1 hidden " + aMethod  + " node found");
  HUDService.clearDisplay(hudId);

  // now toggle the current method off - make sure no visible message
  // nodes are logged
  filterBox.value = "";
  HUDService.setFilterState(hudId, aMethod, false);
  browser.contentWindow.wrappedJSObject.console[aMethod]("foo-bar-baz");
  count = outputNode.querySelectorAll(".hud-hidden").length;
  ok(count == 0, aMethod + " logging tunred off, 0 messages logged");
  HUDService.clearDisplay(hudId);
  filterBox.value = "";
}

function testLogEntry(aOutputNode, aMatchString, aSuccessErrObj)
{
  executeSoon(function (){
                var msgs = aOutputNode.childNodes;
                for (var i = 0; i < msgs.length; i++) {
                  var message = msgs[i].innerHTML.indexOf(aMatchString);
                  if (message > -1) {
                    ok(true, aSuccessErrObj.success);
                    return;
                  }
                  else {
                    throw new Error(aSuccessErrObj.err);
                  }
                }
              });
}

// test network logging
function testNet()
{
  HUDService.activateHUDForContext(tab);
  content.location = TEST_NETWORK_URI;
  executeSoon(function () {
    HUDService.setFilterState(hudId, "network", true);
    filterBox.value = "";
    var successMsg =
      "Found the loggged network message referencing a js file";
    var errMsg = "Could not get logged network message for js file";
    testLogEntry(outputNode,
                 "Network:", { success: successMsg, err: errMsg });
                 content.location.href = noCacheUriSpec(TEST_NETWORK_URI);
  });
}

// test DOM Mutation logging
function testDOMMutation()
{
  HUDService.setFilterState(hudId, "mutation", true);
  filterBox.value = "";
  content.location = TEST_MUTATION_URI;
  executeSoon(function() {
                content.wrappedJSObject.addEventListener("DOMContentLoaded",
                function () {
                  var successMsg = "Found Mutation Log Message";
                  var errMsg = "Could NOT find Mutation Log Message";
                  var display = HUDService.getHeadsUpDisplay(hudId);
                  var outputNode = display.querySelectorAll(".hud-output-node")[0];
                  testLogEntry(outputNode,
                  "Mutation", { success: successMsg, err: errMsg });
                  }, false);
                content.location.href = TEST_NETWORK_URI;
              });
}

function testCreateDisplay() {
  ok(typeof cs.consoleDisplays == "object",
     "consoledisplays exist");
  ok(typeof cs.displayIndexes == "object",
     "console indexes exist");
  cs.createDisplay("foo");
  ok(typeof cs.consoleDisplays["foo"] == "object",
     "foo display exists");
  ok(typeof cs.displayIndexes["foo"] == "object",
     "foo index exists");
}

function testRecordEntry() {
  var config = {
    logLevel: "network",
    message: "HumminaHummina!",
    activity: {
      stage: "barStage",
      data: "bar bar bar bar"
    }
  };
  var entry = cs.recordEntry("foo", config);
  var res = entry.id;
  ok(entry.id != null, "Entry.id is: " + res);
  ok(cs.displayIndexes["foo"].length == 1,
     "We added one entry.");
  entry = cs.getEntry(res);
  ok(entry.id > -1,
     "We got an entry through the global interface");
}

function testRecordManyEntries() {
  var configArr = [];

  for (var i = 0; i < 1000; i++){
    let config = {
      logLevel: "network",
      message: "HumminaHummina!",
      activity: {
        stage: "barStage",
        data: "bar bar bar bar"
      }
    };
    configArr.push(config);
  }

  var start = Date.now();
  cs.recordEntries("foo", configArr);
  var end = Date.now();
  var elapsed = end - start;
  ok(cs.displayIndexes["foo"].length == 1001,
     "1001 entries in foo now");
}

function testIteration() {
  var id = "foo";
  var it = cs.displayStore(id);
  var entry = it.next();
  var entry2 = it.next();

  let entries = [];
  for (var i = 0; i < 100; i++) {
    let _entry = it.next();
    entries.push(_entry);
  }

  ok(entries.length == 100, "entries length == 100");

  let entries2 = [];

  for (var i = 0; i < 100; i++){
    let _entry = it.next();
    entries2.push(_entry);
  }

  ok(entries[0].id != entries2[0].id,
     "two distinct pages of log entries");
}

let tab, browser, hudId, hud, filterBox, outputNode, cs;

let win = gBrowser.selectedBrowser;
tab = gBrowser.selectedTab;
browser = gBrowser.getBrowserForTab(tab);

function test() {
  waitForExplicitFinish();
  browser.addEventListener("DOMContentLoaded", function onLoad(event) {
    browser.removeEventListener("DOMContentLoaded", onLoad, false);

    HUDService.activateHUDForContext(tab);
    hudId = HUDService.displaysIndex()[0];
    hud = HUDService.getHeadsUpDisplay(hudId);
    cs = HUDService.storage;
    // enter some filter text into the filter textbox
    filterBox = hud.querySelectorAll(".hud-filter-box")[0];
    outputNode = HUDService.getHeadsUpDisplay(hudId);


    executeSoon(function () {
      testRegistries();
      testGetDisplayByURISpec();
      introspectLogNodes();
      getAllHUDS();
      getHUDById();
      testGetDisplayByLoadGroup();
      testGetContentWindowFromHUDId();

      content.location.href = TEST_FILTER_URI;

      testConsoleLoggingAPI("log");
      testConsoleLoggingAPI("info");
      testConsoleLoggingAPI("warn");
      testConsoleLoggingAPI("error");
      testConsoleLoggingAPI("exception");

      testNet();
      // testDOMMutation();

      // ConsoleStorageTests
      testCreateDisplay();
      testRecordEntry();
      testRecordManyEntries();
      testIteration();

      // testUnregister();
      executeSoon(function () {
        HUDService.deactivateHUDForContext(tab);
        HUDService.shutdown();
      });
      finish();
    });
  }, false);
}
