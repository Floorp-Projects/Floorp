"use strict";

ChromeUtils.import("resource://testing-common/httpd.js");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

var httpserver = null;
XPCOMUtils.defineLazyGetter(this, "cpURI", function() {
  return "http://localhost:" + httpserver.identity.primaryPort + "/captive.txt";
});

const SUCCESS_STRING = "success\n";
let cpResponse = SUCCESS_STRING;
function contentHandler(metadata, response)
{
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(cpResponse, cpResponse.length);
}

const PREF_CAPTIVE_ENABLED = "network.captive-portal-service.enabled";
const PREF_CAPTIVE_TESTMODE = "network.captive-portal-service.testMode";
const PREF_CAPTIVE_ENDPOINT = "captivedetect.canonicalURL";
const PREF_CAPTIVE_MINTIME = "network.captive-portal-service.minInterval";
const PREF_CAPTIVE_MAXTIME = "network.captive-portal-service.maxInterval";

registerCleanupFunction(() => {
  Services.prefs.clearUserPref(PREF_CAPTIVE_ENABLED);
  Services.prefs.clearUserPref(PREF_CAPTIVE_TESTMODE);
  Services.prefs.clearUserPref(PREF_CAPTIVE_ENDPOINT);
  Services.prefs.clearUserPref(PREF_CAPTIVE_MINTIME);
  Services.prefs.clearUserPref(PREF_CAPTIVE_MAXTIME);

  httpserver.stop(() => {});
});

function observerPromise(topic) {
  return new Promise(resolve => {
    let observer = {
      QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
      observe: function(aSubject, aTopic, aData) {
        if (aTopic == topic) {
          Services.obs.removeObserver(observer, topic);
          resolve(aData);
        }
      }
    }
    Services.obs.addObserver(observer, topic);
  });
}

add_task(function setup(){
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/captive.txt", contentHandler);
  httpserver.start(-1);

  Services.prefs.setCharPref(PREF_CAPTIVE_ENDPOINT, cpURI);
  Services.prefs.setIntPref(PREF_CAPTIVE_MINTIME, 50);
  Services.prefs.setIntPref(PREF_CAPTIVE_MAXTIME, 100);
  Services.prefs.setBoolPref(PREF_CAPTIVE_TESTMODE, true);

});

add_task(async function test_simple()
{
  var cps = Cc["@mozilla.org/network/captive-portal-service;1"]
              .getService(Ci.nsICaptivePortalService);
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, false);

  equal(cps.state, Ci.nsICaptivePortalService.UNKNOWN);

  let notification = observerPromise("network:captive-portal-connectivity");
  // The service is started by nsIOService when the pref becomes true.
  // We might want to add a method to do this in the future.
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, true);

  let observerPayload = await notification;
  equal(observerPayload, "clear");
  equal(cps.state, Ci.nsICaptivePortalService.NOT_CAPTIVE);

  cpResponse = "other";
  notification = observerPromise("captive-portal-login");
  cps.recheckCaptivePortal();
  await notification;
  equal(cps.state, Ci.nsICaptivePortalService.LOCKED_PORTAL);

  cpResponse = SUCCESS_STRING;
  notification = observerPromise("captive-portal-login-success");
  cps.recheckCaptivePortal();
  await notification;
  equal(cps.state, Ci.nsICaptivePortalService.UNLOCKED_PORTAL);
});
