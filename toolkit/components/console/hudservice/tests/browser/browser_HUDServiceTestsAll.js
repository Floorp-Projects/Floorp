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

const TEST_PROPERTY_PROVIDER_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-property-provider.html";

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

  // test for multiple arguments.
  HUDService.clearDisplay(hudId);
  HUDService.setFilterState(hudId, aMethod, true);
  browser.contentWindow.wrappedJSObject.console[aMethod]("foo", "bar");

  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let outputLogNode = jsterm.outputNode;
  ok(/foo bar/.test(outputLogNode.childNodes[0].childNodes[0].nodeValue),
    "Emitted both console arguments");
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

function testOutputOrder()
{
  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let outputNode = jsterm.outputNode;

  jsterm.clearOutput();
  jsterm.execute("console.log('foo', 'bar');");

  is(outputNode.childNodes.length, 3, "Three children in output");
  let outputChildren = outputNode.childNodes;

  let executedStringFirst =
    /console\.log\('foo', 'bar'\);/.test(outputChildren[0].childNodes[0].nodeValue);

  let outputSecond =
    /foo bar/.test(outputChildren[1].childNodes[0].nodeValue);

  ok(executedStringFirst && outputSecond, "executed string comes first");
}

function testNullUndefinedOutput()
{
  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let outputNode = jsterm.outputNode;

  jsterm.clearOutput();
  jsterm.execute("null;");

  is(outputNode.childNodes.length, 2, "Two children in output");
  let outputChildren = outputNode.childNodes;

  is (outputChildren[1].childNodes[0].nodeValue, "null",
      "'null' printed to output");

  jsterm.clearOutput();
  jsterm.execute("undefined;");

  is(outputNode.childNodes.length, 2, "Two children in output");
  outputChildren = outputNode.childNodes;

  is (outputChildren[1].childNodes[0].nodeValue, "undefined",
      "'undefined' printed to output");
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

function testExposedConsoleAPI()
{
  let apis = [];
  for (var prop in browser.contentWindow.wrappedJSObject.console) {
    apis.push(prop);
  }

  is(apis.join(" "), "log info warn error exception", "Only console API is exposed on console object");
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

function testConsoleHistory()
{
  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let input = jsterm.inputNode;

  let executeList = ["document", "window", "window.location"];

  for each (var item in executeList) {
    input.value = item;
    jsterm.execute();
  }

  for (var i = executeList.length - 1; i != -1; i--) {
    jsterm.historyPeruse(true);
    is (input.value, executeList[i], "check history previous idx:" + i);
  }

  jsterm.historyPeruse(true);
  is (input.value, executeList[0], "test that item is still index 0");

  jsterm.historyPeruse(true);
  is (input.value, executeList[0], "test that item is still still index 0");


  for (var i = 1; i < executeList.length; i++) {
    jsterm.historyPeruse(false);
    is (input.value, executeList[i], "check history next idx:" + i);
  }

  jsterm.historyPeruse(false);
  is (input.value, "", "check input is empty again");

  // Simulate pressing Arrow_Down a few times and then if Arrow_Up shows
  // the previous item from history again.
  jsterm.historyPeruse(false);
  jsterm.historyPeruse(false);
  jsterm.historyPeruse(false);

  is (input.value, "", "check input is still empty");

  let idxLast = executeList.length - 1;
  jsterm.historyPeruse(true);
  is (input.value, executeList[idxLast], "check history next idx:" + idxLast);
}

// test property provider
function testPropertyProvider()
{
  var HUD = HUDService.hudWeakReferences[hudId].get();
  var jsterm = HUD.jsterm;
  var context = jsterm.sandbox.window;
  var completion;

  // Test if the propertyProvider can be accessed from the jsterm object.
  ok (jsterm.propertyProvider !== undefined, "JSPropertyProvider is defined");

  completion = jsterm.propertyProvider(context, "thisIsNotDefined");
  is (completion.matches.length, 0, "no match for 'thisIsNotDefined");

  // This is a case the PropertyProvider can't handle. Should return null.
  completion = jsterm.propertyProvider(context, "window[1].acb");
  is (completion, null, "no match for 'window[1].acb");

  // A very advanced completion case.
  var strComplete =
    'function a() { }document;document.getElementById(window.locatio';
  completion = jsterm.propertyProvider(context, strComplete);
  ok(completion.matches.length == 2, "two matches found");
  ok(completion.matchProp == "locatio", "matching part is 'test'");
  ok(completion.matches[0] == "location", "the first match is 'location'");
  ok(completion.matches[1] == "locationbar", "the second match is 'locationbar'");
}

function testCompletion()
{
  var HUD = HUDService.hudWeakReferences[hudId].get();
  var jsterm = HUD.jsterm;
  var input = jsterm.inputNode;

  // Test typing 'docu'.
  input.value = "docu";
  input.setSelectionRange(4, 4);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY);
  is(input.value, "document", "'docu' completion");
  is(input.selectionStart, 4, "start selection is alright");
  is(input.selectionEnd, 8, "end selection is alright");

  // Test typing 'docu' and press tab.
  input.value = "docu";
  input.setSelectionRange(4, 4);
  jsterm.complete(jsterm.COMPLETE_FORWARD);
  is(input.value, "document", "'docu' tab completion");
  is(input.selectionStart, 8, "start selection is alright");
  is(input.selectionEnd, 8, "end selection is alright");

  // Test typing 'document.getElem'.
  input.value = "document.getElem";
  input.setSelectionRange(16, 16);
  jsterm.complete(jsterm.COMPLETE_HINT_ONLY);
  is(input.value, "document.getElementById", "'document.getElem' completion");
  is(input.selectionStart, 16, "start selection is alright");
  is(input.selectionEnd, 23, "end selection is alright");

  // Test pressing tab another time.
  jsterm.complete(jsterm.COMPLETE_FORWARD);
  is(input.value, "document.getElementsByClassName", "'document.getElem' another tab completion");
  is(input.selectionStart, 16, "start selection is alright");
  is(input.selectionEnd, 31, "end selection is alright");

  // Test pressing shift_tab.
  jsterm.complete(jsterm.COMPLETE_BACKWARD);
  is(input.value, "document.getElementById", "'document.getElem' untab completion");
  is(input.selectionStart, 16, "start selection is alright");
  is(input.selectionEnd, 23, "end selection is alright");
}

function testExecutionScope()
{
  content.location.href = TEST_URI;

  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let outputNode = jsterm.outputNode;

  jsterm.clearOutput();
  jsterm.execute("location;");

  is(outputNode.childNodes.length, 2, "Two children in output");
  let outputChildren = outputNode.childNodes;

  is(/location;/.test(outputChildren[0].childNodes[0].nodeValue), true,
    "'location;' written to output");

  isnot(outputChildren[1].childNodes[0].nodeValue.indexOf(TEST_URI), -1,
    "command was executed in the window scope");
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

function testHUDGetters()
{
  var HUD = HUDService.hudWeakReferences[hudId].get();
  var jsterm = HUD.jsterm;
  var klass = jsterm.inputNode.getAttribute("class");
  ok(klass == "jsterm-input-node", "We have the input node.");

  var hudconsole = HUD.console;
  is(typeof hudconsole, "object", "HUD.console is an object");
  is(typeof hudconsole.log, "function", "HUD.console.log is a function");
  is(typeof hudconsole.info, "function", "HUD.console.info is a function");
}

function testPageReload() {
  // see bug 578437 - The HUD console fails to re-attach the window.console 
  // object after page reload.

  browser.addEventListener("DOMContentLoaded", function onDOMLoad () {
    browser.removeEventListener("DOMContentLoaded", onDOMLoad, false);

    var console = browser.contentWindow.wrappedJSObject.console;

    is(typeof console, "object", "window.console is an object, after page reload");
    is(typeof console.log, "function", "console.log is a function");
    is(typeof console.info, "function", "console.info is a function");
    is(typeof console.warn, "function", "console.warn is a function");
    is(typeof console.error, "function", "console.error is a function");
    is(typeof console.exception, "function", "console.exception is a function");

    testEnd();
  }, false);

  content.location.reload();
}

function testEnd() {
  // testUnregister();
  executeSoon(function () {
    HUDService.deactivateHUDForContext(tab);
    HUDService.shutdown();
  });
  finish();
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
      testHUDGetters();
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

      // ConsoleStorageTests
      testCreateDisplay();
      testRecordEntry();
      testRecordManyEntries();
      testIteration();
      testConsoleHistory();
      testOutputOrder();
      testNullUndefinedOutput();
      testExecutionScope();
      testCompletion();
      testPropertyProvider();
      testPageReload();
    });
  }, false);
}
