/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Julian Viereck <jviereck@mozilla.com>
 *  Patrick Walton <pcwalton@mozilla.com>
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

// Tests that network log messages bring up the network panel.

const TEST_NETWORK_REQUEST_URI = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-network-request.html";

const TEST_IMG = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/test-image.png";

const TEST_DATA_JSON_CONTENT =
  '{ id: "test JSON data", myArray: [ "foo", "bar", "baz", "biff" ] }';

let lastRequest = null;

function test()
{
  addTab("data:text/html,Web Console network logging tests");

  browser.addEventListener("load", function() {
    browser.removeEventListener("load", arguments.callee, true);

    openConsole();
    is(HUDService.displaysIndex().length, 1, "Web Console was opened");

    hudId = HUDService.displaysIndex()[0];
    hud = HUDService.getHeadsUpDisplay(hudId);

    HUDService.lastFinishedRequestCallback = function(aRequest) {
      lastRequest = aRequest;
    };

    executeSoon(testPageLoad);
  }, true);
}

function testPageLoad()
{
  browser.addEventListener("load", function(aEvent) {
    browser.removeEventListener(aEvent.type, arguments.callee, true);

    // Check if page load was logged correctly.
    ok(lastRequest, "Page load was logged");
    is(lastRequest.url, TEST_NETWORK_REQUEST_URI,
      "Logged network entry is page load");
    is(lastRequest.method, "GET", "Method is correct");
    ok(!("body" in lastRequest.request), "No request body was stored");
    ok(!("body" in lastRequest.response), "No response body was stored");
    ok(!lastRequest.response.listener, "No response listener is stored");

    lastRequest = null;
    executeSoon(testPageLoadBody);
  }, true);

  content.location = TEST_NETWORK_REQUEST_URI;
}

function testPageLoadBody()
{
  // Turn on logging of request bodies and check again.
  HUDService.saveRequestAndResponseBodies = true;
  browser.addEventListener("load", function(aEvent) {
    browser.removeEventListener(aEvent.type, arguments.callee, true);

    ok(lastRequest, "Page load was logged again");
    is(lastRequest.response.body.indexOf("<!DOCTYPE HTML>"), 0,
      "Response body's beginning is okay");

    lastRequest = null;
    executeSoon(testXhrGet);
  }, true);

  content.location.reload();
}

function testXhrGet()
{
  let callback = function() {
    ok(lastRequest, "testXhrGet() was logged");
    is(lastRequest.method, "GET", "Method is correct");
    is(lastRequest.request.body, null, "No request body was sent");
    is(lastRequest.response.body, TEST_DATA_JSON_CONTENT,
      "Response is correct");

    lastRequest = null;
    executeSoon(testXhrPost);
  };

  // Start the XMLHttpRequest() GET test.
  content.wrappedJSObject.testXhrGet(function() {
    // Use executeSoon here as the xhr callback is invoked before the network
    // observer detected that the request is completly done and the
    // HUDService.lastFinishedRequest is set. executeSoon solves that problem.
    executeSoon(callback);
  });
}

function testXhrPost()
{
  let callback = function() {
    ok(lastRequest, "testXhrPost() was logged");
    is(lastRequest.method, "POST", "Method is correct");
    is(lastRequest.request.body, "Hello world!",
      "Request body was logged");
    is(lastRequest.response.body, TEST_DATA_JSON_CONTENT,
      "Response is correct");

    lastRequest = null;
    executeSoon(testFormSubmission);
  };

  // Start the XMLHttpRequest() POST test.
  content.wrappedJSObject.testXhrPost(function() {
    executeSoon(callback);
  });
}

function testFormSubmission()
{
  // Start the form submission test. As the form is submitted, the page is
  // loaded again. Bind to the load event to catch when this is done.
  browser.addEventListener("load", function(aEvent) {
    browser.removeEventListener(aEvent.type, arguments.callee, true);

    ok(lastRequest, "testFormSubmission() was logged");
    is(lastRequest.method, "POST", "Method is correct");
    isnot(lastRequest.request.body.
      indexOf("Content-Type: application/x-www-form-urlencoded"), -1,
      "Content-Type is correct");
    isnot(lastRequest.request.body.
      indexOf("Content-Length: 20"), -1, "Content-length is correct");
    isnot(lastRequest.request.body.
      indexOf("name=foo+bar&age=144"), -1, "Form data is correct");
    ok(lastRequest.response.body.indexOf("<!DOCTYPE HTML>") == 0,
      "Response body's beginning is okay");

    executeSoon(testNetworkPanel);
  }, true);

  let form = content.document.querySelector("form");
  ok(form, "we have the HTML form");
  form.submit();
}

function testNetworkPanel()
{
  // Open the NetworkPanel. The functionality of the NetworkPanel is tested
  // within separate test files.
  let filterBox = hud.querySelector(".hud-filter-box");
  let networkPanel = HUDService.openNetworkPanel(filterBox, lastRequest);
  is(networkPanel, lastRequest.panels[0].get(),
    "Network panel stored on lastRequest object");

  networkPanel.panel.addEventListener("load", function(aEvent) {
    networkPanel.panel.removeEventListener(aEvent.type, arguments.callee,
      true);

    ok(true, "NetworkPanel was opened");

    // All tests are done. Shutdown.
    networkPanel.panel.hidePopup();
    lastRequest = null;
    HUDService.lastFinishedRequestCallback = null;
    executeSoon(finishTest);
  }, true);
}

