/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cc["@mozilla.org/moz/jssubscript-loader;1"]
  .getService(Ci.mozIJSSubScriptLoader)
  .loadSubScript("chrome://mochikit/content/tests/SimpleTest/MockObjects.js", this);

var mockTransferCallback;

/**
 * This "transfer" object implementation continues the currently running test
 * when the download is completed, reporting true for success or false for
 * failure as the first argument of the testRunner.continueTest function.
 */
function MockTransfer() {
  this._downloadIsSuccessful = true;
}

MockTransfer.prototype = {
  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIWebProgressListener,
    Ci.nsIWebProgressListener2,
    Ci.nsITransfer,
  ]),

  /* nsIWebProgressListener */
  onStateChange: function MTFC_onStateChange(aWebProgress, aRequest,
                                             aStateFlags, aStatus) {
    // If at least one notification reported an error, the download failed.
    if (!Components.isSuccessCode(aStatus))
      this._downloadIsSuccessful = false;

    // If the download is finished
    if ((aStateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
        (aStateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK))
      // Continue the test, reporting the success or failure condition.
      mockTransferCallback(this._downloadIsSuccessful);
  },
  onProgressChange() {},
  onLocationChange() {},
  onStatusChange: function MTFC_onStatusChange(aWebProgress, aRequest, aStatus,
                                               aMessage) {
    // If at least one notification reported an error, the download failed.
    if (!Components.isSuccessCode(aStatus))
      this._downloadIsSuccessful = false;
  },
  onSecurityChange() {},

  /* nsIWebProgressListener2 */
  onProgressChange64() {},
  onRefreshAttempted() {},

  /* nsITransfer */
  init() {},
  setSha256Hash() {},
  setSignatureInfo() {}
};

// Create an instance of a MockObjectRegisterer whose methods can be used to
// temporarily replace the default "@mozilla.org/transfer;1" object factory with
// one that provides the mock implementation above. To activate the mock object
// factory, call the "register" method. Starting from that moment, all the
// transfer objects that are requested will be mock objects, until the
// "unregister" method is called.
var mockTransferRegisterer =
  new MockObjectRegisterer("@mozilla.org/transfer;1", MockTransfer);
