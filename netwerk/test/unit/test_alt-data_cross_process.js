/**
 * Test for the "alternative data stream" stored withing a cache entry.
 *
 * - we load a URL with preference for an alt data (check what we get is the raw data,
 *   since there was nothing previously cached)
 * - we store the alt data along the channel (to the cache entry)
 * - we flush the HTTP cache
 * - we reload the same URL using a new channel, again prefering the alt data be loaded
 * - this time the alt data must arive
 */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpServer.identity.primaryPort + "/content";
});

var httpServer = null;

function make_channel(url, callback, ctx) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

function inChildProcess() {
  return Services.appinfo.processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;
}

const responseContent = "response body";
const responseContent2 = "response body 2";
const altContent = "!@#$%^&*()";
const altContentType = "text/binary";

var servedNotModified = false;
var shouldPassRevalidation = true;

var cache_storage = null;

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "no-cache");
  response.setHeader("ETag", "test-etag1");

  let etag;
  try {
    etag = metadata.getHeader("If-None-Match");
  } catch (ex) {
    etag = "";
  }

  if (etag == "test-etag1" && shouldPassRevalidation) {
    response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
    servedNotModified = true;
  } else {
    var content = shouldPassRevalidation ? responseContent : responseContent2;
    response.bodyOutputStream.write(content, content.length);
  }
}

function check_has_alt_data_in_index(aHasAltData, callback) {
  if (inChildProcess()) {
    callback();
    return;
  }

  syncWithCacheIOThread(() => {
    var hasAltData = {};
    cache_storage.getCacheIndexEntryAttrs(createURI(URL), "", hasAltData, {});
    Assert.equal(hasAltData.value, aHasAltData);
    callback();
  }, true);
}

// This file is loaded as part of test_alt-data_cross_process_wrap.js.
// eslint-disable-next-line no-unused-vars
function run_test() {
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);
  do_test_pending();

  asyncOpen();
}

function asyncOpen() {
  var chan = make_channel(URL);

  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(
    altContentType,
    "",
    Ci.nsICacheInfoChannel.ASYNC
  );

  chan.asyncOpen(new ChannelListener(readServerContent, null));
}

function readServerContent(request, buffer) {
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(buffer, responseContent);
  Assert.equal(cc.alternativeDataType, "");
  check_has_alt_data_in_index(false, () => {
    executeSoon(() => {
      var os = cc.openAlternativeOutputStream(
        altContentType,
        altContent.length
      );
      os.write(altContent, altContent.length);
      os.close();

      executeSoon(flushAndOpenAltChannel);
    });
  });
}

function flushAndOpenAltChannel() {
  // We need to do a GC pass to ensure the cache entry has been freed.
  gc();
  do_send_remote_message("flush");
  do_await_remote_message("flushed").then(() => {
    openAltChannel();
  });
}

function openAltChannel() {
  var chan = make_channel(URL);
  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(
    altContentType,
    "",
    Ci.nsICacheInfoChannel.ASYNC
  );

  chan.asyncOpen(new ChannelListener(readAltContent, null));
}

function readAltContent(request, buffer) {
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(servedNotModified, true);
  Assert.equal(cc.alternativeDataType, altContentType);
  Assert.equal(buffer, altContent);

  // FINISH
  do_send_remote_message("done");
  do_await_remote_message("finish").then(() => {
    httpServer.stop(do_test_finished);
  });
}
