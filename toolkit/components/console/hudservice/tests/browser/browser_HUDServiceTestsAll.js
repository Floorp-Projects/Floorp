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
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Julian Viereck <jviereck@mozilla.com>
 *  Mihai Șucan <mihai.sucan@gmail.com>
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

XPCOMUtils.defineLazyGetter(this, "ConsoleUtils", function () {
  Cu.import("resource://gre/modules/HUDService.jsm");
  try {
    return ConsoleUtils;
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

const TEST_PROPERTY_PROVIDER_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-property-provider.html";

const TEST_ERROR_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-error.html";

const TEST_DUPLICATE_ERROR_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-duplicate-error.html";

const TEST_IMG = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-image.png";

const TEST_ENCODING_ISO_8859_1 = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-encoding-ISO-8859-1.html";

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

  let klasses = ["hud-group",
                 "hud-msg-node hud-log",
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

// Tests to ensure that the input box is focused when the console opens. See
// bug 579412.
function testInputFocus() {
  let hud = HUDService.getHeadsUpDisplay(hudId);
  let inputNode = hud.querySelectorAll(".jsterm-input-node")[0];
  is(inputNode.getAttribute("focused"), "true", "input node is focused");
}

function testGetContentWindowFromHUDId() {
  let window = HUDService.getContentWindowFromHUDId(hudId);
  ok(window.document, "we have a contentWindow");
}

function setStringFilter(aValue)
{
  let hud = HUDService.getHeadsUpDisplay(hudId);
  hud.querySelector(".hud-filter-box").value = aValue;
  HUDService.adjustVisibilityOnSearchStringChange(hudId, aValue);
}

function testConsoleLoggingAPI(aMethod)
{
  HUDService.clearDisplay(hudId);

  setStringFilter("foo");
  browser.contentWindow.wrappedJSObject.console[aMethod]("foo-bar-baz");
  browser.contentWindow.wrappedJSObject.console[aMethod]("bar-baz");
  var count = outputNode.querySelectorAll(".hud-filtered-by-string").length;
  ok(count == 1, "1 hidden " + aMethod  + " node found");
  HUDService.clearDisplay(hudId);

  // now toggle the current method off - make sure no visible message
  // nodes are logged
  setStringFilter("");
  HUDService.setFilterState(hudId, aMethod, false);
  browser.contentWindow.wrappedJSObject.console[aMethod]("foo-bar-baz");
  count = outputNode.querySelectorAll(".hud-filtered-by-type").length;
  is(count, 1, aMethod + " logging turned off, 1 message hidden");
  HUDService.clearDisplay(hudId);
  setStringFilter("");

  // test for multiple arguments.
  HUDService.clearDisplay(hudId);
  HUDService.setFilterState(hudId, aMethod, true);
  browser.contentWindow.wrappedJSObject.console[aMethod]("foo", "bar");

  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let node = jsterm.outputNode.
    querySelector(".hud-group:last-child > label:last-child");
  ok(/foo bar/.test(node.textContent), "Emitted both console arguments");
}

function testLogEntry(aOutputNode, aMatchString, aSuccessErrObj)
{
  var msgs = aOutputNode.querySelector(".hud-group").childNodes;
  for (var i = 1; i < msgs.length; i++) {
    var message = msgs[i].textContent.indexOf(aMatchString);
    if (message > -1) {
      ok(true, aSuccessErrObj.success);
      return;
    }
  }
  throw new Error(aSuccessErrObj.err);
}

// test network logging
//
// NB: After this test, the HUD (including its "jsterm" attribute) will be gone
// forever due to bug 580618!
function testNet()
{
  HUDService.setFilterState(hudId, "network", true);
  setStringFilter("");

  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let outputNode = jsterm.outputNode;
  jsterm.clearOutput();

  browser.addEventListener("load", function onTestNetLoad () {
    browser.removeEventListener("load", onTestNetLoad, true);

    executeSoon(function(){
      let group = outputNode.querySelector(".hud-group");
      is(group.childNodes.length, 4, "Four children in output");
      let outputChildren = group.childNodes;

      isnot(outputChildren[0].textContent.indexOf("test-network.html"), -1,
                                                "html page is logged");
      isnot(outputChildren[1].textContent.indexOf("testscript.js"), -1,
                                                "javascript is logged");

      let imageLogged =
        (outputChildren[2].textContent.indexOf("test-image.png") != -1 ||
         outputChildren[3].textContent.indexOf("test-image.png") != -1);
      ok(imageLogged, "image is logged");

      let logOutput = "running network console logging tests";
      let logLogged =
        (outputChildren[2].textContent.indexOf(logOutput) != -1 ||
         outputChildren[3].textContent.indexOf(logOutput) != -1);
      ok(logLogged, "log() is logged")

      testLiveFilteringForMessageTypes();
    });
  }, true);

  content.location = TEST_NETWORK_URI;
}

// General driver for filter tests.
function testLiveFiltering(callback) {
  HUDService.setFilterState(hudId, "network", true);
  setStringFilter("");

  browser.addEventListener("DOMContentLoaded", function onTestNetLoad() {
    browser.removeEventListener("DOMContentLoaded", onTestNetLoad, false);

    let display = HUDService.getDisplayByURISpec(TEST_NETWORK_URI);
    let outputNode = display.querySelector(".hud-output-node");

    function countNetworkNodes() {
      let networkNodes = outputNode.querySelectorAll(".hud-network");
      let displayedNetworkNodes = 0;
      let view = outputNode.ownerDocument.defaultView;
      for (let i = 0; i < networkNodes.length; i++) {
        let computedStyle = view.getComputedStyle(networkNodes[i], null);
        if (computedStyle.display !== "none") {
          displayedNetworkNodes++;
        }
      }

      return displayedNetworkNodes;
    }
callback(countNetworkNodes); }, false); content.location = TEST_NETWORK_URI;
}

// Tests the live filtering for message types.
function testLiveFilteringForMessageTypes() {
  testLiveFiltering(function(countNetworkNodes) {
    HUDService.setFilterState(hudId, "network", false);
    is(countNetworkNodes(), 0, "the network nodes are hidden when the " +
      "corresponding filter is switched off");

    HUDService.setFilterState(hudId, "network", true);
    isnot(countNetworkNodes(), 0, "the network nodes reappear when the " +
      "corresponding filter is switched on");

    testLiveFilteringForSearchStrings();
  });
}

// Tests the live filtering on search strings.
function testLiveFilteringForSearchStrings()
{
  testLiveFiltering(function(countNetworkNodes) {
    setStringFilter("http");
    isnot(countNetworkNodes(), 0, "the network nodes are not hidden when " +
      "the search string is set to \"http\"");

    setStringFilter("hxxp");
    is(countNetworkNodes(), 0, "the network nodes are hidden when the " +
      "search string is set to \"hxxp\"");

    setStringFilter("ht tp");
    isnot(countNetworkNodes(), 0, "the network nodes are not hidden when " +
      "the search string is set to \"ht tp\"");

    setStringFilter(" zzzz   zzzz ");
    is(countNetworkNodes(), 0, "the network nodes are hidden when the " +
      "search string is set to \" zzzz   zzzz \"");

    setStringFilter("");
    isnot(countNetworkNodes(), 0, "the network nodes are not hidden when " +
      "the search string is removed");

    setStringFilter("\u9f2c");
    is(countNetworkNodes(), 0, "the network nodes are hidden when searching " +
      "for weasels");

    setStringFilter("\u0007");
    is(countNetworkNodes(), 0, "the network nodes are hidden when searching " +
      "for the bell character");

    setStringFilter('"foo"');
    is(countNetworkNodes(), 0, "the network nodes are hidden when searching " +
      'for the string "foo"');

    setStringFilter("'foo'");
    is(countNetworkNodes(), 0, "the network nodes are hidden when searching " +
      "for the string 'foo'");

    setStringFilter("foo\"bar'baz\"boo'");
    is(countNetworkNodes(), 0, "the network nodes are hidden when searching " +
      "for the string \"foo\"bar'baz\"boo'\"");

    testTextNodeInsertion();
  });
}

function testOutputOrder()
{
  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let outputNode = jsterm.outputNode;

  jsterm.clearOutput();
  jsterm.execute("console.log('foo', 'bar');");

  let group = outputNode.querySelector(".hud-group");
  is(group.childNodes.length, 3, "Four children in output");
  let outputChildren = group.childNodes;

  let executedStringFirst =
    /console\.log\('foo', 'bar'\);/.test(outputChildren[0].childNodes[0].nodeValue);

  let outputSecond =
    /foo bar/.test(outputChildren[1].childNodes[0].nodeValue);

  ok(executedStringFirst && outputSecond, "executed string comes first");
}

function testGroups()
{
  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let outputNode = jsterm.outputNode;

  jsterm.clearOutput();

  let timestamp0 = Date.now();
  jsterm.execute("0");
  is(outputNode.querySelectorAll(".hud-group").length, 1,
    "one group exists after the first console message");

  jsterm.execute("1");
  let timestamp1 = Date.now();
  if (timestamp1 - timestamp0 < 5000) {
    is(outputNode.querySelectorAll(".hud-group").length, 1,
      "only one group still exists after the second console message");
  }

  HUD.HUDBox.lastTimestamp = 0;   // a "far past" value
  jsterm.execute("2");
  is(outputNode.querySelectorAll(".hud-group").length, 2,
    "two groups exist after the third console message");
}

function testNullUndefinedOutput()
{
  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;
  let outputNode = jsterm.outputNode;

  jsterm.clearOutput();
  jsterm.execute("null;");

  let group = outputNode.querySelector(".hud-group");
  is(group.childNodes.length, 2, "Three children in output");
  let outputChildren = group.childNodes;

  is (outputChildren[1].childNodes[0].nodeValue, "null\n",
      "'null' printed to output");

  jsterm.clearOutput();
  jsterm.execute("undefined;");

  group = outputNode.querySelector(".hud-group");
  is(group.childNodes.length, 2, "Three children in output");
  outputChildren = group.childNodes;

  is (outputChildren[1].childNodes[0].nodeValue, "undefined\n",
      "'undefined' printed to output");
}

function testJSInputAndOutputStyling() {
  let jsterm = HUDService.hudWeakReferences[hudId].get().jsterm;

  jsterm.clearOutput();
  jsterm.execute("2 + 2");

  let group = jsterm.outputNode.querySelector(".hud-group");
  let outputChildren = group.childNodes;
  let jsInputNode = outputChildren[0];
  isnot(jsInputNode.childNodes[0].nodeValue.indexOf("2 + 2"), -1,
    "JS input node contains '2 + 2'");
  isnot(jsInputNode.getAttribute("class").indexOf("jsterm-input-line"), -1,
    "JS input node is of the CSS class 'jsterm-input-line'");

  let jsOutputNode = outputChildren[1];
  isnot(jsOutputNode.childNodes[0].textContent.indexOf("4"), -1,
    "JS output node contains '4'");
  isnot(jsOutputNode.getAttribute("class").indexOf("jsterm-output-line"), -1,
    "JS output node is of the CSS class 'jsterm-output-line'");
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

function testNetworkPanel()
{
  function checkIsVisible(aPanel, aList) {
    for (let id in aList) {
      let node = aPanel.document.getElementById(id);
      let isVisible = aList[id];
      is(node.style.display, (isVisible ? "block" : "none"), id + " isVisible=" + isVisible);
    }
  }

  function checkNodeContent(aPanel, aId, aContent) {
    let node = aPanel.document.getElementById(aId);
    if (node == null) {
      ok(false, "Tried to access node " + aId + " that doesn't exist!");
    }
    else if (node.textContent.indexOf(aContent) != -1) {
      ok(true, "checking content of " + aId);
    }
    else {
      ok(false, "Got false value for " + aId + ": " + node.textContent + " doesn't have " + aContent);
    }
  }

  function checkNodeKeyValue(aPanel, aId, aKey, aValue) {
    let node = aPanel.document.getElementById(aId);

    let testHTML = '<span xmlns="http://www.w3.org/1999/xhtml" class="property-name">' + aKey + ':</span>';
    testHTML += '<span xmlns="http://www.w3.org/1999/xhtml" class="property-value">' + aValue + '</span>';
    isnot(node.innerHTML.indexOf(testHTML), -1, "checking content of " + aId);
  }

  let testDriver;
  function testGen() {
    var httpActivity = {
      url: "http://www.testpage.com",
      method: "GET",

      panels: [],
      request: {
        header: {
          foo: "bar"
        }
      },
      response: { },
      timing: {
        "REQUEST_HEADER": 0
      }
    };

    let networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);

    is (networkPanel, httpActivity.panels[0].get(), "Network panel stored on httpActivity object");
    networkPanel.panel.addEventListener("load", function onLoad() {
      networkPanel.panel.removeEventListener("load", onLoad, true);
      testDriver.next();
    }, true);
    yield;

    checkIsVisible(networkPanel, {
      requestCookie: false,
      requestFormData: false,
      requestBody: false,
      responseContainer: false,
      responseBody: false,
      responseNoBody: false,
      responseImage: false,
      responseImageCached: false
    });

    checkNodeContent(networkPanel, "header", "http://www.testpage.com");
    checkNodeContent(networkPanel, "header", "GET");
    checkNodeKeyValue(networkPanel, "requestHeadersContent", "foo", "bar");

    // Test request body.
    httpActivity.request.body = "hello world";
    networkPanel.update();
    checkIsVisible(networkPanel, {
      requestBody: true,
      requestFormData: false,
      requestCookie: false,
      responseContainer: false,
      responseBody: false,
      responseNoBody: false,
      responseImage: false,
      responseImageCached: false
    });
    checkNodeContent(networkPanel, "requestBodyContent", "hello world");

    // Test response header.
    httpActivity.timing.RESPONSE_HEADER = 1000;
    httpActivity.response.status = "999 earthquake win";
    httpActivity.response.header = {
      leaveHouses: "true"
    }
    networkPanel.update();
    checkIsVisible(networkPanel, {
      requestBody: true,
      requestFormData: false,
      requestCookie: false,
      responseContainer: true,
      responseBody: false,
      responseNoBody: false,
      responseImage: false,
      responseImageCached: false
    });

    checkNodeContent(networkPanel, "header", "999 earthquake win");
    checkNodeKeyValue(networkPanel, "responseHeadersContent", "leaveHouses", "true");
    checkNodeContent(networkPanel, "responseHeadersInfo", "1ms");

    httpActivity.timing.RESPONSE_COMPLETE = 2500;
    // This is necessary to show that the request is done.
    httpActivity.timing.TRANSACTION_CLOSE = 2500;
    networkPanel.update();

    checkIsVisible(networkPanel, {
      requestBody: true,
      requestCookie: false,
      requestFormData: false,
      responseContainer: true,
      responseBody: false,
      responseNoBody: false,
      responseImage: false,
      responseImageCached: false
    });

    httpActivity.response.isDone = true;
    networkPanel.update();

    checkNodeContent(networkPanel, "responseNoBodyInfo", "2ms");
    checkIsVisible(networkPanel, {
      requestBody: true,
      requestCookie: false,
      responseContainer: true,
      responseBody: false,
      responseNoBody: true,
      responseImage: false,
      responseImageCached: false
    });

    networkPanel.panel.hidePopup();

    // Second run: Test for cookies and response body.
    httpActivity.request.header.Cookie = "foo=bar;  hello=world";
    httpActivity.response.body = "get out here";

    networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);
    is (networkPanel, httpActivity.panels[1].get(), "Network panel stored on httpActivity object");
    networkPanel.panel.addEventListener("load", function onLoad() {
      networkPanel.panel.removeEventListener("load", onLoad, true);
      testDriver.next();
    }, true);
    yield;


    checkIsVisible(networkPanel, {
      requestBody: true,
      requestFormData: false,
      requestCookie: true,
      responseContainer: true,
      responseBody: true,
      responseNoBody: false,
      responseImage: false,
      responseImageCached: false
    });

    checkNodeKeyValue(networkPanel, "requestCookieContent", "foo", "bar");
    checkNodeKeyValue(networkPanel, "requestCookieContent", "hello", "world");
    checkNodeContent(networkPanel, "responseBodyContent", "get out here");
    checkNodeContent(networkPanel, "responseBodyInfo", "2ms");

    networkPanel.panel.hidePopup();

    // Check image request.
    httpActivity.response.header["Content-Type"] = "image/png";
    httpActivity.url =  TEST_IMG;

    networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);
    networkPanel.panel.addEventListener("load", function onLoad() {
      networkPanel.panel.removeEventListener("load", onLoad, true);
      testDriver.next();
    }, true);
    yield;

    checkIsVisible(networkPanel, {
      requestBody: true,
      requestFormData: false,
      requestCookie: true,
      responseContainer: true,
      responseBody: false,
      responseNoBody: false,
      responseImage: true,
      responseImageCached: false
    });

    let imgNode = networkPanel.document.getElementById("responseImageNode");
    is(imgNode.getAttribute("src"), TEST_IMG, "Displayed image is correct");

    function checkImageResponseInfo() {
      checkNodeContent(networkPanel, "responseImageInfo", "2ms");
      checkNodeContent(networkPanel, "responseImageInfo", "16x16px");
    }

    // Check if the image is loaded already.
    if (imgNode.width == 0) {
      imgNode.addEventListener("load", function onLoad() {
        imgNode.removeEventListener("load", onLoad, false);
        checkImageResponseInfo();
        networkPanel.panel.hidePopup();
        testDriver.next();
      }, false);
      // Wait until the image is loaded.
      yield;
    }
    else {
      checkImageResponseInfo();
      networkPanel.panel.hidePopup();
    }

    // Check cached image request.
    httpActivity.response.status = "HTTP/1.1 304 Not Modified";

    networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);
    networkPanel.panel.addEventListener("load", function onLoad() {
      networkPanel.panel.removeEventListener("load", onLoad, true);
      testDriver.next();
    }, true);
    yield;

    checkIsVisible(networkPanel, {
      requestBody: true,
      requestFormData: false,
      requestCookie: true,
      responseContainer: true,
      responseBody: false,
      responseNoBody: false,
      responseImage: false,
      responseImageCached: true
    });

    let imgNode = networkPanel.document.getElementById("responseImageCachedNode");
    is(imgNode.getAttribute("src"), TEST_IMG, "Displayed image is correct");

    networkPanel.panel.hidePopup();

    // Test sent form data.
    httpActivity.request.body = [
      "Content-Type:      application/x-www-form-urlencoded\n" +
      "Content-Length: 59\n" +
      "name=rob&age=20"
    ].join("");

    networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);
    networkPanel.panel.addEventListener("load", function onLoad() {
      networkPanel.panel.removeEventListener("load", onLoad, true);
      testDriver.next();
    }, true);
    yield;

    checkIsVisible(networkPanel, {
      requestBody: false,
      requestFormData: true,
      requestCookie: true,
      responseContainer: true,
      responseBody: false,
      responseNoBody: false,
      responseImage: false,
      responseImageCached: true
    });

    checkNodeKeyValue(networkPanel, "requestFormDataContent", "name", "rob");
    checkNodeKeyValue(networkPanel, "requestFormDataContent", "age", "20");
    networkPanel.panel.hidePopup();

    // Test no space after Content-Type:
    httpActivity.request.body = "Content-Type:application/x-www-form-urlencoded\n";

    networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);
    networkPanel.panel.addEventListener("load", function onLoad() {
      networkPanel.panel.removeEventListener("load", onLoad, true);
      testDriver.next();
    }, true);
    yield;

    checkIsVisible(networkPanel, {
      requestBody: false,
      requestFormData: true,
      requestCookie: true,
      responseContainer: true,
      responseBody: false,
      responseNoBody: false,
      responseImage: false,
      responseImageCached: true
    });

    networkPanel.panel.hidePopup();

    // Test cached data.

    // Load a Latein-1 encoded page.
    browser.addEventListener("load", function onLoad () {
      browser.removeEventListener("load", onLoad, true);
      httpActivity.charset = content.document.characterSet;
      testDriver.next();
    }, true);
    content.location = TEST_ENCODING_ISO_8859_1;

    yield;

    httpActivity.url = TEST_ENCODING_ISO_8859_1;
    httpActivity.response.header["Content-Type"] = "application/json";
    httpActivity.response.body = "";

    networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);
    networkPanel.isDoneCallback = function NP_doneCallback() {
      networkPanel.isDoneCallback = null;

      checkIsVisible(networkPanel, {
        requestBody: false,
        requestFormData: true,
        requestCookie: true,
        responseContainer: true,
        responseBody: false,
        responseBodyCached: true,
        responseNoBody: false,
        responseImage: false,
        responseImageCached: false
      });

      checkNodeContent(networkPanel, "responseBodyCachedContent", "<body>\u00fc\u00f6\u00E4</body>");
      networkPanel.panel.hidePopup();

      // Run the next test.
      testErrorOnPageReload();
    }
    yield;
  };

  testDriver = testGen();
  testDriver.next();
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

  jsterm.clearOutput();
  jsterm.execute("location;");

  let group = jsterm.outputNode.querySelector(".hud-group");

  is(group.childNodes.length, 2, "Three children in output");
  let outputChildren = group.childNodes;

  is(/location;/.test(outputChildren[0].childNodes[0].nodeValue), true,
    "'location;' written to output");

  isnot(outputChildren[1].childNodes[0].textContent.indexOf(TEST_URI), -1,
    "command was executed in the window scope");
}

