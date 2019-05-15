"use strict";

var {WebRequest} = ChromeUtils.import("resource://gre/modules/WebRequest.jsm");

const server = createHttpServer({hosts: ["example.com"]});
server.registerPathHandler("/", (request, response) => {
  response.setStatusLine(request.httpVersion, 200, "OK");
  if (request.hasHeader("Cookie")) {
    let value = request.getHeader("Cookie");
    if (value == "blinky=1") {
      response.setHeader("Set-Cookie", "dinky=1", false);
    }
    response.write("cookie-present");
  } else {
    response.setHeader("Set-Cookie", "foopy=1", false);
    response.write("cookie-not-present");
  }
});

const URL = "http://example.com/";

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
      equal(value, "foopy=1", "Cookie is correct");
      headers.push({name, value: "blinky=1"});
      found = true;
    } else {
      headers.push({name, value});
    }
  }
  ok(found, "Saw cookie header");
  equal(countBefore, 1, "onBeforeSendHeaders hit once");

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
    if (name == "set-cookie") {
      equal(value, "dinky=1", "Cookie is correct");
      found = true;
    }
  }
  ok(found, "Saw cookie header");
  equal(countAfter, 1, "onResponseStarted hit once");
}

add_task(async function filter_urls() {
  // First load the URL so that we set cookie foopy=1.
  let contentPage = await ExtensionTestUtils.loadContentPage(URL);
  await contentPage.close();

  // Now load with WebRequest set up.
  WebRequest.onBeforeSendHeaders.addListener(onBeforeSendHeaders, null, ["blocking", "requestHeaders"]);
  WebRequest.onResponseStarted.addListener(onResponseStarted, null, ["responseHeaders"]);

  contentPage = await ExtensionTestUtils.loadContentPage(URL);
  await contentPage.close();

  WebRequest.onBeforeSendHeaders.removeListener(onBeforeSendHeaders);
  WebRequest.onResponseStarted.removeListener(onResponseStarted);
});
