// This test exists mostly as documentation that
// Firefox can load brotli files over HTTP if we set the proper pref.

"use strict";

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Encoding", "br", false);
  response.write("\x0b\x02\x80hello\x03");
}

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL", function () {
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
          (req, buff1) => {
            resolve([req, buff1]);
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

// Make sure we still decode brotli on HTTPS
// Node server doesn't work on Android yet.
add_task(
  { skip_if: () => AppConstants.platform == "android" },
  async function check_https() {
    Services.prefs.setBoolPref(
      "network.http.encoding.trustworthy_is_https",
      true
    );
    let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
      Ci.nsIX509CertDB
    );
    addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

    let server = new NodeHTTPSServer();
    await server.start();
    registerCleanupFunction(async () => {
      await server.stop();
    });
    await server.registerPathHandler("/brotli", (req, resp) => {
      resp.setHeader("Content-Type", "text/plain");
      resp.setHeader("Content-Encoding", "br");
      let output = "\x0b\x02\x80hello\x03";
      resp.writeHead(200);
      resp.end(output, "binary");
    });
    equal(
      Services.prefs.getCharPref("network.http.accept-encoding.secure"),
      "gzip, deflate, br"
    );
    let { req, buff } = await new Promise(resolve => {
      let chan = NetUtil.newChannel({
        uri: `${server.origin()}/brotli`,
        loadUsingSystemPrincipal: true,
      });
      chan.asyncOpen(
        new ChannelListener(
          (req1, buff1) => resolve({ req: req1, buff: buff1 }),
          null,
          CL_ALLOW_UNKNOWN_CL
        )
      );
    });
    equal(req.status, Cr.NS_OK);
    equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
    equal(buff, "hello");
  }
);
