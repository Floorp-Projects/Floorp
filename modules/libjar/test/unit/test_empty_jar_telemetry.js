/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const nsIBinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

// Enable the collection (during test) for all products so even products
// that don't collect the data will be able to run the test without failure.
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

Services.prefs.setBoolPref("network.jar.record_failure_reason", true);

const fileBase = "test_empty_file.zip";
const file = do_get_file("data/" + fileBase);
const tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);
var copy;

function setup() {
  copy = tmpDir.clone();
  copy.append("zzdxphd909dbc6r2bxtqss2m000017");
  copy.append("zzdxphd909dbc6r2bxtqss2m000017");
  copy.append(fileBase);
  file.copyTo(copy.parent, copy.leafName);
}

setup();

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.jar.record_failure_reason");
  try {
    copy.remove(false);
  } catch (e) {}
});

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

const TELEMETRY_EVENTS_FILTERS = {
  category: "zero_byte_load",
  method: "load",
};

function makeChan() {
  var uri = "jar:" + Services.io.newFileURI(copy).spec + "!/test.txt";
  var chan = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });
  return chan;
}

add_task(async function test_empty_jar_file_async() {
  var chan = makeChan();

  Services.telemetry.setEventRecordingEnabled("zero_byte_load", true);
  Services.telemetry.clearEvents();

  await new Promise(resolve => {
    chan.asyncOpen(
      new Listener(function(l) {
        Assert.ok(chan.contentLength == 0);
        resolve();
      })
    );
  });

  TelemetryTestUtils.assertEvents(
    [
      {
        category: "zero_byte_load",
        method: "load",
        object: "others",
        value: null,
        extra: {
          sync: "false",
          file_name: `${fileBase}!/test.txt`,
          status: "NS_OK",
          cancelled: "false",
        },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );
});

add_task(async function test_empty_jar_file_sync() {
  var chan = makeChan();

  Services.telemetry.setEventRecordingEnabled("zero_byte_load", true);
  Services.telemetry.clearEvents();

  await new Promise(resolve => {
    let stream = chan.open();
    Assert.equal(stream.available(), 0);
    resolve();
  });

  TelemetryTestUtils.assertEvents(
    [
      {
        category: "zero_byte_load",
        method: "load",
        object: "others",
        value: null,
        extra: {
          sync: "true",
          file_name: `${fileBase}!/test.txt`,
          status: "NS_OK",
          cancelled: "false",
        },
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );
});
