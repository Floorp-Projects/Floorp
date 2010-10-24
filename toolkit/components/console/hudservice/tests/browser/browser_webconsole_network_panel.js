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
 *  Mihai Sucan <mihai.sucan@gmail.com>
 *  Rob Campbell <rcampbell@mozilla.com>
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

// Tests that the network panel works.

const TEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-console.html";
const TEST_IMG = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-image.png";
const TEST_ENCODING_ISO_8859_1 = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-encoding-ISO-8859-1.html";

let testDriver;

function test() {
  addTab(TEST_URI);
  browser.addEventListener("DOMContentLoaded", testNetworkPanel, false);
}

function testNetworkPanel() {
  browser.removeEventListener("DOMContentLoaded", testNetworkPanel,
                              false);
  openConsole();

  testDriver = testGen();
  testDriver.next();
}

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

function testGen() {
  let hudId = HUDService.displaysIndex()[0];
  let hudBox = HUDService.getHeadsUpDisplay(hudId);
  let filterBox = hudBox.querySelector(".hud-filter-box");

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
    "Content-Type": "text/html",
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
  httpActivity.url = TEST_IMG;

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

  // Load a Latin-1 encoded page.
  browser.addEventListener("load", function onLoad () {
    browser.removeEventListener("load", onLoad, true);
    httpActivity.charset = content.document.characterSet;
    testDriver.next();
  }, true);
  browser.contentWindow.wrappedJSObject.document.location = TEST_ENCODING_ISO_8859_1;

  yield;

  httpActivity.url = TEST_ENCODING_ISO_8859_1;
  httpActivity.response.header["Content-Type"] = "application/json";
  httpActivity.response.body = "";

  networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);
  networkPanel.isDoneCallback = function NP_doneCallback() {
    networkPanel.isDoneCallback = null;
    testDriver.next();
  }

  yield;

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

  // Test a response with a content type that can't be displayed in the
  // NetworkPanel.
  httpActivity.response.header["Content-Type"] = "application/x-shockwave-flash";

  networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);
  networkPanel.isDoneCallback = function NP_doneCallback() {
    networkPanel.isDoneCallback = null;
    testDriver.next();
  }

  yield;

  checkIsVisible(networkPanel, {
    requestBody: false,
    requestFormData: true,
    requestCookie: true,
    responseContainer: true,
    responseBody: false,
    responseBodyCached: false,
    responseBodyUnknownType: true,
    responseNoBody: false,
    responseImage: false,
    responseImageCached: false
  });

  let responseString = HUDService.getFormatStr("NetworkPanel.responseBodyUnableToDisplay.content", ["application/x-shockwave-flash"]);
  checkNodeContent(networkPanel, "responseBodyUnknownTypeContent", responseString);
  networkPanel.panel.hidePopup();

  /*

  // This test disabled. See bug 603620.

  // Test if the NetworkPanel figures out the content type based on an URL as
  // well.
  delete httpActivity.response.header["Content-Type"];
  httpActivity.url = "http://www.test.com/someCrazyFile.swf?done=right&ending=txt";

  networkPanel = HUDService.openNetworkPanel(filterBox, httpActivity);
  networkPanel.isDoneCallback = function NP_doneCallback() {
    networkPanel.isDoneCallback = null;
    testDriver.next();
  }

  yield;

  checkIsVisible(networkPanel, {
    requestBody: false,
    requestFormData: true,
    requestCookie: true,
    responseContainer: true,
    responseBody: false,
    responseBodyCached: false,
    responseBodyUnknownType: true,
    responseNoBody: false,
    responseImage: false,
    responseImageCached: false
  });

  // Systems without Flash installed will return an empty string here. Ignore.
  if (networkPanel.document.getElementById("responseBodyUnknownTypeContent").textContent !== "")
    checkNodeContent(networkPanel, "responseBodyUnknownTypeContent", responseString);
  else
    ok(true, "Flash not installed");

  networkPanel.panel.hidePopup(); */

  // All done!
  finishTest();
}
