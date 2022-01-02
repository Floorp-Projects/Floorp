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

add_task(async function test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);
  Services.prefs.setCharPref(
    "network.http.accept-encoding",
    "gzip, deflate, br"
  );

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
  equal(buff, "hello");
  Services.prefs.clearUserPref("network.http.accept-encoding");
  await httpServer.stop();
});
