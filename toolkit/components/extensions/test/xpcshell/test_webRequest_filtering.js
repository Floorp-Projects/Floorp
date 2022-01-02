"use strict";

var { WebRequest } = ChromeUtils.import(
  "resource://gre/modules/WebRequest.jsm"
);

var { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

const server = createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

const BASE = "http://example.com/data/";
const URL = BASE + "/file_WebRequest_page2.html";

var requested = [];

function onBeforeRequest(details) {
  info(`onBeforeRequest ${details.url}`);
  if (details.url.startsWith(BASE)) {
    requested.push(details.url);
  }
}

var sendHeaders = [];

function onBeforeSendHeaders(details) {
  info(`onBeforeSendHeaders ${details.url}`);
  if (details.url.startsWith(BASE)) {
    sendHeaders.push(details.url);
  }
}

var completed = [];

function onResponseStarted(details) {
  if (details.url.startsWith(BASE)) {
    completed.push(details.url);
  }
}

const expected_urls = [
  BASE + "/file_style_good.css",
  BASE + "/file_style_bad.css",
  BASE + "/file_style_redirect.css",
];

function resetExpectations() {
  requested.length = 0;
  sendHeaders.length = 0;
  completed.length = 0;
}

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
  equal(String(list1), String(list2), `${kind} URLs correct`);
}

async function openAndCloseContentPage(url) {
  let contentPage = await ExtensionTestUtils.loadContentPage(URL);
  // Clear the sheet cache so that it doesn't interact with following tests: A
  // stylesheet with the same URI loaded from the same origin doesn't otherwise
  // guarantee that onBeforeRequest and so on happen, because it may not need
  // to go through necko at all.
  await contentPage.spawn(null, () =>
    content.windowUtils.clearSharedStyleSheetCache()
  );
  await contentPage.close();
}

add_task(async function setup() {
  // Disable rcwn to make cache behavior deterministic.
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);

  // When WebRequest.jsm is used directly instead of through ext-webRequest.js,
  // ExtensionParent.apiManager is not automatically initialized. Do it here.
  await ExtensionParent.apiManager.lazyInit();
});

add_task(async function filter_urls() {
  let filter = { urls: new MatchPatternSet(["*://*/*_style_*"]) };

  WebRequest.onBeforeRequest.addListener(onBeforeRequest, filter, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, filter, [
    "blocking",
  ]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, filter);

  await openAndCloseContentPage(URL);

  compareLists(requested, expected_urls, "requested");
  compareLists(sendHeaders, expected_urls, "sendHeaders");
  compareLists(completed, expected_urls, "completed");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
});

add_task(async function filter_types() {
  resetExpectations();
  let filter = { types: ["stylesheet"] };

  WebRequest.onBeforeRequest.addListener(onBeforeRequest, filter, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, filter, [
    "blocking",
  ]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, filter);

  await openAndCloseContentPage(URL);

  compareLists(requested, expected_urls, "requested");
  compareLists(sendHeaders, expected_urls, "sendHeaders");
  compareLists(completed, expected_urls, "completed");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
});

add_task(async function filter_windowId() {
  resetExpectations();
  // Check that adding windowId will exclude non-matching requests.
  // test_ext_webrequest_filter.html provides coverage for matching requests.
  let filter = { urls: new MatchPatternSet(["*://*/*_style_*"]), windowId: 0 };

  WebRequest.onBeforeRequest.addListener(onBeforeRequest, filter, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, filter, [
    "blocking",
  ]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, filter);

  await openAndCloseContentPage(URL);

  compareLists(requested, [], "requested");
  compareLists(sendHeaders, [], "sendHeaders");
  compareLists(completed, [], "completed");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
});

add_task(async function filter_tabId() {
  resetExpectations();
  // Check that adding tabId will exclude non-matching requests.
  // test_ext_webrequest_filter.html provides coverage for matching requests.
  let filter = { urls: new MatchPatternSet(["*://*/*_style_*"]), tabId: 0 };

  WebRequest.onBeforeRequest.addListener(onBeforeRequest, filter, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, filter, [
    "blocking",
  ]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, filter);

  await openAndCloseContentPage(URL);

  compareLists(requested, [], "requested");
  compareLists(sendHeaders, [], "sendHeaders");
  compareLists(completed, [], "completed");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
});
