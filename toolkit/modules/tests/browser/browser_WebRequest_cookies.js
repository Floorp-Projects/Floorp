"use strict";

var { interfaces: Ci, classes: Cc, utils: Cu, results: Cr } = Components;

var {WebRequest} = Cu.import("resource://gre/modules/WebRequest.jsm", {});

const BASE = "http://example.com/browser/toolkit/modules/tests/browser";
const URL = BASE + "/WebRequest_dynamic.sjs";

var countBefore = 0;
var countAfter = 0;

function onBeforeSendHeaders(details) {
  if (details.url != URL) {
    return undefined;
  }

  countBefore++;

  info(`onBeforeSendHeaders ${details.url}`);
  let found = false;
  let headers = [];
  for (let {name, value} of details.requestHeaders) {
    info(`Saw header ${name} '${value}'`);
    if (name == "Cookie") {
      is(value, "foopy=1", "Cookie is correct");
      headers.push({name, value: "blinky=1"});
      found = true;
    } else {
      headers.push({name, value});
    }
  }
  ok(found, "Saw cookie header");

  return {requestHeaders: headers};
}

function onResponseStarted(details) {
  if (details.url != URL) {
    return;
  }

  countAfter++;

  info(`onResponseStarted ${details.url}`);
  let found = false;
  for (let {name, value} of details.responseHeaders) {
    info(`Saw header ${name} '${value}'`);
    if (name == "Set-Cookie") {
      is(value, "dinky=1", "Cookie is correct");
      found = true;
    }
  }
  ok(found, "Saw cookie header");
}

add_task(function* filter_urls() {
  // First load the URL so that we set cookie foopy=1.
  gBrowser.selectedTab = gBrowser.addTab(URL);
  yield waitForLoad();
  gBrowser.removeCurrentTab();

  // Now load with WebRequest set up.
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, null, ["blocking"]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, null);

  gBrowser.selectedTab = gBrowser.addTab(URL);

  yield waitForLoad();

  gBrowser.removeCurrentTab();

  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);

  is(countBefore, 1, "onBeforeSendHeaders hit once");
  is(countAfter, 1, "onResponseStarted hit once");
});

function waitForLoad(browser = gBrowser.selectedBrowser) {
  return new Promise(resolve => {
    browser.addEventListener("load", function listener() {
      browser.removeEventListener("load", listener, true);
      resolve();
    }, true);
  });
}
