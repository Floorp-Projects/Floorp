const BASE = "http://mochi.test:8888/tests/toolkit/components/extensions/test/mochitest";

var savedTabId = -1;

function checkType(details)
{
  var expected_type = "???";
  if (details.url.indexOf("style") != -1) {
    expected_type = "stylesheet";
  } else if (details.url.indexOf("image") != -1) {
    expected_type = "image";
  } else if (details.url.indexOf("script") != -1) {
    expected_type = "script";
  } else if (details.url.indexOf("page1") != -1) {
    expected_type = "main_frame";
  } else if (details.url.indexOf("page2") != -1) {
    expected_type = "sub_frame";
  } else if (details.url.indexOf("xhr") != -1) {
    expected_type = "xmlhttprequest";
  }
  browser.test.assertEq(details.type, expected_type, "resource type is correct");
}

var frameIDs = new Map();

var requested = [];

function onBeforeRequest(details)
{
  browser.test.log(`onBeforeRequest ${details.url}`);
  if (details.url.startsWith(BASE)) {
    requested.push(details.url);

    if (savedTabId == -1) {
      browser.test.assertTrue(details.tabId !== undefined, "tab ID defined");
      savedTabId = details.tabId;
    }

    browser.test.assertEq(details.tabId, savedTabId, "correct tab ID");
    checkType(details);

    frameIDs.set(details.url, details.frameId);
    if (details.url.indexOf("page1") != -1) {
      browser.test.assertEq(details.frameId, 0, "frame ID correct");
      browser.test.assertEq(details.parentFrameId, -1, "parent frame ID correct");
    }
    if (details.url.indexOf("page2") != -1) {
      browser.test.assertTrue(details.frameId != 0, "sub-frame gets its own frame ID");
      browser.test.assertTrue(details.frameId !== undefined, "sub-frame ID defined");
      browser.test.assertEq(details.parentFrameId, 0, "parent frame id is correct");
    }
  }
  if (details.url.indexOf("_bad.") != -1) {
    return {cancel: true};
  }
}

var sendHeaders = [];

function onBeforeSendHeaders(details)
{
  browser.test.log(`onBeforeSendHeaders ${details.url}`);
  if (details.url.startsWith(BASE)) {
    sendHeaders.push(details.url);

    browser.test.assertEq(details.tabId, savedTabId, "correct tab ID");
    checkType(details);

    var id = frameIDs.get(details.url);
    browser.test.assertEq(id, details.frameId, "frame ID same in onBeforeSendHeaders as onBeforeRequest");
  }
  if (details.url.indexOf("_redirect.") != -1) {
    return {redirectUrl: details.url.replace("_redirect.", "_good.")};
  }
}

var headersReceived = [];

function onResponseStarted(details)
{
  if (details.url.startsWith(BASE)) {
    headersReceived.push(details.url);
  }
}

browser.webRequest.onBeforeRequest.addListener(onBeforeRequest, {urls: "<all_urls>"}, ["blocking"]);
browser.webRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, {urls: "<all_urls>"}, ["blocking"]);
browser.webRequest.onResponseStarted.addListener(onResponseStarted, {urls: "<all_urls>"});

function onTestMessage()
{
  browser.test.sendMessage("results", [requested, sendHeaders, headersReceived]);
}

browser.test.onMessage.addListener(onTestMessage);

browser.test.sendMessage("ready");
