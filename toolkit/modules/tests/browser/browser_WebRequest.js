"use strict";

var { interfaces: Ci, classes: Cc, utils: Cu, results: Cr } = Components;

var {WebRequest} = Cu.import("resource://gre/modules/WebRequest.jsm", {});

const BASE = "http://example.com/browser/toolkit/modules/tests/browser";
const URL = BASE + "/file_WebRequest_page1.html";

var expected_browser;

function checkType(details) {
  let expected_type = "???";
  if (details.url.indexOf("style") != -1) {
    expected_type = "stylesheet";
  } else if (details.url.indexOf("image") != -1) {
    expected_type = "image";
  } else if (details.url.indexOf("script") != -1) {
    expected_type = "script";
  } else if (details.url.indexOf("page1") != -1) {
    expected_type = "main_frame";
  } else if (/page2|_redirection\.|dummy_page/.test(details.url)) {
    expected_type = "sub_frame";
  } else if (details.url.indexOf("xhr") != -1) {
    expected_type = "xmlhttprequest";
  }
  is(details.type, expected_type, "resource type is correct");
}

var windowIDs = new Map();

var requested = [];

function onBeforeRequest(details) {
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

      is(details.frameAncestors.length, 1, "correctly has only one ancestor");
      let ancestor = details.frameAncestors[0];
      ok(ancestor.url.includes("page1"), "parent window url seems correct");
      is(ancestor.frameId, page1id, "parent window id is correct");
    }
  }
  if (details.url.indexOf("_bad.") != -1) {
    return {cancel: true};
  }
  return undefined;
}

var sendHeaders = [];

function onBeforeSendHeaders(details) {
  info(`onBeforeSendHeaders ${details.url}`);
  if (details.url.startsWith(BASE)) {
    sendHeaders.push(details.url);

    is(details.browser, expected_browser, "correct <browser> element");
    checkType(details);

    let id = windowIDs.get(details.url);
    is(id, details.windowId, "window ID same in onBeforeSendHeaders as onBeforeRequest");
  }
  if (details.url.indexOf("_redirect.") != -1) {
    return {redirectUrl: details.url.replace("_redirect.", "_good.")};
  }
  return undefined;
}

var beforeRedirect = [];

function onBeforeRedirect(details) {
  info(`onBeforeRedirect ${details.url} -> ${details.redirectUrl}`);
  checkType(details);
  if (details.url.startsWith(BASE)) {
    beforeRedirect.push(details.url);

    is(details.browser, expected_browser, "correct <browser> element");
    checkType(details);

    let expectedUrl = details.url.replace("_redirect.", "_good.").replace(/\w+_redirection\..*/, "dummy_page.html");
    is(details.redirectUrl, expectedUrl, "Correct redirectUrl value");
  }
  let id = windowIDs.get(details.url);
  is(id, details.windowId, "window ID same in onBeforeRedirect as onBeforeRequest");
  // associate stored windowId with final url
  windowIDs.set(details.redirectUrl, details.windowId);
  return {};
}

var headersReceived = [];

function onResponseStarted(details) {
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
                            BASE + "/WebRequest_redirection.sjs",
                            BASE + "/dummy_page.html",
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
                              BASE + "/WebRequest_redirection.sjs",
                              BASE + "/dummy_page.html",
                              BASE + "/xhr_resource"];

const expected_beforeRedirect = expected_sendHeaders.filter(u => /_redirect\./.test(u))
                                  .concat(BASE + "/WebRequest_redirection.sjs");

const expected_headersReceived = [BASE + "/file_WebRequest_page1.html",
                                  BASE + "/file_style_good.css",
                                  BASE + "/file_image_good.png",
                                  BASE + "/file_script_good.js",
                                  BASE + "/file_script_xhr.js",
                                  BASE + "/file_WebRequest_page2.html",
                                  BASE + "/nonexistent_script_url.js",
                                  BASE + "/dummy_page.html",
                                  BASE + "/xhr_resource"];

function removeDupes(list) {
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

function compareLists(list1, list2, kind) {
  list1.sort();
  removeDupes(list1);
  list2.sort();
  removeDupes(list2);
  is(String(list1), String(list2), `${kind} URLs correct`);
}

async function test_once() {
  WebRequest.onBeforeRequest.addListener(onBeforeRequest, null, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, null, ["blocking"]);
  WebRequest.onBeforeRedirect.addListener(onBeforeRedirect);
  WebRequest.onResponseStarted.addListener(onResponseStarted);

  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
    async function(browser) {
      expected_browser = browser;
      BrowserTestUtils.loadURI(browser, URL);
      await BrowserTestUtils.browserLoaded(expected_browser);

      expected_browser = null;

      await ContentTask.spawn(browser, null, function() {
        let win = content.wrappedJSObject;
        is(win.success, 2, "Good script ran");
        is(win.failure, undefined, "Failure script didn't run");

        let style =
          content.getComputedStyle(content.document.getElementById("test"));
        is(style.getPropertyValue("color"), "rgb(255, 0, 0)", "Good CSS loaded");
      });
    });

  compareLists(requested, expected_requested, "requested");
  compareLists(sendHeaders, expected_sendHeaders, "sendHeaders");
  compareLists(beforeRedirect, expected_beforeRedirect, "beforeRedirect");
  compareLists(headersReceived, expected_headersReceived, "headersReceived");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onBeforeRedirect.removeListener(onBeforeRedirect);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
}

// Run the test twice to make sure it works with caching.
add_task(test_once);
add_task(test_once);
