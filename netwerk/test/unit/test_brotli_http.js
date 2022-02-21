// This test exists mostly as documentation that
// Firefox can load brotli files over HTTP if we set the proper pref.

"use strict";

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Encoding", "br", false);
  response.write("\x0b\x02\x80hello\x03");
}

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + "/content";
});

var httpServer = null;

add_task(async function check_brotli() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  async function test() {
    let chan = NetUtil.newChannel({ uri: URL, loadUsingSystemPrincipal: true });
    let [, buff] = await new Promise(resolve => {
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
    return buff;
  }

  Services.prefs.setBoolPref(
    "network.http.encoding.trustworthy_is_https",
    true
  );
  equal(
    await test(),
    "hello",
    "Should decode brotli when trustworthy_is_https=true"
  );
  Services.prefs.setBoolPref(
    "network.http.encoding.trustworthy_is_https",
    false
  );
  equal(
    await test(),
    "\x0b\x02\x80hello\x03",
    "Should not decode brotli when trustworthy_is_https=false"
  );
  Services.prefs.setCharPref(
    "network.http.accept-encoding",
    "gzip, deflate, br"
  );
  equal(
    await test(),
    "hello",
    "Should decode brotli if we set the HTTP accept encoding to include brotli"
  );
  Services.prefs.clearUserPref("network.http.accept-encoding");
  Services.prefs.clearUserPref("network.http.encoding.trustworthy_is_https");
  await httpServer.stop();
});
