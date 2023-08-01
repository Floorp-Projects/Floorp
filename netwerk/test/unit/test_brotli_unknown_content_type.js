/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This test file makes sure that we can decode brotli files when the
// Content-Type header is missing (Bug 1715401)

"use strict";

function emptyBrotli(metadata, response) {
  response.setHeader("Content-Encoding", "br", false);
  response.write("\x01\x03\x06\x03");
}

function largeEmptyBrotli(metadata, response) {
  response.setHeader("Content-Encoding", "br", false);
  response.write("\x01\x03" + "\x06".repeat(600) + "\x03");
}

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL_EMPTY_BROTLI", function () {
  return (
    "http://localhost:" + httpServer.identity.primaryPort + "/empty-brotli"
  );
});

ChromeUtils.defineLazyGetter(this, "URL_LARGE_EMPTY_BROTLI", function () {
  return (
    "http://localhost:" +
    httpServer.identity.primaryPort +
    "/large-empty-brotli"
  );
});

var httpServer = null;

add_task(async function check_brotli() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/empty-brotli", emptyBrotli);
  httpServer.registerPathHandler("/large-empty-brotli", largeEmptyBrotli);
  httpServer.start(-1);

  async function test(url) {
    let chan = NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
    let [, response] = await new Promise(resolve => {
      chan.asyncOpen(
        new ChannelListener(
          (req, buff) => {
            resolve([req, buff]);
          },
          null,
          CL_IGNORE_CL
        )
      );
    });
    return response;
  }
  equal(
    await test(URL_EMPTY_BROTLI),
    "",
    "Should decode brotli even when brotli output is empty"
  );
  equal(
    await test(URL_LARGE_EMPTY_BROTLI),
    "",
    "Should decode brotli even when the nsUnknownDecoder can't get any decoded output"
  );
  Services.prefs.clearUserPref("network.http.accept-encoding");
  Services.prefs.clearUserPref("network.http.encoding.trustworthy_is_https");
  await httpServer.stop();
});
