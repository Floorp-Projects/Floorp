/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../../../../../testing/marionette/harness/marionette/atoms/b2g_update_test.js */

const TEST_URL = "http://localhost";

setPref("b2g.update.apply-idle-timeout", 0);
setPref("app.update.backgroundErrors", 0);
setPref("app.update.backgroundMaxErrors", 100);

function forceCheckAndTestStatus(status, next) {
  let mozSettings = window.navigator.mozSettings;
  let forceSent = false;

  mozSettings.addObserver("gecko.updateStatus", function statusObserver(setting) {
    if (!forceSent) {
      return;
    }

    mozSettings.removeObserver("gecko.updateStatus", statusObserver);
    is(setting.settingValue, status, "gecko.updateStatus");
    next();
  });

  sendContentEvent("force-update-check");
  forceSent = true;
}

function testBadXml() {
  setDefaultPref("app.update.url", TEST_URL + "/bad.xml");
  forceCheckAndTestStatus("check-error-http-200", testAccessDenied);
}

function testAccessDenied() {
  setDefaultPref("app.update.url", TEST_URL + "/cgi-bin/err.cgi?403");
  forceCheckAndTestStatus("check-error-http-403", testNoUpdateXml);
}

function testNoUpdateXml() {
  setDefaultPref("app.update.url", TEST_URL + "/none.html");
  forceCheckAndTestStatus("check-error-http-404", testInternalServerError);
}

function testInternalServerError() {
  setDefaultPref("app.update.url", TEST_URL + "/cgi-bin/err.cgi?500");
  forceCheckAndTestStatus("check-error-http-500", testBadHostStatus);
}

function testBadHostStatus() {
  setDefaultPref("app.update.url", "http://bad-host-doesnt-exist-sorry.com");
  forceCheckAndTestStatus("check-error-" + Cr.NS_ERROR_UNKNOWN_HOST, cleanUp);
}

// Update test functions
function preUpdate() {
  testBadXml();
}
