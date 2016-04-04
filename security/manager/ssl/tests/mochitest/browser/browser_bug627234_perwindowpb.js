/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var FakeSSLStatus = function() {
};

FakeSSLStatus.prototype = {
  serverCert: null,
  cipherName: null,
  keyLength: 2048,
  isDomainMismatch: false,
  isNotValidAtThisTime: false,
  isUntrusted: false,
  isExtendedValidation: false,
  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },
  QueryInterface: function(aIID) {
    if (aIID.equals(Ci.nsISSLStatus) ||
        aIID.equals(Ci.nsISupports)) {
      return this;
    }
    throw new Error(Cr.NS_ERROR_NO_INTERFACE);
  },
};

// This is a template to help porting global private browsing tests
// to per-window private browsing tests
function test() {
  // initialization
  waitForExplicitFinish();
  let windowsToClose = [];
  let testURI = "about:blank";
  let uri;
  let gSSService = Cc["@mozilla.org/ssservice;1"].
                   getService(Ci.nsISiteSecurityService);

  function privacyFlags(aIsPrivateMode) {
    return aIsPrivateMode ? Ci.nsISocketProvider.NO_PERMANENT_STORAGE : 0;
  }

  function doTest(aIsPrivateMode, aWindow, aCallback) {
    aWindow.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      aWindow.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
      let sslStatus = new FakeSSLStatus();
      uri = aWindow.Services.io.newURI("https://localhost/img.png", null, null);
      gSSService.processHeader(Ci.nsISiteSecurityService.HEADER_HSTS, uri,
                               "max-age=1000", sslStatus, privacyFlags(aIsPrivateMode));
      ok(gSSService.isSecureHost(Ci.nsISiteSecurityService.HEADER_HSTS, "localhost", privacyFlags(aIsPrivateMode)), "checking sts host");

      aCallback();
    }, true);

    aWindow.gBrowser.selectedBrowser.loadURI(testURI);
  }

  function testOnWindow(aOptions, aCallback) {
    whenNewWindowLoaded(aOptions, function(aWin) {
      windowsToClose.push(aWin);
      // execute should only be called when need, like when you are opening
      // web pages on the test. If calling executeSoon() is not necesary, then
      // call whenNewWindowLoaded() instead of testOnWindow() on your test.
      executeSoon(function() { aCallback(aWin); });
    });
  }

   // this function is called after calling finish() on the test.
  registerCleanupFunction(function() {
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
    uri = Services.io.newURI("http://localhost", null, null);
    gSSService.removeState(Ci.nsISiteSecurityService.HEADER_HSTS, uri, 0);
  });

  // test first when on private mode
  testOnWindow({private: true}, function(aWin) {
    doTest(true, aWin, function() {
      //test when not on private mode
      testOnWindow({}, function(aWin) {
        doTest(false, aWin, function() {
          //test again when on private mode
          testOnWindow({private: true}, function(aWin) {
            doTest(true, aWin, function () {
              finish();
            });
          });
        });
      });
    });
  });
}
