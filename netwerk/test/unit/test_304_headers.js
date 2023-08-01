"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return `http://localhost:${httpServer.identity.primaryPort}/test`;
});

let httpServer = null;

function make_channel(url) {
  return NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
}

function contentHandler(metadata, response) {
  response.seizePower();
  let etag = "";
  try {
    etag = metadata.getHeader("If-None-Match");
  } catch (ex) {}

  if (etag == "test-etag1") {
    response.write("HTTP/1.1 304 Not Modified\r\n");

    response.write("Link: <ref>; param1=value1\r\n");
    response.write("Link: <ref2>; param2=value2\r\n");
    response.write("Link: <ref3>; param1=value1\r\n");
    response.write("\r\n");
    response.finish();
    return;
  }

  response.write("HTTP/1.1 200 OK\r\n");

  response.write("ETag: test-etag1\r\n");
  response.write("Link: <ref>; param1=value1\r\n");
  response.write("Link: <ref2>; param2=value2\r\n");
  response.write("Link: <ref3>; param1=value1\r\n");
  response.write("\r\n");
  response.finish();
}

add_task(async function test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/test", contentHandler);
  httpServer.start(-1);
  registerCleanupFunction(async () => {
    await httpServer.stop();
  });

  let chan = make_channel(Services.io.newURI(URL));
  chan.requestMethod = "HEAD";
  await new Promise(resolve => {
    chan.asyncOpen({
      onStopRequest(req, status) {
        equal(status, Cr.NS_OK);
        equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
        equal(
          req.QueryInterface(Ci.nsIHttpChannel).getResponseHeader("Link"),
          "<ref>; param1=value1, <ref2>; param2=value2, <ref3>; param1=value1"
        );
        resolve();
      },
      onStartRequest(req) {},
      onDataAvailable() {},
    });
  });

  chan = make_channel(Services.io.newURI(URL));
  chan.requestMethod = "HEAD";
  await new Promise(resolve => {
    chan.asyncOpen({
      onStopRequest(req, status) {
        equal(status, Cr.NS_OK);
        equal(req.QueryInterface(Ci.nsIHttpChannel).responseStatus, 200);
        equal(
          req.QueryInterface(Ci.nsIHttpChannel).getResponseHeader("Link"),
          "<ref>; param1=value1, <ref2>; param2=value2, <ref3>; param1=value1"
        );
        resolve();
      },
      onStartRequest(req) {},
      onDataAvailable() {},
    });
  });
});
