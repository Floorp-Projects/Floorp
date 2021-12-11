/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests some basic jar channel functionality
 */

const { Constructor: ctor } = Components;

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const ios = Services.io;
const dirSvc = Services.dirsvc;
const obs = Services.obs;

const nsIBinaryInputStream = ctor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

const fileBase = "test_bug637286.zip";
const file = do_get_file("data/" + fileBase);
const jarBase = "jar:" + ios.newFileURI(file).spec + "!";
const tmpDir = dirSvc.get("TmpD", Ci.nsIFile);

function Listener(callback) {
  this._callback = callback;
}
Listener.prototype = {
  gotStartRequest: false,
  available: -1,
  gotStopRequest: false,
  QueryInterface: ChromeUtils.generateQI(["nsIRequestObserver"]),
  onDataAvailable(request, stream, offset, count) {
    try {
      this.available = stream.available();
      Assert.equal(this.available, count);
      // Need to consume stream to avoid assertion
      new nsIBinaryInputStream(stream).readBytes(count);
    } catch (ex) {
      do_throw(ex);
    }
  },
  onStartRequest(request) {
    this.gotStartRequest = true;
  },
  onStopRequest(request, status) {
    this.gotStopRequest = true;
    Assert.equal(status, 0);
    if (this._callback) {
      this._callback.call(null, this);
    }
  },
};

/**
 * Basic reading test for asynchronously opened jar channel
 */
function testAsync() {
  var uri = jarBase + "/inner40.zip";
  var chan = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });
  Assert.ok(chan.contentLength < 0);
  chan.asyncOpen(
    new Listener(function(l) {
      Assert.ok(chan.contentLength > 0);
      Assert.ok(l.gotStartRequest);
      Assert.ok(l.gotStopRequest);
      Assert.equal(l.available, chan.contentLength);

      run_next_test();
    })
  );
}

add_test(testAsync);
// Run same test again so we test the codepath for a zipcache hit
add_test(testAsync);

/**
 * Basic test for nsIZipReader.
 */
function testZipEntry() {
  var uri = jarBase + "/inner40.zip";
  var chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIJARChannel);
  var entry = chan.zipEntry;
  Assert.ok(entry.CRC32 == 0x8b635486);
  Assert.ok(entry.realSize == 184);
  run_next_test();
}

add_test(testZipEntry);

/**
 * Basic reading test for synchronously opened jar channels
 */
add_test(function testSync() {
  var uri = jarBase + "/inner40.zip";
  var chan = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });
  var stream = chan.open();
  Assert.ok(chan.contentLength > 0);
  Assert.equal(stream.available(), chan.contentLength);
  stream.close();
  stream.close(); // should still not throw

  run_next_test();
});

/**
 * Basic reading test for synchronously opened, nested jar channels
 */
add_test(function testSyncNested() {
  var uri = "jar:" + jarBase + "/inner40.zip!/foo";
  var chan = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });
  var stream = chan.open();
  Assert.ok(chan.contentLength > 0);
  Assert.equal(stream.available(), chan.contentLength);
  stream.close();
  stream.close(); // should still not throw

  run_next_test();
});

/**
 * Basic reading test for asynchronously opened, nested jar channels
 */
add_test(function testAsyncNested(next) {
  var uri = "jar:" + jarBase + "/inner40.zip!/foo";
  var chan = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });
  chan.asyncOpen(
    new Listener(function(l) {
      Assert.ok(chan.contentLength > 0);
      Assert.ok(l.gotStartRequest);
      Assert.ok(l.gotStopRequest);
      Assert.equal(l.available, chan.contentLength);

      run_next_test();
    })
  );
});

/**
 * Verify that file locks are released when closing a synchronously
 * opened jar channel stream
 */
add_test(function testSyncCloseUnlocks() {
  var copy = tmpDir.clone();
  copy.append(fileBase);
  file.copyTo(copy.parent, copy.leafName);
  var uri = "jar:" + ios.newFileURI(copy).spec + "!/inner40.zip";
  var chan = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });
  var stream = chan.open();
  Assert.ok(chan.contentLength > 0);
  stream.close();

  // Drop any jar caches
  obs.notifyObservers(null, "chrome-flush-caches");

  try {
    copy.remove(false);
  } catch (ex) {
    do_throw(ex);
  }

  run_next_test();
});

/**
 * Verify that file locks are released when closing an asynchronously
 * opened jar channel stream
 */
add_test(function testAsyncCloseUnlocks() {
  var copy = tmpDir.clone();
  copy.append(fileBase);
  file.copyTo(copy.parent, copy.leafName);

  var uri = "jar:" + ios.newFileURI(copy).spec + "!/inner40.zip";
  var chan = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });

  chan.asyncOpen(
    new Listener(function(l) {
      Assert.ok(chan.contentLength > 0);

      // Drop any jar caches
      obs.notifyObservers(null, "chrome-flush-caches");

      try {
        copy.remove(false);
      } catch (ex) {
        do_throw(ex);
      }

      run_next_test();
    })
  );
});

function run_test() {
  return run_next_test();
}
