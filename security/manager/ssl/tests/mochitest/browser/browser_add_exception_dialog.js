/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test makes sure that adding certificate exceptions behaves correctly
// when done from the prefs window

ChromeUtils.defineESModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
});

function test() {
  const EXCEPTIONS_DLG_URL = "chrome://pippki/content/exceptionDialog.xhtml";
  const EXCEPTIONS_DLG_FEATURES = "chrome,centerscreen";
  const INVALID_CERT_DOMAIN = "self-signed.example.com";
  const INVALID_CERT_LOCATION = "https://" + INVALID_CERT_DOMAIN + "/";
  waitForExplicitFinish();

  function testAddCertificate() {
    win.removeEventListener("load", testAddCertificate);
    Services.obs.addObserver(async function onCertUI(aSubject, aTopic, aData) {
      Services.obs.removeObserver(onCertUI, "cert-exception-ui-ready");
      ok(win.gCert, "The certificate information should be available now");

      let dialog = win.document.getElementById("exceptiondialog");
      let confirmButton = dialog.getButton("extra1");
      confirmButton.click();
      ok(
        params.exceptionAdded,
        "The certificate exception should have been added"
      );

      registerCleanupFunction(() => {
        let certOverrideService = Cc[
          "@mozilla.org/security/certoverride;1"
        ].getService(Ci.nsICertOverrideService);
        certOverrideService.clearValidityOverride(INVALID_CERT_DOMAIN, -1, {});
      });

      BrowserTestUtils.startLoadingURIString(gBrowser, INVALID_CERT_LOCATION);
      let loaded = await BrowserTestUtils.browserLoaded(
        gBrowser,
        false,
        INVALID_CERT_LOCATION,
        true
      );
      ok(loaded, "The certificate exception should allow the page to load");

      finish();
    }, "cert-exception-ui-ready");
  }

  let bWin = BrowserWindowTracker.getTopWindow();
  let params = {
    exceptionAdded: false,
    location: INVALID_CERT_LOCATION,
    prefetchCert: true,
  };

  let win = bWin.openDialog(
    EXCEPTIONS_DLG_URL,
    "",
    EXCEPTIONS_DLG_FEATURES,
    params
  );
  win.addEventListener("load", testAddCertificate);
}