function testJSTermHelper()
{
  content.location.href = TEST_URI;

  let HUD = HUDService.hudWeakReferences[hudId].get();
  let jsterm = HUD.jsterm;

  jsterm.clearOutput();
  jsterm.execute("'id=' + $('header').getAttribute('id')");
  let group = jsterm.outputNode.querySelector(".hud-group");
  is(group.childNodes[1].textContent, "id=header\n", "$() worked");

  jsterm.clearOutput();
  jsterm.execute("headerQuery = $$('h1')");
  jsterm.execute("'length=' + headerQuery.length");
  let group = jsterm.outputNode.querySelector(".hud-group");
  is(group.childNodes[3].textContent, "length=1\n", "$$() worked");

  jsterm.clearOutput();
  jsterm.execute("xpathQuery = $x('.//*', document.body);");
  jsterm.execute("'headerFound='  + (xpathQuery[0] == headerQuery[0])");
  let group = jsterm.outputNode.querySelector(".hud-group");
  is(group.childNodes[3].textContent, "headerFound=true\n", "$x() worked");

  // no jsterm.clearOutput() here as we clear the output using the clear() fn.
  jsterm.execute("clear()");
  let group = jsterm.outputNode.querySelector(".hud-group");
  is(group.childNodes[0].textContent, "undefined\n", "clear() worked");

  jsterm.clearOutput();
  jsterm.execute("'keysResult=' + (keys({b:1})[0] == 'b')");
  let group = jsterm.outputNode.querySelector(".hud-group");
  is(group.childNodes[1].textContent, "keysResult=true\n", "keys() worked");

  jsterm.clearOutput();
  jsterm.execute("'valuesResult=' + (values({b:1})[0] == 1)");
  let group = jsterm.outputNode.querySelector(".hud-group");
  is(group.childNodes[1].textContent, "valuesResult=true\n",
     "values() worked");

  jsterm.clearOutput();
  jsterm.execute("pprint({b:2, a:1})");
  let group = jsterm.outputNode.querySelector(".hud-group");
  is(group.childNodes[1].textContent, "  a: 1\n  b: 2\n", "pprint() worked");
}

