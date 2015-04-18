"use strict";

const { interfaces: Ci, classes: Cc, utils: Cu, results: Cr } = Components;

let {WebRequest} = Cu.import("resource://gre/modules/WebRequest.jsm", {});

const BASE = "http://example.com/browser/toolkit/modules/tests/browser";
const URL = BASE + "/file_WebRequest_page1.html";

let expected_browser;

function checkType(details)
{
  let expected_type = "???";
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
  is(details.type, expected_type, "resource type is correct");
}

let windowIDs = new Map();

let requested = [];

function onBeforeRequest(details)
{
  info(`onBeforeRequest ${details.url}`);
  if (details.url.startsWith(BASE)) {
    requested.push(details.url);

    is(details.browser, expected_browser, "correct <browser> element");
    checkType(details);

    windowIDs.set(details.url, details.windowId);
    if (details.url.indexOf("page2") != -1) {
      let page1id = windowIDs.get(URL);
      ok(details.windowId != page1id, "sub-frame gets its own window ID");
      is(details.parentWindowId, page1id, "parent window id is correct");
    }
  }
  if (details.url.indexOf("_bad.") != -1) {
    return {cancel: true};
  }
}

let sendHeaders = [];

function onBeforeSendHeaders(details)
{
  info(`onBeforeSendHeaders ${details.url}`);
  if (details.url.startsWith(BASE)) {
    sendHeaders.push(details.url);

    is(details.browser, expected_browser, "correct <browser> element");
    checkType(details);
  }
  if (details.url.indexOf("_redirect.") != -1) {
    return {redirectUrl: details.url.replace("_redirect.", "_good.")};
  }
}

let headersReceived = [];

function onResponseStarted(details)
{
  if (details.url.startsWith(BASE)) {
    headersReceived.push(details.url);
  }
}

const expected_requested = [BASE + "/file_WebRequest_page1.html",
                            BASE + "/file_style_good.css",
                            BASE + "/file_style_bad.css",
                            BASE + "/file_style_redirect.css",
                            BASE + "/file_image_good.png",
                            BASE + "/file_image_bad.png",
                            BASE + "/file_image_redirect.png",
                            BASE + "/file_script_good.js",
                            BASE + "/file_script_bad.js",
                            BASE + "/file_script_redirect.js",
                            BASE + "/file_script_xhr.js",
                            BASE + "/file_WebRequest_page2.html",
                            BASE + "/nonexistent_script_url.js",
                            BASE + "/xhr_resource"];

const expected_sendHeaders = [BASE + "/file_WebRequest_page1.html",
                              BASE + "/file_style_good.css",
                              BASE + "/file_style_redirect.css",
                              BASE + "/file_image_good.png",
                              BASE + "/file_image_redirect.png",
                              BASE + "/file_script_good.js",
                              BASE + "/file_script_redirect.js",
                              BASE + "/file_script_xhr.js",
                              BASE + "/file_WebRequest_page2.html",
                              BASE + "/nonexistent_script_url.js",
                              BASE + "/xhr_resource"];

const expected_headersReceived = [BASE + "/file_WebRequest_page1.html",
                                  BASE + "/file_style_good.css",
                                  BASE + "/file_image_good.png",
                                  BASE + "/file_script_good.js",
                                  BASE + "/file_script_xhr.js",
                                  BASE + "/file_WebRequest_page2.html",
                                  BASE + "/nonexistent_script_url.js",
                                  BASE + "/xhr_resource"];

function removeDupes(list)
{
  let j = 0;
  for (let i = 1; i < list.length; i++) {
    if (list[i] != list[j]) {
      j++;
      if (i != j) {
        list[j] = list[i];
      }
    }
  }
  list.length = j + 1;
}

function compareLists(list1, list2, kind)
{
  list1.sort();
  removeDupes(list1);
  list2.sort();
  removeDupes(list2);
  is(String(list1), String(list2), `${kind} URLs correct`);
}

function* test_once()
{
  WebRequest.onBeforeRequest.addListener(onBeforeRequest, null, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, null, ["blocking"]);
  WebRequest.onResponseStarted.addListener(onResponseStarted);

  gBrowser.selectedTab = gBrowser.addTab();
  let browser = gBrowser.selectedBrowser;
  expected_browser = browser;

  yield waitForLoad();

  browser.messageManager.loadFrameScript(`data:,content.location = "${URL}";`, false);

  yield waitForLoad();

  let win = browser.contentWindow.wrappedJSObject;
  is(win.success, 2, "Good script ran");
  is(win.failure, undefined, "Failure script didn't run");

  let style = browser.contentWindow.getComputedStyle(browser.contentDocument.getElementById("test"), null);
  is(style.getPropertyValue("color"), "rgb(255, 0, 0)", "Good CSS loaded");

  gBrowser.removeCurrentTab();

  compareLists(requested, expected_requested, "requested");
  compareLists(sendHeaders, expected_sendHeaders, "sendHeaders");
  compareLists(headersReceived, expected_headersReceived, "headersReceived");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
}

// Run the test twice to make sure it works with caching.
add_task(test_once);
add_task(test_once);

function waitForLoad(browser = gBrowser.selectedBrowser) {
  return new Promise(resolve => {
    browser.addEventListener("load", function listener() {
      browser.removeEventListener("load", listener, true);
      resolve();
    }, true);
  });
}
