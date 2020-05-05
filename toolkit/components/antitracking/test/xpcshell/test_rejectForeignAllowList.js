"use strict";

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

do_get_profile();

// Let's use AddonTestUtils and ExtensionTestUtils to open/close tabs.
var { AddonTestUtils, MockAsyncShutdown } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);

// eslint-disable-next-line no-unused-vars
XPCOMUtils.defineLazyModuleGetters(this, {
  ExtensionTestUtils: "resource://testing-common/ExtensionXPCShellUtils.jsm",
});

ExtensionTestUtils.init(this);

var createHttpServer = (...args) => {
  AddonTestUtils.maybeInit(this);
  return AddonTestUtils.createHttpServer(...args);
};

const server = createHttpServer({
  hosts: ["3rdparty.org", "4thparty.org", "foobar.com"],
});

async function testThings(expected) {
  let cookiePromise = new Promise(resolve => {
    server.registerPathHandler("/test3rdPartyChannel", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.write(`<html><img src="http://3rdparty.org/img" /></html>`);
    });

    server.registerPathHandler("/img", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      resolve(request.hasHeader("Cookie") ? request.getHeader("Cookie") : "");
      response.setHeader("Content-Type", "image/png", false);
      response.write("Not an image");
    });
  });

  // Let's load 3rdparty.org as a 3rd-party.
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://foobar.com/test3rdPartyChannel"
  );
  Assert.equal(await cookiePromise, expected, "Cookies received?");
  await contentPage.close();

  cookiePromise = new Promise(resolve => {
    server.registerPathHandler("/test3rdPartyDocument", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.write(
        `<html><iframe src="http://3rdparty.org/iframe" /></html>`
      );
    });

    server.registerPathHandler("/iframe", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      resolve(request.hasHeader("Cookie") ? request.getHeader("Cookie") : "");
      response.setHeader("Content-Type", "text/html; charset=utf-8", false);
      response.write(`<html><img src="http://4thparty.org/img" /></html>`);
    });

    server.registerPathHandler("/img", (request, response) => {
      response.setStatusLine(request.httpVersion, 200, "OK");
      resolve(request.hasHeader("Cookie") ? request.getHeader("Cookie") : "");
      response.setHeader("Content-Type", "image/png", false);
      response.write("Not an image");
    });
  });

  // Let's load 3rdparty.org loading a 4th-party.
  contentPage = await ExtensionTestUtils.loadContentPage(
    "http://foobar.com/test3rdPartyDocument"
  );
  Assert.equal(await cookiePromise, expected, "Cookies received?");
  await contentPage.close();
}

async function initializeSkipListService() {
  let records = [
    {
      id: "1",
      last_modified: 100000000000000000001,
      feature: "tracking-annotation-test",
      pattern: "example.com",
    },
  ];

  // Add some initial data.
  let db = await RemoteSettings("url-classifier-skip-urls").db;
  await db.create(records[0]);
  await db.saveLastModified(42);
}

add_task(async function test_rejectForeignAllowList() {
  await initializeSkipListService();

  Services.prefs.setIntPref("network.cookie.cookieBehavior", 1);
  Services.prefs.setBoolPref(
    "network.cookie.rejectForeignWithExceptions.enabled",
    true
  );

  // We don't want to have 'secure' cookies because our test http server doesn't run in https.
  Services.prefs.setBoolPref(
    "network.cookie.sameSite.noneRequiresSecure",
    false
  );

  server.registerPathHandler("/setCookies", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html; charset=utf-8", false);
    response.setHeader("Set-Cookie", "cookie=wow; sameSite=none", true);
    response.write("<html></html>");
  });

  // Empty whitelist.
  Services.prefs.setCharPref("privacy.rejectForeign.allowList", "");

  // Let's set a cookie.
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://3rdparty.org/setCookies"
  );
  await contentPage.close();
  Assert.equal(Services.cookies.cookies.length, 1);

  // Without whitelisting, no cookies should be shared.
  await testThings("");

  // Let's whitelist 3rdparty.org
  Services.prefs.setCharPref("privacy.rejectForeign.allowList", "3rdparty.org");

  await testThings("cookie=wow");
});