function testPropertyPanel()
{
  var HUD = HUDService.hudWeakReferences[hudId].get();
  var jsterm = HUD.jsterm;

  let propPanel = jsterm.openPropertyPanel("Test", [
    1,
    /abc/,
    null,
    undefined,
    function test() {},
    {}
  ]);
  is (propPanel.treeView.rowCount, 6, "six elements shown in propertyPanel");
  propPanel.destroy();

  propPanel = jsterm.openPropertyPanel("Test2", {
    "0.02": 0,
    "0.01": 1,
    "02":   2,
    "1":    3,
    "11":   4,
    "1.2":  5,
    "1.1":  6,
    "foo":  7,
    "bar":  8
  });
  is (propPanel.treeView.rowCount, 9, "nine elements shown in propertyPanel");

  let treeRows = propPanel.treeView._rows;
  is (treeRows[0].display, "0.01: 1", "1. element is okay");
  is (treeRows[1].display, "0.02: 0", "2. element is okay");
  is (treeRows[2].display, "1: 3",    "3. element is okay");
  is (treeRows[3].display, "1.1: 6",  "4. element is okay");
  is (treeRows[4].display, "1.2: 5",  "5. element is okay");
  is (treeRows[5].display, "02: 2",   "6. element is okay");
  is (treeRows[6].display, "11: 4",   "7. element is okay");
  is (treeRows[7].display, "bar: 8",  "8. element is okay");
  is (treeRows[8].display, "foo: 7",  "9. element is okay");
  propPanel.destroy();
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

// Test for bug 588730: Adding a text node to an existing label element causes
// warnings
function testTextNodeInsertion() {
  HUDService.clearDisplay(hudId);

  let display = HUDService.getDisplayByURISpec(TEST_NETWORK_URI);
  let outputNode = display.querySelector(".hud-output-node");

  let label = document.createElementNS(
    "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul", "label");
  outputNode.appendChild(label);

  let error = false;
  let listener = {
    observe: function(aMessage) {
      let messageText = aMessage.message;
      if (messageText.indexOf("JavaScript Warning") !== -1) {
        error = true;
      }
    }
  };

  let nsIConsoleServiceClass = Cc["@mozilla.org/consoleservice;1"];
  let nsIConsoleService = nsIConsoleServiceClass.getService(Ci.
    nsIConsoleService);
  nsIConsoleService.registerListener(listener);

  // This shouldn't fail.
  label.appendChild(document.createTextNode("foo"));

  executeSoon(function() {
    nsIConsoleService.unregisterListener(listener);
    ok(!error, "no error when adding text nodes as children of labels");

    testPageReload();
  });
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

    testNetworkPanel();
  }, false);

  content.location.reload();
}

