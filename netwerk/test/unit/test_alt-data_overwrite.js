/**
 * Test for overwriting the alternative data in a cache entry.
 *
 * - run_test loads a new channel
 * - readServerContent checks the content, and saves alt-data
 * - cacheFlushObserver creates a new channel with "text/binary" alt-data type
 * - readAltContent checks that it gets back alt-data and creates a channel with the dummy/null alt-data type
 * - readServerContent2 checks that it gets regular content, from the cache and tries to overwrite the alt-data with the same representation
 * - cacheFlushObserver2 creates a new channel with "text/binary" alt-data type
 * - readAltContent2 checks that it gets back alt-data, and tries to overwrite with a kind of alt-data
 * - cacheFlushObserver3 creates a new channel with "text/binary2" alt-data type
 * - readAltContent3 checks that it gets back the newly saved alt-data
 */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

ChromeUtils.defineLazyGetter(this, "URL", function () {
  return "http://localhost:" + httpServer.identity.primaryPort + "/content";
});

let httpServer = null;

function make_and_open_channel(url, altContentType, callback) {
  let chan = NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
  if (altContentType) {
    let cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
    cc.preferAlternativeDataType(
      altContentType,
      "",
      Ci.nsICacheInfoChannel.ASYNC
    );
  }
  chan.asyncOpen(new ChannelListener(callback, null));
}

const responseContent = "response body";
const altContent = "!@#$%^&*()";
const altContentType = "text/binary";
const altContent2 = "abc";
const altContentType2 = "text/binary2";

let servedNotModified = false;

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "no-cache");
  response.setHeader("ETag", "test-etag1");
  let etag = "";
  try {
    etag = metadata.getHeader("If-None-Match");
  } catch (ex) {
    etag = "";
  }

  if (etag == "test-etag1") {
    response.setStatusLine(metadata.httpVersion, 304, "Not Modified");
    servedNotModified = true;
  } else {
    servedNotModified = false;
    response.bodyOutputStream.write(responseContent, responseContent.length);
  }
}

function run_test() {
  do_get_profile();
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  do_test_pending();
  make_and_open_channel(URL, altContentType, readServerContent);
}

function readServerContent(request, buffer) {
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(buffer, responseContent);
  Assert.equal(cc.alternativeDataType, "");

  executeSoon(() => {
    let os = cc.openAlternativeOutputStream(altContentType, altContent.length);
    os.write(altContent, altContent.length);
    os.close();

    executeSoon(flushAndOpenAltChannel);
  });
}

function flushAndOpenAltChannel() {
  // We need to do a GC pass to ensure the cache entry has been freed.
  Cu.forceShrinkingGC();
  Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver);
}

// needs to be rooted
let cacheFlushObserver = {
  observe() {
    if (!cacheFlushObserver) {
      info("ignoring cacheFlushObserver\n");
      return;
    }
    cacheFlushObserver = null;
    Cu.forceShrinkingGC();
    make_and_open_channel(URL, altContentType, readAltContent);
  },
};

function readAltContent(request, buffer, closure, fromCache) {
  Cu.forceShrinkingGC();
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(fromCache || servedNotModified, true);
  Assert.equal(cc.alternativeDataType, altContentType);
  Assert.equal(buffer, altContent);

  make_and_open_channel(URL, "dummy/null", readServerContent2);
}

function readServerContent2(request, buffer, closure, fromCache) {
  Cu.forceShrinkingGC();
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(fromCache || servedNotModified, true);
  Assert.equal(buffer, responseContent);
  Assert.equal(cc.alternativeDataType, "");

  executeSoon(() => {
    let os = cc.openAlternativeOutputStream(altContentType, altContent.length);
    os.write(altContent, altContent.length);
    os.close();

    executeSoon(flushAndOpenAltChannel2);
  });
}

function flushAndOpenAltChannel2() {
  // We need to do a GC pass to ensure the cache entry has been freed.
  Cu.forceShrinkingGC();
  Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver2);
}

// needs to be rooted
let cacheFlushObserver2 = {
  observe() {
    if (!cacheFlushObserver2) {
      info("ignoring cacheFlushObserver2\n");
      return;
    }
    cacheFlushObserver2 = null;
    Cu.forceShrinkingGC();
    make_and_open_channel(URL, altContentType, readAltContent2);
  },
};

function readAltContent2(request, buffer, closure, fromCache) {
  Cu.forceShrinkingGC();
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(servedNotModified || fromCache, true);
  Assert.equal(cc.alternativeDataType, altContentType);
  Assert.equal(buffer, altContent);

  executeSoon(() => {
    Cu.forceShrinkingGC();
    info("writing other content\n");
    let os = cc.openAlternativeOutputStream(
      altContentType2,
      altContent2.length
    );
    os.write(altContent2, altContent2.length);
    os.close();

    executeSoon(flushAndOpenAltChannel3);
  });
}

function flushAndOpenAltChannel3() {
  // We need to do a GC pass to ensure the cache entry has been freed.
  Cu.forceShrinkingGC();
  Services.cache2.QueryInterface(Ci.nsICacheTesting).flush(cacheFlushObserver3);
}

// needs to be rooted
let cacheFlushObserver3 = {
  observe() {
    if (!cacheFlushObserver3) {
      info("ignoring cacheFlushObserver3\n");
      return;
    }

    cacheFlushObserver3 = null;
    Cu.forceShrinkingGC();
    make_and_open_channel(URL, altContentType2, readAltContent3);
  },
};

function readAltContent3(request, buffer, closure, fromCache) {
  let cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(servedNotModified || fromCache, true);
  Assert.equal(cc.alternativeDataType, altContentType2);
  Assert.equal(buffer, altContent2);

  httpServer.stop(do_test_finished);
}
