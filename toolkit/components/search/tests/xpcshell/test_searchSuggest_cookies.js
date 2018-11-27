/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

/**
 * Testing search suggestions from SearchSuggestionController.jsm.
 */

"use strict";

ChromeUtils.import("resource://gre/modules/FormHistory.jsm");
ChromeUtils.import("resource://gre/modules/SearchSuggestionController.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm");

// We must make sure the FormHistoryStartup component is
// initialized in order for it to respond to FormHistory
// requests from nsFormAutoComplete.js.
var formHistoryStartup = Cc["@mozilla.org/satchel/form-history-startup;1"].
                         getService(Ci.nsIObserver);
formHistoryStartup.observe(null, "profile-after-change", null);

var httpServer = new HttpServer();
var getEngine;

add_task(async function setup() {
  Services.prefs.setBoolPref("browser.search.suggest.enabled", true);

  let server = useHttpServer();
  server.registerContentType("sjs", "sjs");

  registerCleanupFunction(async function cleanup() {
    Services.prefs.clearUserPref("browser.search.suggest.enabled");
  });

  [getEngine] = await addTestEngines([
    {
      name: "GET suggestion engine",
      xmlFileName: "engineMaker.sjs?" + JSON.stringify({
        baseURL: gDataUrl,
        name: "GET suggestion engine",
        method: "GET",
      }),
    },
  ]);
});

add_task(async function test_private() {
  await new Promise(resolve => {
    let controller = new SearchSuggestionController((result) => {
      Assert.equal(result.term, "cookie");
      Assert.equal(result.local.length, 0);
      Assert.equal(result.remote.length, 0);
      resolve();
    });
    controller.fetch("cookie", getEngine);
  });
  info("Enumerating cookies");
  let enumerator = Services.cookies.enumerator;
  let cookies = [];
  for (let cookie of enumerator) {
    info("Cookie:" + cookie.rawHost + " " + JSON.stringify(cookie.originAttributes));
    cookies.push(cookie);
    break;
  }
  Assert.equal(cookies.length, 0, "Should not find any cookie");
});

add_task(async function test_nonprivate() {
  let controller;
  await new Promise(resolve => {
     controller = new SearchSuggestionController((result) => {
      Assert.equal(result.term, "cookie");
      Assert.equal(result.local.length, 0);
      Assert.equal(result.remote.length, 0);
      resolve();
    });
    controller.fetch("cookie", getEngine, 0, false);
  });
  info("Enumerating cookies");
  let enumerator = Services.cookies.enumerator;
  let cookies = [];
  for (let cookie of enumerator) {
    info("Cookie:" + cookie.rawHost + " " + JSON.stringify(cookie.originAttributes));
    cookies.push(cookie);
    break;
  }
  Assert.equal(cookies.length, 1, "Should find one cookie");
  Assert.equal(cookies[0].originAttributes.firstPartyDomain,
               controller.FIRST_PARTY_DOMAIN, "Check firstPartyDomain");
});
