"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

let httpserver = null;
XPCOMUtils.defineLazyGetter(this, "cpURI", function() {
  return "http://localhost:" + httpserver.identity.primaryPort + "/captive.txt";
});

const SUCCESS_STRING =
  '<meta http-equiv="refresh" content="0;url=https://support.mozilla.org/kb/captive-portal"/>';
let cpResponse = SUCCESS_STRING;
function contentHandler(metadata, response) {
  response.setHeader("Content-Type", "text/plain");
  response.bodyOutputStream.write(cpResponse, cpResponse.length);
}

const PREF_CAPTIVE_ENABLED = "network.captive-portal-service.enabled";
const PREF_CAPTIVE_TESTMODE = "network.captive-portal-service.testMode";
const PREF_CAPTIVE_ENDPOINT = "captivedetect.canonicalURL";
const PREF_CAPTIVE_MINTIME = "network.captive-portal-service.minInterval";
const PREF_CAPTIVE_MAXTIME = "network.captive-portal-service.maxInterval";
const PREF_DNS_NATIVE_IS_LOCALHOST = "network.dns.native-is-localhost";

const cps = Cc["@mozilla.org/network/captive-portal-service;1"].getService(
  Ci.nsICaptivePortalService
);

registerCleanupFunction(async () => {
  Services.prefs.clearUserPref(PREF_CAPTIVE_ENABLED);
  Services.prefs.clearUserPref(PREF_CAPTIVE_TESTMODE);
  Services.prefs.clearUserPref(PREF_CAPTIVE_ENDPOINT);
  Services.prefs.clearUserPref(PREF_CAPTIVE_MINTIME);
  Services.prefs.clearUserPref(PREF_CAPTIVE_MAXTIME);
  Services.prefs.clearUserPref(PREF_DNS_NATIVE_IS_LOCALHOST);

  await new Promise(resolve => {
    httpserver.stop(resolve);
  });
});

function observerPromise(topic) {
  return new Promise(resolve => {
    let observer = {
      QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
      observe(aSubject, aTopic, aData) {
        if (aTopic == topic) {
          Services.obs.removeObserver(observer, topic);
          resolve(aData);
        }
      },
    };
    Services.obs.addObserver(observer, topic);
  });
}

add_task(function setup() {
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/captive.txt", contentHandler);
  httpserver.start(-1);

  Services.prefs.setCharPref(PREF_CAPTIVE_ENDPOINT, cpURI);
  Services.prefs.setIntPref(PREF_CAPTIVE_MINTIME, 50);
  Services.prefs.setIntPref(PREF_CAPTIVE_MAXTIME, 100);
  Services.prefs.setBoolPref(PREF_CAPTIVE_TESTMODE, true);
  Services.prefs.setBoolPref(PREF_DNS_NATIVE_IS_LOCALHOST, true);
});

add_task(async function test_simple() {
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

// This test redirects to another URL which returns the same content.
// It should still be interpreted as a captive portal.
add_task(async function test_redirect_success() {
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, false);
  equal(cps.state, Ci.nsICaptivePortalService.UNKNOWN);

  httpserver.registerPathHandler("/succ.txt", (metadata, response) => {
    response.setHeader("Content-Type", "text/plain");
    response.bodyOutputStream.write(cpResponse, cpResponse.length);
  });
  httpserver.registerPathHandler("/captive.txt", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 307, "Moved Temporarily");
    response.setHeader(
      "Location",
      `http://localhost:${httpserver.identity.primaryPort}/succ.txt`
    );
  });

  let notification = observerPromise("captive-portal-login").then(
    () => "login"
  );
  let succNotif = observerPromise("network:captive-portal-connectivity").then(
    () => "connectivity"
  );
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, true);

  let winner = await Promise.race([notification, succNotif]);
  equal(winner, "login", "This should have been a login, not a success");
  equal(
    cps.state,
    Ci.nsICaptivePortalService.LOCKED_PORTAL,
    "Should be locked after redirect to same text"
  );
});

// This redirects to another URI with a different content.
// We check that it triggers a captive portal login
add_task(async function test_redirect_bad() {
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, false);
  equal(cps.state, Ci.nsICaptivePortalService.UNKNOWN);

  httpserver.registerPathHandler("/bad.txt", (metadata, response) => {
    response.setHeader("Content-Type", "text/plain");
    response.bodyOutputStream.write("bad", "bad".length);
  });

  httpserver.registerPathHandler("/captive.txt", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 307, "Moved Temporarily");
    response.setHeader(
      "Location",
      `http://localhost:${httpserver.identity.primaryPort}/bad.txt`
    );
  });

  let notification = observerPromise("captive-portal-login");
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, true);

  await notification;
  equal(
    cps.state,
    Ci.nsICaptivePortalService.LOCKED_PORTAL,
    "Should be locked after redirect to bad text"
  );
});

// This redirects to the same URI.
// We check that it triggers a captive portal login
add_task(async function test_redirect_loop() {
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, false);
  equal(cps.state, Ci.nsICaptivePortalService.UNKNOWN);

  // This is actually a redirect loop
  httpserver.registerPathHandler("/captive.txt", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 307, "Moved Temporarily");
    response.setHeader("Location", cpURI);
  });

  let notification = observerPromise("captive-portal-login");
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, true);

  await notification;
  equal(cps.state, Ci.nsICaptivePortalService.LOCKED_PORTAL);
});

// This redirects to a https URI.
// We check that it triggers a captive portal login
add_task(async function test_redirect_https() {
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, false);
  equal(cps.state, Ci.nsICaptivePortalService.UNKNOWN);

  let h2Port = Cc["@mozilla.org/process/environment;1"]
    .getService(Ci.nsIEnvironment)
    .get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Any kind of redirection should trigger the captive portal login.
  httpserver.registerPathHandler("/captive.txt", (metadata, response) => {
    response.setStatusLine(metadata.httpVersion, 307, "Moved Temporarily");
    response.setHeader("Location", `https://foo.example.com:${h2Port}/exit`);
  });

  let notification = observerPromise("captive-portal-login");
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, true);

  await notification;
  equal(
    cps.state,
    Ci.nsICaptivePortalService.LOCKED_PORTAL,
    "Should be locked after redirect to https"
  );
  Services.prefs.setBoolPref(PREF_CAPTIVE_ENABLED, false);
});
