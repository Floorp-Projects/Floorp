/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla XUL Toolkit Testing Code.
 *
 * The Initial Developer of the Original Code is
 * Paolo Amadini <http://www.amadzone.org/>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
  onProgressChange: function () {},
  onLocationChange: function () {},
  onStatusChange: function MTFC_onStatusChange(aWebProgress, aRequest, aStatus,
                                               aMessage) {
    // If at least one notification reported an error, the download failed.
    if (!Components.isSuccessCode(aStatus))
      this._downloadIsSuccessful = false;
  },
  onSecurityChange: function () {},

  /* nsIWebProgressListener2 */
  onProgressChange64: function () {},
  onRefreshAttempted: function () {},

  /* nsITransfer */
  init: function () {}
};

// Create an instance of a MockObjectRegisterer whose methods can be used to
// temporarily replace the default "@mozilla.org/transfer;1" object factory with
// one that provides the mock implementation above. To activate the mock object
// factory, call the "register" method. Starting from that moment, all the
// transfer objects that are requested will be mock objects, until the
// "unregister" method is called.
var mockTransferRegisterer =
  new MockObjectRegisterer("@mozilla.org/transfer;1",  MockTransfer);
