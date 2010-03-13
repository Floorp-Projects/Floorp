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
 * The Original Code is the Application Update Service.
 *
 * The Initial Developer of the Original Code is
 * Robert Strong <robert.bugzilla@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
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
 * ***** END LICENSE BLOCK *****
 */

/* General MAR File Download Tests */

var gNextRunFunc;
var gStatusResult;
var gExpectedStatusResult;

function run_test() {
  do_test_pending();
  removeUpdateDirsAndFiles();
  setUpdateURLOverride();
  // The mock XMLHttpRequest is MUCH faster
  overrideXHR(callHandleEvent);
  standardInit();
  // The HTTP server is only used for the mar file downloads which is slow
  start_httpserver(URL_PATH);
  do_timeout(0, run_test_pt1);
}

function end_test() {
  stop_httpserver(do_test_finished);
  cleanUp();
}

// Callback function used by the custom XMLHttpRequest implementation to
// call the nsIDOMEventListener's handleEvent method for onload.
function callHandleEvent() {
  gXHR.status = 400;
  gXHR.responseText = gResponseBody;
  try {
    var parser = AUS_Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(AUS_Ci.nsIDOMParser);
    gXHR.responseXML = parser.parseFromString(gResponseBody, "application/xml");
  }
  catch(e) {
  }
  var e = { target: gXHR };
  gXHR.onload.handleEvent(e);
}

// Helper function for testing mar downloads that have the correct size
// specified in the update xml.
function run_test_helper_pt1(aMsg, aExpectedStatusResult, aNextRunFunc) {
  gUpdates = null;
  gUpdateCount = null;
  gStatusResult = null;
  gCheckFunc = check_test_helper_pt1_1;
  gNextRunFunc = aNextRunFunc;
  gExpectedStatusResult = aExpectedStatusResult;
  dump("Testing: " + aMsg + "\n");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_helper_pt1_1() {
  do_check_eq(gUpdateCount, 1);
  gCheckFunc = check_test_helper_pt1_2;
  var bestUpdate = gAUS.selectUpdate(gUpdates, gUpdateCount);
  var state = gAUS.downloadUpdate(bestUpdate, false);
  if (state == STATE_NONE || state == STATE_FAILED)
    do_throw("nsIApplicationUpdateService:downloadUpdate returned " + state);
  gAUS.addDownloadListener(downloadListener);
}

function check_test_helper_pt1_2() {
  do_check_eq(gStatusResult, gExpectedStatusResult);
  gAUS.removeDownloadListener(downloadListener);
  gNextRunFunc();
}

function setResponseBody(aHashFunction, aHashValue) {
  var patches = getRemotePatchString(null, null, aHashFunction, aHashValue);
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
}

// mar download with a valid MD5 hash
function run_test_pt1() {
  setResponseBody("MD5", "6232cd43a1c77e30191c53a329a3f99d");
  run_test_helper_pt1("run_test_pt1 - mar download with a valid MD5 hash",
                      AUS_Cr.NS_OK, run_test_pt2);
}

// mar download with an invalid MD5 hash
function run_test_pt2() {
  setResponseBody("MD5", "6232cd43a1c77e30191c53a329a3f99e");
  run_test_helper_pt1("run_test_pt2 - mar download with an invalid MD5 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt3);
}

// mar download with a valid SHA1 hash
function run_test_pt3() {
  setResponseBody("SHA1", "63A739284A1A73ECB515176B1A9D85B987E789CE");
  run_test_helper_pt1("run_test_pt3 - mar download with a valid SHA1 hash",
                      AUS_Cr.NS_OK, run_test_pt4);
}

// mar download with an invalid SHA1 hash
function run_test_pt4() {
  setResponseBody("SHA1", "63A739284A1A73ECB515176B1A9D85B987E789CD");
  run_test_helper_pt1("run_test_pt4 - mar download with an invalid SHA1 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt5);
}

// mar download with a valid SHA256 hash
function run_test_pt5() {
  var hashValue = "a8d9189f3978afd90dc7cd72e887ef22474c178e8314f23df2f779c881" +
                  "b872e2";
  setResponseBody("SHA256", hashValue);
  run_test_helper_pt1("run_test_pt5 - mar download with a valid SHA256 hash",
                      AUS_Cr.NS_OK, run_test_pt6);
}

// mar download with an invalid SHA256 hash
function run_test_pt6() {
  var hashValue = "a8d9189f3978afd90dc7cd72e887ef22474c178e8314f23df2f779c881" +
                  "b872e1";
  setResponseBody("SHA256", hashValue);
  run_test_helper_pt1("run_test_pt6 - mar download with an invalid SHA256 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt7);
}

// mar download with a valid SHA384 hash
function run_test_pt7() {
  var hashValue = "802c64f6caa6c356f7a5f8d9a008c08c54fe915c3ec7cf9e215c3bccc9" +
                  "e195c78b2669840d7b1d46ff3c1dfa751d72e1";
  setResponseBody("SHA384", hashValue);
  run_test_helper_pt1("run_test_pt7 - mar download with a valid SHA384 hash",
                      AUS_Cr.NS_OK, run_test_pt8);
}

// mar download with an invalid SHA384 hash
function run_test_pt8() {
  var hashValue = "802c64f6caa6c356f7a5f8d9a008c08c54fe915c3ec7cf9e215c3bccc9" +
                  "e195c78b2669840d7b1d46ff3c1dfa751d72e2";
  setResponseBody("SHA384", hashValue);
  run_test_helper_pt1("run_test_pt8 - mar download with an invalid SHA384 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt9);
}

// mar download with a valid SHA512 hash
function run_test_pt9() {
  var hashValue = "1d2307e309587ddd04299423b34762639ce6af3ee17cfdaa8fdd4e66b5" +
                  "a61bfb6555b6e40a82604908d6d68d3e42f318f82e22b6f5e1118b4222" +
                  "e3417a2fa2d0";
  setResponseBody("SHA512", hashValue);
  run_test_helper_pt1("run_test_pt9 - mar download with a valid SHA512 hash",
                      AUS_Cr.NS_OK, run_test_pt10);
}

// mar download with an invalid SHA384 hash
function run_test_pt10() {
  var hashValue = "1d2307e309587ddd04299423b34762639ce6af3ee17cfdaa8fdd4e66b5" +
                  "a61bfb6555b6e40a82604908d6d68d3e42f318f82e22b6f5e1118b4222" +
                  "e3417a2fa2d1";
  setResponseBody("SHA512", hashValue);
  run_test_helper_pt1("run_test_pt10 - mar download with an invalid SHA512 hash",
                      AUS_Cr.NS_ERROR_UNEXPECTED, run_test_pt11);
}

// mar download with the mar not found
function run_test_pt11() {
  var patches = getRemotePatchString(null, URL_HOST + URL_PATH + "/missing.mar");
  var updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  run_test_helper_pt1("run_test_pt11 - mar download with the mar not found",
                      AUS_Cr.NS_ERROR_UNEXPECTED, end_test);
}

/* Update download listener - nsIRequestObserver */
const downloadListener = {
  onStartRequest: function(request, context) {
  },

  onProgress: function(request, context, progress, maxProgress) {
  },

  onStatus: function(request, context, status, statusText) {
  },

  onStopRequest: function(request, context, status) {
    gStatusResult = status;
    // Use a timeout to allow the request to complete
    do_timeout(0, gCheckFunc);
  },

  QueryInterface: function(iid) {
    if (!iid.equals(AUS_Ci.nsIRequestObserver) &&
        !iid.equals(AUS_Ci.nsIProgressEventSink) &&
        !iid.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};
