/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
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

const fileBase = "test_empty_file.zip";
const file = do_get_file("data/" + fileBase);
const jarBase = "jar:" + Services.io.newFileURI(file).spec + "!";
const tmpDir = Services.dirsvc.get("TmpD", Ci.nsIFile);

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
  category: "network.jar.channel",
  method: "noData",
};

add_task(async function test_empty_jar_file() {
  var copy = tmpDir.clone();
  copy.append("zzdxphd909dbc6r2bxtqss2m000017");
  copy.append("zzdxphd909dbc6r2bxtqss2m000017");
  copy.append(fileBase);
  file.copyTo(copy.parent, copy.leafName);

  var uri = "jar:" + Services.io.newFileURI(copy).spec + "!/test.txt";
  var chan = NetUtil.newChannel({ uri, loadUsingSystemPrincipal: true });

  Services.telemetry.setEventRecordingEnabled("network.jar.channel", true);
  Services.telemetry.clearEvents();

  await new Promise(resolve => {
    chan.asyncOpen(
      new Listener(function(l) {
        Assert.ok(chan.contentLength == 0);

        resolve();
      })
    );
  });

  try {
    copy.remove(false);
  } catch (e) {}

  TelemetryTestUtils.assertEvents(
    [
      {
        category: "network.jar.channel",
        method: "noData",
        object: "onStop",
        value: `${fileBase}!/test.txt`,
      },
    ],
    TELEMETRY_EVENTS_FILTERS
  );
});
