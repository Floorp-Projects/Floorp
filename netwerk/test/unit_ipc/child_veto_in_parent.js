/* import-globals-from ../unit/head_trr.js */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpServer.identity.primaryPort;
});

var httpServer = null;
// Need to randomize, because apparently no one clears our cache
var randomPath = "/redirect/" + Math.random();

ChromeUtils.defineLazyGetter(this, "randomURI", function () {
  return URL + randomPath;
});

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

const responseBody = "response body";

function redirectHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 301, "Moved");
  response.setHeader("Location", URL + "/content", false);
}

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(responseBody, responseBody.length);
}

add_task(async function doStuff() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler(randomPath, redirectHandler);
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  let chan = make_channel(randomURI);
  let [req, buff] = await new Promise(resolve =>
    chan.asyncOpen(
      new ChannelListener((aReq, aBuff) => resolve([aReq, aBuff]), null)
    )
  );
  Assert.equal(buff, "");
  Assert.equal(req.status, Cr.NS_OK);
  await httpServer.stop();
  await do_send_remote_message("child-test-done");
});