function testErrorOnPageReload() {
  // see bug 580030: the error handler fails silently after page reload.
  // https://bugzilla.mozilla.org/show_bug.cgi?id=580030

  var consoleObserver = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

    observe: function (aMessage)
    {
      // we ignore errors we don't care about
      if (!(aMessage instanceof Ci.nsIScriptError) ||
        aMessage.category != "content javascript") {
        return;
      }

      Services.console.unregisterListener(this);

      const successMsg = "Found the error message after page reload";
      const errMsg = "Could not get the error message after page reload";

      const successMsgErrorLine = "Error line is correct";
      const errMsgErrorLine = "Error line is incorrect";

      var display = HUDService.getDisplayByURISpec(content.location.href);
      var outputNode = display.querySelectorAll(".hud-output-node")[0];

      executeSoon(function () {
        testLogEntry(outputNode, "fooBazBaz",
          { success: successMsg, err: errMsg });

        testLogEntry(outputNode, "Line: 14, Column: 0",
          { success: successMsgErrorLine, err: errMsgErrorLine });

        testDuplicateError();
      });
    }
  };

  var pageReloaded = false;
  browser.addEventListener("load", function() {
    if (!pageReloaded) {
      pageReloaded = true;
      content.location.reload();
      return;
    }

    browser.removeEventListener("load", arguments.callee, true);

    // dispatch a click event to the button in the test page and listen for
    // errors.

    var contentDocument = browser.contentDocument.wrappedJSObject;
    var button = contentDocument.getElementsByTagName("button")[0];
    var clickEvent = contentDocument.createEvent("MouseEvents");
    clickEvent.initMouseEvent("click", true, true,
      browser.contentWindow.wrappedJSObject, 0, 0, 0, 0, 0, false, false,
      false, false, 0, null);

    Services.console.registerListener(consoleObserver);
    button.dispatchEvent(clickEvent);
  }, true);

  content.location = TEST_ERROR_URI;
}

