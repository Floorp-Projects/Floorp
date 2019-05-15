"use strict";

var {WebRequest} = ChromeUtils.import("resource://gre/modules/WebRequest.jsm");

const server = createHttpServer({hosts: ["example.com"]});
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

const expected_urls = [BASE + "/file_style_good.css",
                       BASE + "/file_style_bad.css",
                       BASE + "/file_style_redirect.css"];

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

add_task(async function setup() {
  // Disable rcwn to make cache behavior deterministic.
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);
});

add_task(async function filter_urls() {
  let filter = {urls: new MatchPatternSet(["*://*/*_style_*"])};

  WebRequest.onBeforeRequest.addListener(onBeforeRequest, filter, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, filter, ["blocking"]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, filter);

  let contentPage = await ExtensionTestUtils.loadContentPage(URL);
  await contentPage.close();

  compareLists(requested, expected_urls, "requested");
  compareLists(sendHeaders, expected_urls, "sendHeaders");
  compareLists(completed, expected_urls, "completed");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
});

add_task(async function filter_types() {
  let filter = {types: ["stylesheet"]};

  WebRequest.onBeforeRequest.addListener(onBeforeRequest, filter, ["blocking"]);
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, filter, ["blocking"]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, filter);

  let contentPage = await ExtensionTestUtils.loadContentPage(URL);
  await contentPage.close();

  compareLists(requested, expected_urls, "requested");
  compareLists(sendHeaders, expected_urls, "sendHeaders");
  compareLists(completed, expected_urls, "completed");

  WebRequest.onBeforeRequest.removeListener(onBeforeRequest);
  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
});
