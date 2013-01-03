/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests some basic jar channel functionality
 */


const {classes: Cc,
       interfaces: Ci,
       results: Cr,
       Constructor: ctor
       } = Components;

const ios = Cc["@mozilla.org/network/io-service;1"].
                getService(Ci.nsIIOService);
const dirSvc = Cc["@mozilla.org/file/directory_service;1"].
                getService(Ci.nsIProperties);
const obs = Cc["@mozilla.org/observer-service;1"].
                getService(Ci.nsIObserverService);

const nsIBinaryInputStream = ctor("@mozilla.org/binaryinputstream;1",
                               "nsIBinaryInputStream",
                               "setInputStream"
                               );

const fileBase = "test_bug637286.zip";
const file = do_get_file("data/" + fileBase);
// on child we'll test with jar:remoteopenfile:// instead of jar:file://
const jarBase = "jar:" + filePrefix + ios.newFileURI(file).spec + "!";
const tmpDir = dirSvc.get("TmpD", Ci.nsIFile);

function Listener(callback) {
    this._callback = callback;
}
Listener.prototype = {
    gotStartRequest: false,
    available: -1,
    gotStopRequest: false,
    QueryInterface: function(iid) {
        if (iid.equals(Ci.nsISupports) ||
            iid.equals(Ci.nsIRequestObserver))
            return this;
        throw Cr.NS_ERROR_NO_INTERFACE;
    },
    onDataAvailable: function(request, ctx, stream, offset, count) {
        try {
            this.available = stream.available();
            do_check_eq(this.available, count);
            // Need to consume stream to avoid assertion
            new nsIBinaryInputStream(stream).readBytes(count);
        }
        catch (ex) {
            do_throw(ex);
        }
    },
    onStartRequest: function(request, ctx) {
        this.gotStartRequest = true;
    },
    onStopRequest: function(request, ctx) {
        this.gotStopRequest = true;
        if (this._callback) {
            this._callback.call(null, this);
        }
    }
};

/**
 * Basic reading test for asynchronously opened jar channel
 */
function testAsync() {
    var uri = jarBase + "/inner40.zip";
    var chan = ios.newChannel(uri, null, null);
    do_check_true(chan.contentLength < 0);
    chan.asyncOpen(new Listener(function(l) {
        do_check_true(chan.contentLength > 0);
        do_check_true(l.gotStartRequest);
        do_check_true(l.gotStopRequest);
        do_check_eq(l.available, chan.contentLength);

        run_next_test();
    }), null);
}

add_test(testAsync);
// Run same test again so we test the codepath for a zipcache hit
add_test(testAsync);


// In e10s child processes we don't currently support 
// 1) synchronously opening jar files on parent
// 2) nested jar channels in e10s: (app:// doesn't use them).
// 3) we can't do file lock checks on android, so skip those tests too.
if (!inChild) {

  /**
   * Basic reading test for synchronously opened jar channels
   */
  add_test(function testSync() {
      var uri = jarBase + "/inner40.zip";
      var chan = ios.newChannel(uri, null, null);
      var stream = chan.open();
      do_check_true(chan.contentLength > 0);
      do_check_eq(stream.available(), chan.contentLength);
      stream.close();
      stream.close(); // should still not throw

      run_next_test();
  });


  /**
   * Basic reading test for synchronously opened, nested jar channels
   */
  add_test(function testSyncNested() {
      var uri = "jar:" + jarBase + "/inner40.zip!/foo";
      var chan = ios.newChannel(uri, null, null);
      var stream = chan.open();
      do_check_true(chan.contentLength > 0);
      do_check_eq(stream.available(), chan.contentLength);
      stream.close();
      stream.close(); // should still not throw

      run_next_test();
  });

  /**
   * Basic reading test for asynchronously opened, nested jar channels
   */
  add_test(function testAsyncNested(next) {
      var uri = "jar:" + jarBase + "/inner40.zip!/foo";
      var chan = ios.newChannel(uri, null, null);
      chan.asyncOpen(new Listener(function(l) {
          do_check_true(chan.contentLength > 0);
          do_check_true(l.gotStartRequest);
          do_check_true(l.gotStopRequest);
          do_check_eq(l.available, chan.contentLength);

          run_next_test();
      }), null);
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
      var chan = ios.newChannel(uri, null, null);
      var stream = chan.open();
      do_check_true(chan.contentLength > 0);
      stream.close();

      // Drop any jar caches
      obs.notifyObservers(null, "chrome-flush-caches", null);

      try {
          copy.remove(false);
      }
      catch (ex) {
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
      var chan = ios.newChannel(uri, null, null);
      chan.asyncOpen(new Listener(function (l) {
          do_check_true(chan.contentLength > 0);

          // Drop any jar caches
          obs.notifyObservers(null, "chrome-flush-caches", null);

          try {
              copy.remove(false);
          }
          catch (ex) {
              do_throw(ex);
          }

          run_next_test();
      }), null);
  });

} // if !inChild

function run_test() run_next_test();