function testDuplicateError() {
  // see bug 582201 - exceptions show twice in WebConsole
  // https://bugzilla.mozilla.org/show_bug.cgi?id=582201

  var consoleObserver = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

    observe: function (aMessage)
    {
      // we ignore errors we don't care about
      if (!(aMessage instanceof Ci.nsIScriptError) ||
        aMessage.category != "content javascript") {
        return;
      }

      Services.console.unregisterListener(this);

      var display = HUDService.getDisplayByURISpec(content.location.href);
      var outputNode = display.querySelectorAll(".hud-output-node")[0];

      executeSoon(function () {
        var text = outputNode.textContent;
        var error1pos = text.indexOf("fooDuplicateError1");
        ok(error1pos > -1, "found fooDuplicateError1");
        if (error1pos > -1) {
          ok(text.indexOf("fooDuplicateError1", error1pos + 1) == -1,
            "no duplicate for fooDuplicateError1");
        }

        ok(text.indexOf("test-duplicate-error.html") > -1,
          "found test-duplicate-error.html");

        text = null;
        testWebConsoleClose();
      });
    }
  };

  Services.console.registerListener(consoleObserver);
  content.location = TEST_DUPLICATE_ERROR_URI;
}

/**
 * Unit test for bug 580001:
 * 'Close console after completion causes error "inputValue is undefined"'
 */
