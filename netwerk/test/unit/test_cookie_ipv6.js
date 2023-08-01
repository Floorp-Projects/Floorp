/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *  Test that channels with different LoadInfo
 *  are stored in separate namespaces ("cookie jars")
 */

"use strict";

let ip = "[::1]";
ChromeUtils.defineLazyGetter(this, "URL", function () {
  return `http://${ip}:${httpserver.identity.primaryPort}/`;
});

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

let httpserver = new HttpServer();

function cookieSetHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "Ok");
  response.setHeader(
    "Set-Cookie",
    `Set-Cookie: T1=T2; path=/; SameSite=Lax; domain=${ip}; httponly`,
    false
  );
  response.setHeader("Content-Type", "text/html");
  response.setHeader("Content-Length", "2");
  response.bodyOutputStream.write("Ok", "Ok".length);
}

add_task(async function test_cookie_ipv6() {
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);
  Services.prefs.setBoolPref(
    "network.cookieJarSettings.unblocked_for_testing",
    true
  );

  httpserver.registerPathHandler("/", cookieSetHandler);
  httpserver._start(-1, ip);

  var chan = NetUtil.newChannel({
    uri: URL,
    loadUsingSystemPrincipal: true,
  });
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve));
  });
  equal(Services.cookies.cookies.length, 1);
});
