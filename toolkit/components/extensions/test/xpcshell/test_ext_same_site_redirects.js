"use strict";

/**
 * This test tests various redirection scenarios, and checks whether sameSite
 * cookies are sent.
 *
 * The file has the following tests:
 * - verify_firstparty_web_behavior - base case, confirms normal web behavior.
 * - samesite_is_foreign_without_host_permissions
 * - wildcard_host_permissions_enable_samesite_cookies
 * - explicit_host_permissions_enable_samesite_cookies
 * - some_host_permissions_enable_some_samesite_cookies
 */

// This simulates a common pattern used for sites that require authentication.
// After logging in, there may be multiple redirects, HTTP and scripted.
const SITE_START = "start.example.net";
// set "start" cookies + 302 redirects to found.
const SITE_FOUND = "found.example.net";
// set "found" cookies + uses a HTML redirect to redir.
const SITE_REDIR = "redir.example.net";
// set "redir" cookies + 302 redirects to final.
const SITE_FINAL = "final.example.net";

const SITE = "example.net";

const URL_START = `http://${SITE_START}/start`;

const server = createHttpServer({
  hosts: [SITE_START, SITE_FOUND, SITE_REDIR, SITE_FINAL],
});

function getCookies(request) {
  return request.hasHeader("Cookie") ? request.getHeader("Cookie") : "";
}

function sendCookies(response, prefix, suffix = "") {
  const cookies = [
    prefix + "-none=1; sameSite=none; domain=" + SITE + suffix,
    prefix + "-lax=1; sameSite=lax; domain=" + SITE + suffix,
    prefix + "-strict=1; sameSite=strict; domain=" + SITE + suffix,
  ];
  for (let cookie of cookies) {
    response.setHeader("Set-Cookie", cookie, true);
  }
}

function deleteCookies(response, prefix) {
  sendCookies(response, prefix, "; expires=Thu, 01 Jan 1970 00:00:00 GMT");
}

var receivedCookies = [];

server.registerPathHandler("/start", (request, response) => {
  Assert.equal(request.host, SITE_START);
  Assert.equal(getCookies(request), "", "No cookies at start of test");

  response.setStatusLine(request.httpVersion, 302, "Found");
  sendCookies(response, "start");
  response.setHeader("Location", `http://${SITE_FOUND}/found`);
});

server.registerPathHandler("/found", (request, response) => {
  Assert.equal(request.host, SITE_FOUND);
  receivedCookies.push(getCookies(request));

  response.setHeader("Content-Type", "text/html; charset=utf-8", false);
  deleteCookies(response, "start");
  sendCookies(response, "found");
  response.write(`<script>location = "http://${SITE_REDIR}/redir";</script>`);
});

server.registerPathHandler("/redir", (request, response) => {
  Assert.equal(request.host, SITE_REDIR);
  receivedCookies.push(getCookies(request));

  response.setStatusLine(request.httpVersion, 302, "Found");
  deleteCookies(response, "found");
  sendCookies(response, "redir");
  response.setHeader("Location", `http://${SITE_FINAL}/final`);
});

server.registerPathHandler("/final", (request, response) => {
  Assert.equal(request.host, SITE_FINAL);
  receivedCookies.push(getCookies(request));

  response.setStatusLine(request.httpVersion, 302, "Found");
  deleteCookies(response, "redir");
  // In test some_host_permissions_enable_some_samesite_cookies, the cookies
  // from the start haven't been cleared due to the lack of host permissions.
  // Do that here instead.
  deleteCookies(response, "start");
  response.setHeader("Location", "/final_and_clean");
});

// Should be called before any request is made.
function promiseFinalResponse() {
  Assert.deepEqual(receivedCookies, [], "Test starts without observed cookies");
  return new Promise(resolve => {
    server.registerPathHandler("/final_and_clean", (request, response) => {
      Assert.equal(request.host, SITE_FINAL);
      Assert.equal(getCookies(request), "", "Cookies cleaned up");
      resolve(receivedCookies.splice(0));
    });
  });
}