function testWebConsoleClose() {
  let display = HUDService.getDisplayByURISpec(content.location.href);
  let input = display.querySelector(".jsterm-input-node");

  let errorWhileClosing = false;
  function errorListener(evt) {
    errorWhileClosing = true;
  }
  window.addEventListener("error", errorListener, false);

  // Focus the inputNode and perform the keycombo to close the WebConsole.
  input.focus();
  EventUtils.synthesizeKey("k", { accelKey: true, shiftKey: true });

  // We can't test for errors right away, because the error occures after a
  // setTimeout(..., 0) in the WebConsole code.
  executeSoon(function() {
    window.removeEventListener("error", errorListener, false);
    is (errorWhileClosing, false, "no error while closing the WebConsole");
    testEnd();
  });
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
      testInputFocus();
      testGetContentWindowFromHUDId();

      testConsoleLoggingAPI("log");
      testConsoleLoggingAPI("info");
      testConsoleLoggingAPI("warn");
      testConsoleLoggingAPI("error");
      testConsoleLoggingAPI("exception");

      // ConsoleStorageTests
      testCreateDisplay();
      testRecordEntry();
      testRecordManyEntries();
      testIteration();
      testConsoleHistory();
      testOutputOrder();
      testGroups();
      testNullUndefinedOutput();
      testJSInputAndOutputStyling();
      testExecutionScope();
      testCompletion();
      testPropertyProvider();
      testPropertyPanel();
      testJSTermHelper();

      // NOTE: Put any sync test above this comment.
      //
      // There is a set of async tests that have to run one after another.
      // Currently, when one test is done the next one is called from within the
      // test function until testEnd() is called that terminates the test.
      testNet();
    });
  }, false);
}
