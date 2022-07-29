"use strict";

do_get_profile();

// Let's use XPCShellContentUtils to open/close tabs.
const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);

XPCShellContentUtils.init(this);

var createHttpServer = (...args) => {
  return XPCShellContentUtils.createHttpServer(...args);
};

const server = createHttpServer({
  hosts: ["3rdparty.org", "4thparty.org", "foobar.com"],
});

async function testThings(prefValue, expected) {
  await new Promise(resolve =>
    Services.clearData.deleteData(
      Ci.nsIClearDataService.CLEAR_ALL_CACHES,
      resolve
    )
  );

  Services.prefs.setCharPref("privacy.rejectForeign.allowList", prefValue);

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
  let contentPage = await XPCShellContentUtils.loadContentPage(
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
  contentPage = await XPCShellContentUtils.loadContentPage(
    "http://foobar.com/test3rdPartyDocument"
  );
  Assert.equal(await cookiePromise, expected, "Cookies received?");
  await contentPage.close();
}

add_task(async function test_rejectForeignAllowList() {
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

  // Let's set a cookie.
  let contentPage = await XPCShellContentUtils.loadContentPage(
    "http://3rdparty.org/setCookies"
  );
  await contentPage.close();
  Assert.equal(Services.cookies.cookies.length, 1);

  // Without exceptionlisting, no cookies should be shared.
  await testThings("", "");

  // Let's exceptionlist 3rdparty.org
  await testThings("3rdparty.org", "cookie=wow");
});
