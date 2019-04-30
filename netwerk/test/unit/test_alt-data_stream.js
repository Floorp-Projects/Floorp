/**
 * Test for the "alternative data stream" stored withing a cache entry.
 *
 * - we load a URL with preference for an alt data (check what we get is the raw data,
 *   since there was nothing previously cached)
 * - we write a big chunk of alt-data to the output stream
 * - we load the URL again, expecting to get alt-data
 * - we check that the alt-data is streamed. We should get the first chunk, then
 *   the rest of the alt-data is written, and we check that it is received in
 *   the proper order.
 *
 */

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpServer.identity.primaryPort + "/content";
});

var httpServer = null;

function make_channel(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

const responseContent = "response body";
// We need a large content in order to make sure that the IPDL stream is cut
// into several different chunks.
// We fill each chunk with a different character for easy debugging.
const altContent = "a".repeat(128*1024) +
                   "b".repeat(128*1024) +
                   "c".repeat(128*1024) +
                   "d".repeat(128*1024) +
                   "e".repeat(128*1024) +
                   "f".repeat(128*1024) +
                   "g".repeat(128*1024) +
                   "h".repeat(128*1024) +
                   "i".repeat(13); // Just so the chunk size doesn't match exactly.

const firstChunkSize = Math.floor(altContent.length / 4);
const altContentType = "text/binary";

function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.setHeader("Cache-Control", "max-age=86400");

  response.bodyOutputStream.write(responseContent, responseContent.length);
}

function run_test()
{
  do_get_profile();
  httpServer = new HttpServer();
  httpServer.registerPathHandler("/content", contentHandler);
  httpServer.start(-1);

  var chan = make_channel(URL);

  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(altContentType, "", true);

  chan.asyncOpen(new ChannelListener(readServerContent, null));
  do_test_pending();
}

// Output stream used to write alt-data to the cache entry.
var os;

function readServerContent(request, buffer)
{
  var cc = request.QueryInterface(Ci.nsICacheInfoChannel);

  Assert.equal(buffer, responseContent);
  Assert.equal(cc.alternativeDataType, "");

  executeSoon(() => {
    os = cc.openAlternativeOutputStream(altContentType, altContent.length);
    // Write a quarter of the alt data content
    os.write(altContent, firstChunkSize);

    executeSoon(openAltChannel);
  });
}

function openAltChannel()
{
  var chan = make_channel(URL);
  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(altContentType, "", true);

  chan.asyncOpen(altDataListener);
}

var altDataListener = {
  buffer: "",
  onStartRequest: function(request) { },
  onDataAvailable: function(request, stream, offset, count) {
    let string = NetUtil.readInputStreamToString(stream, count);
    this.buffer += string;

    // XXX: this condition might be a bit volatile. If this test times out,
    // it probably means that for some reason, the listener didn't get all the
    // data in the first chunk.
    if (this.buffer.length == firstChunkSize) {
      // write the rest of the content
      os.write(altContent.substring(firstChunkSize, altContent.length), altContent.length - firstChunkSize);
      os.close();
    }
  },
  onStopRequest: function(request, status) {
    var cc = request.QueryInterface(Ci.nsICacheInfoChannel);
    Assert.equal(cc.alternativeDataType, altContentType);
    Assert.equal(this.buffer.length, altContent.length);
    Assert.equal(this.buffer, altContent);
    openAltChannelWithOriginalContent();
  },
};

function openAltChannelWithOriginalContent()
{
  var chan = make_channel(URL);
  var cc = chan.QueryInterface(Ci.nsICacheInfoChannel);
  cc.preferAlternativeDataType(altContentType, "", false);

  chan.asyncOpen(originalListener);
}

var originalListener = {
  buffer: "",
  onStartRequest: function(request) { },
  onDataAvailable: function(request, stream, offset, count) {
    let string = NetUtil.readInputStreamToString(stream, count);
    this.buffer += string;
  },
  onStopRequest: function(request, status) {
    var cc = request.QueryInterface(Ci.nsICacheInfoChannel);
    Assert.equal(cc.alternativeDataType, altContentType);
    Assert.equal(this.buffer.length, responseContent.length);
    Assert.equal(this.buffer, responseContent);
    testAltDataStream(cc);
  },
};

function testAltDataStream(cc)
{
  cc.getAltDataInputStream(altContentType, {
    onInputStreamReady: function(aInputStream) {
      Assert.ok(!!aInputStream);
      httpServer.stop(do_test_finished);
    }
  });
}
