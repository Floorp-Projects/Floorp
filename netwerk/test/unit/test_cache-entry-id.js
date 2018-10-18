/**
 * Test for the "CacheEntryId" under several cases.
 */

ChromeUtils.import("resource://testing-common/httpd.js");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + "/content";
});

var httpServer = null;

const responseContent = "response body";
const responseContent2 = "response body 2";
const altContent = "!@#$%^&*()";
const altContentType = "text/binary";

function isParentProcess() {
    let appInfo = Cc["@mozilla.org/xre/app-info;1"];
    return (!appInfo || appInfo.getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT);
}

var handlers = [
  (m, r) => {r.bodyOutputStream.write(responseContent, responseContent.length)},
  (m, r) => {r.setStatusLine(m.httpVersion, 304, "Not Modified")},
  (m, r) => {r.setStatusLine(m.httpVersion, 304, "Not Modified")},
  (m, r) => {r.setStatusLine(m.httpVersion, 304, "Not Modified")},
  (m, r) => {r.setStatusLine(m.httpVersion, 304, "Not Modified")},
  (m, r) => {r.bodyOutputStream.write(responseContent2, responseContent2.length)},
  (m, r) => {r.setStatusLine(m.httpVersion, 304, "Not Modified")},
];

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "no-cache");

  var handler = handlers.shift();
  if (handler) {
    handler(metadata, response);
    return;
  }

  Assert.ok(false, "Should not reach here.");
}

function fetch(preferredDataType = null)
{
  return new Promise(resolve => {
    var chan = NetUtil.newChannel({uri: URL, loadUsingSystemPrincipal: true});

    if (preferredDataType) {
      var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
      cc.preferAlternativeDataType(altContentType);
    }

    chan.asyncOpen2(new ChannelListener((request,
                                         buffer,
                                         ctx,
                                         isFromCache,
                                         cacheEntryId) => {
      resolve({request, buffer, isFromCache, cacheEntryId});
    }, null));
  });
}

function check(response, content, preferredDataType, isFromCache, cacheEntryIdChecker)
{
  var cc = response.request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(response.buffer, content);
  Assert.equal(cc.alternativeDataType, preferredDataType);
  Assert.equal(response.isFromCache, isFromCache);
  Assert.ok(!cacheEntryIdChecker || cacheEntryIdChecker(response.cacheEntryId));

  return response;
}

function writeAltData(request)
{
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);
  var os = cc.openAlternativeOutputStream(altContentType, altContent.length);
  os.write(altContent, altContent.length);
  os.close();
  gc(); // We need to do a GC pass to ensure the cache entry has been freed.

  return new Promise(resolve => {
    if (isParentProcess()) {
      Services.cache2.QueryInterface(Ci.nsICacheTesting)
              .flush(resolve);
    } else {
      do_send_remote_message('flush');
      do_await_remote_message('flushed').then(resolve);
    }
  });
}

function run_test()
{
  do_get_profile();
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);
  do_test_pending();

  var targetCacheEntryId = null;

  return Promise.resolve()
    // Setup testing environment: Placing alternative data into HTTP cache.
    .then(_ => fetch(altContentType))
    .then(r => check(r, responseContent, "", false,
                     cacheEntryId => cacheEntryId === undefined))
    .then(r => writeAltData(r.request))

    // Start testing.
    .then(_ => fetch(altContentType))
    .then(r => check(r, altContent, altContentType, true,
                     cacheEntryId => cacheEntryId !== undefined))
    .then(r => targetCacheEntryId = r.cacheEntryId)

    .then(_ => fetch())
    .then(r => check(r, responseContent, "", true,
                     cacheEntryId => cacheEntryId === targetCacheEntryId))

    .then(_ => fetch(altContentType))
    .then(r => check(r, altContent, altContentType, true,
                     cacheEntryId => cacheEntryId === targetCacheEntryId))

    .then(_ => fetch())
    .then(r => check(r, responseContent, "", true,
                     cacheEntryId => cacheEntryId === targetCacheEntryId))

    .then(_ => fetch()) // The response is changed here.
    .then(r => check(r, responseContent2, "", false,
                     cacheEntryId => cacheEntryId === undefined))

    .then(_ => fetch())
    .then(r => check(r, responseContent2, "", true,
                     cacheEntryId => cacheEntryId !== undefined &&
                                     cacheEntryId !== targetCacheEntryId))

    // Tear down.
    .catch(e => Assert.ok(false, "Unexpected exception: " + e))
    .then(_ => Assert.equal(handlers.length, 0))
    .then(_ => httpServer.stop(do_test_finished));
}