// Load the page as a child frame of an extension, for the given permissions.
async function getCookiesForLoadInExtension({ permissions }) {
  // embedder.html loads http:-frame.
  allow_unsafe_parent_loads_when_extensions_not_remote();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions,
    },
    files: {
      "embedder.html": `<iframe src="${URL_START}"></iframe>`,
    },
  });
  await extension.startup();
  let cookiesPromise = promiseFinalResponse();
  let contentPage = await ExtensionTestUtils.loadContentPage(
    `moz-extension://${extension.uuid}/embedder.html`,
    { extension }
  );
  let cookies = await cookiesPromise;
  await contentPage.close();
  await extension.unload();

  revert_allow_unsafe_parent_loads_when_extensions_not_remote();

  return cookies;
}

add_task(async function setup() {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", true);

  // Test server runs on http, so disable Secure requirement of sameSite=none.
  Services.prefs.setBoolPref(
    "network.cookie.sameSite.noneRequiresSecure",
    false
  );
});

// First verify that our expectations match with the actual behavior on the web.
add_task(async function verify_firstparty_web_behavior() {
  let cookiesPromise = promiseFinalResponse();
  let contentPage = await ExtensionTestUtils.loadContentPage(URL_START);
  let cookies = await cookiesPromise;
  await contentPage.close();
  Assert.deepEqual(
    cookies,
    // Same expectations as in host_permissions_enable_samesite_cookies
    [
      "start-none=1; start-lax=1; start-strict=1",
      "found-none=1; found-lax=1; found-strict=1",
      "redir-none=1; redir-lax=1; redir-strict=1",
    ],
    "Expected cookies from a first-party load on the web"
  );
});

// Verify that an extension without permission behaves like a third-party page.
add_task(async function samesite_is_foreign_without_host_permissions() {
  let cookies = await getCookiesForLoadInExtension({
    permissions: [],
  });

  Assert.deepEqual(
    cookies,
    ["start-none=1", "found-none=1", "redir-none=1"],
    "SameSite cookies excluded without permissions"
  );
});

// When an extension has permissions for the site, cookies should be included.
add_task(async function wildcard_host_permissions_enable_samesite_cookies() {
  let cookies = await getCookiesForLoadInExtension({
    permissions: ["*://*.example.net/*"], // = *.SITE
  });

  Assert.deepEqual(
    cookies,
    // Same expectations as in verify_firstparty_web_behavior.
    [
      "start-none=1; start-lax=1; start-strict=1",
      "found-none=1; found-lax=1; found-strict=1",
      "redir-none=1; redir-lax=1; redir-strict=1",
    ],
    "Expected cookies from a load in an extension frame"
  );
});

// When an extension has permissions for the site, cookies should be included.
add_task(async function explicit_host_permissions_enable_samesite_cookies() {
  let cookies = await getCookiesForLoadInExtension({
    permissions: [
      "*://start.example.net/*",
      "*://found.example.net/*",
      "*://redir.example.net/*",
      "*://final.example.net/*",
    ],
  });

  Assert.deepEqual(
    cookies,
    // Same expectations as in verify_firstparty_web_behavior.
    [
      "start-none=1; start-lax=1; start-strict=1",
      "found-none=1; found-lax=1; found-strict=1",
      "redir-none=1; redir-lax=1; redir-strict=1",
    ],
    "Expected cookies from a load in an extension frame"
  );
});

// When an extension does not have host permissions for all sites, but only
// some, then same-site cookies are only included in requests with the right
// permissions.
add_task(async function some_host_permissions_enable_some_samesite_cookies() {
  let cookies = await getCookiesForLoadInExtension({
    permissions: ["*://start.example.net/*", "*://final.example.net/*"],
  });

  Assert.deepEqual(
    cookies,
    [
      // Missing permission for "found.example.net":
      "start-none=1",
      // Missing permission for "redir.example.net":
      "found-none=1",
      // "final.example.net" can see cookies from "start.example.net":
      "start-lax=1; start-strict=1; redir-none=1",
    ],
    "Expected some cookies from a load in an extension frame"
  );
});
