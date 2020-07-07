/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "peuService",
  "@mozilla.org/partitioning/exception-list-service;1",
  "nsIPartitioningExceptionListService"
);

const TEST_REDIRECT_TOP_PAGE =
  TEST_3RD_PARTY_DOMAIN + TEST_PATH + "redirect.sjs?" + TEST_TOP_PAGE;
const TEST_REDIRECT_3RD_PARTY_PAGE =
  TEST_DOMAIN + TEST_PATH + "redirect.sjs?" + TEST_3RD_PARTY_PARTITIONED_PAGE;

const COLLECTION_NAME = "partitioning-exempt-urls";
const EXCEPTION_LIST_PREF_NAME = "privacy.restrict3rdpartystorage.skip_list";

async function cleanup() {
  Services.prefs.clearUserPref(EXCEPTION_LIST_PREF_NAME);
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
}

add_task(async function setup() {
  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
      ["privacy.restrict3rdpartystorage.heuristic.redirect", false],
      ["privacy.trackingprotection.enabled", false],
      ["privacy.trackingprotection.pbmode.enabled", false],
      ["privacy.trackingprotection.annotate_channels", true],
    ],
  });
  registerCleanupFunction(cleanup);
});

function executeContentScript(browser, callback, options = {}) {
  return SpecialPowers.spawn(
    browser,
    [
      {
        callback: callback.toString(),
        ...options,
      },
    ],
    obj => {
      return new content.Promise(async resolve => {
        if (obj.page) {
          // third-party
          let ifr = content.document.createElement("iframe");
          ifr.onload = async () => {
            info("Sending code to the 3rd party content");
            ifr.contentWindow.postMessage(
              { cb: obj.callback, value: obj.value },
              "*"
            );
          };

          content.addEventListener("message", event => resolve(event.data), {
            once: true,
          });

          content.document.body.appendChild(ifr);
          ifr.src = obj.page;
        } else {
          // first-party
          let runnableStr = `(() => {return (${obj.callback});})();`;
          let runnable = eval(runnableStr); // eslint-disable-line no-eval
          resolve(await runnable.call(content, content, obj.value));
        }
      });
    }
  );
}

function readNetworkCookie(win) {
  return win
    .fetch("cookies.sjs")
    .then(r => r.text())
    .then(text => {
      return text.substring("cookie:foopy=".length);
    });
}

async function writeNetworkCookie(win, value) {
  await win.fetch("cookies.sjs?" + value).then(r => r.text());
  return true;
}

function createDataInFirstParty(browser, value) {
  return executeContentScript(browser, writeNetworkCookie, { value });
}
function getDataFromFirstParty(browser) {
  return executeContentScript(browser, readNetworkCookie, {});
}
function createDataInThirdParty(browser, value) {
  return executeContentScript(browser, writeNetworkCookie, {
    page: TEST_3RD_PARTY_PARTITIONED_PAGE,
    value,
  });
}
function getDataFromThirdParty(browser) {
  return executeContentScript(browser, readNetworkCookie, {
    page: TEST_3RD_PARTY_PARTITIONED_PAGE,
  });
}

async function redirectWithUserInteraction(browser, url, wait = null) {
  await executeContentScript(
    browser,
    (content, value) => {
      content.document.userInteractionForTesting();

      let link = content.document.createElement("a");
      link.appendChild(content.document.createTextNode("click me!"));
      link.href = value;
      content.document.body.appendChild(link);
      link.click();
    },
    {
      value: url,
    }
  );
  await BrowserTestUtils.browserLoaded(browser, false, wait || url);
}

async function checkData(browser, options) {
  if ("firstParty" in options) {
    is(
      await getDataFromFirstParty(browser),
      options.firstParty,
      "correct first-party data"
    );
  }
  if ("thirdParty" in options) {
    is(
      await getDataFromThirdParty(browser),
      options.thirdParty,
      "correct third-party data"
    );
  }
}

add_task(async function testRedirectHeuristic() {
  info("Starting Dynamic FPI Redirect Heuristic test...");

  await SpecialPowers.pushPrefEnv({
    set: [["privacy.restrict3rdpartystorage.heuristic.recently_visited", true]],
  });

  // mark third-party as tracker
  await UrlClassifierTestUtils.addTestTrackers();

  info("Creating a new tab");
  let tab = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tab;

  let browser = gBrowser.getBrowserForTab(tab);
  await BrowserTestUtils.browserLoaded(browser);

  info("initializing...");
  await checkData(browser, { firstParty: "", thirdParty: "" });

  await Promise.all([
    createDataInFirstParty(browser, "firstParty"),
    createDataInThirdParty(browser, "thirdParty"),
  ]);

  await checkData(browser, {
    firstParty: "firstParty",
    thirdParty: "",
  });

  info("load third-party content as first-party");
  await redirectWithUserInteraction(
    browser,
    TEST_REDIRECT_3RD_PARTY_PAGE,
    TEST_3RD_PARTY_PARTITIONED_PAGE
  );

  await checkData(browser, { firstParty: "" });
  await createDataInFirstParty(browser, "heuristicFirstParty");
  await checkData(browser, { firstParty: "heuristicFirstParty" });

  info("redirect back to first-party page");
  await redirectWithUserInteraction(
    browser,
    TEST_REDIRECT_TOP_PAGE,
    TEST_TOP_PAGE
  );

  info("third-party tracker should NOT able to access first-party data");
  await checkData(browser, {
    firstParty: "firstParty",
    thirdParty: "",
  });

  // remove third-party from tracker
  await UrlClassifierTestUtils.cleanupTestTrackers();

  info("load third-party content as first-party");
  await redirectWithUserInteraction(
    browser,
    TEST_REDIRECT_3RD_PARTY_PAGE,
    TEST_3RD_PARTY_PARTITIONED_PAGE
  );

  await checkData(browser, {
    firstParty: "heuristicFirstParty",
  });

  info("redirect back to first-party page");
  await redirectWithUserInteraction(
    browser,
    TEST_REDIRECT_TOP_PAGE,
    TEST_TOP_PAGE
  );

  info("third-party page should able to access first-party data");
  await checkData(browser, {
    firstParty: "firstParty",
    thirdParty: "heuristicFirstParty",
  });

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);

  await cleanup();
});

class UpdateEvent extends EventTarget {}
function waitForEvent(element, eventName) {
  return new Promise(function(resolve) {
    element.addEventListener(eventName, e => resolve(e.detail), { once: true });
  });
}

add_task(async function testExceptionListPref() {
  info("Starting Dynamic FPI exception list test pref");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.restrict3rdpartystorage.heuristic.recently_visited", false],
    ],
  });

  info("Creating new tabs");
  let tabThirdParty = BrowserTestUtils.addTab(
    gBrowser,
    TEST_3RD_PARTY_PARTITIONED_PAGE
  );
  gBrowser.selectedTab = tabThirdParty;

  let browserThirdParty = gBrowser.getBrowserForTab(tabThirdParty);
  await BrowserTestUtils.browserLoaded(browserThirdParty);

  let tabFirstParty = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tabFirstParty;

  let browserFirstParty = gBrowser.getBrowserForTab(tabFirstParty);
  await BrowserTestUtils.browserLoaded(browserFirstParty);

  info("initializing...");
  await Promise.all([
    checkData(browserFirstParty, { firstParty: "", thirdParty: "" }),
    checkData(browserThirdParty, { firstParty: "" }),
  ]);

  info("fill default data");
  await Promise.all([
    createDataInFirstParty(browserFirstParty, "firstParty"),
    createDataInThirdParty(browserFirstParty, "thirdParty"),
    createDataInFirstParty(browserThirdParty, "ExceptionListFirstParty"),
  ]);

  info("check data");
  await Promise.all([
    checkData(browserFirstParty, {
      firstParty: "firstParty",
      thirdParty: "thirdParty",
    }),
    checkData(browserThirdParty, { firstParty: "ExceptionListFirstParty" }),
  ]);

  info("set exception list pref");
  Services.prefs.setStringPref(
    EXCEPTION_LIST_PREF_NAME,
    `${TEST_DOMAIN},${TEST_3RD_PARTY_DOMAIN}`
  );

  info("check data");
  await Promise.all([
    checkData(browserFirstParty, {
      firstParty: "firstParty",
      thirdParty: "ExceptionListFirstParty",
    }),
    checkData(browserThirdParty, { firstParty: "ExceptionListFirstParty" }),
  ]);

  info("Removing the tab");
  BrowserTestUtils.removeTab(tabFirstParty);
  BrowserTestUtils.removeTab(tabThirdParty);

  await cleanup();
});

add_task(async function testExceptionListRemoteSettings() {
  info("Starting Dynamic FPI exception list test (remote settings)");

  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.restrict3rdpartystorage.heuristic.recently_visited", false],
    ],
  });

  // Make sure we have a pref initially, since the exception list service
  // requires it.
  Services.prefs.setStringPref(EXCEPTION_LIST_PREF_NAME, "");

  // Add some initial data
  let db = await RemoteSettings(COLLECTION_NAME).db;
  await db.importChanges({}, 42, []);

  // make peuSerivce start working by calling
  // registerAndRunExceptionListObserver
  let updateEvent = new UpdateEvent();
  let obs = data => {
    let event = new CustomEvent("update", { detail: data });
    updateEvent.dispatchEvent(event);
  };
  let promise = waitForEvent(updateEvent, "update");
  peuService.registerAndRunExceptionListObserver(obs);
  await promise;

  info("Creating new tabs");
  let tabThirdParty = BrowserTestUtils.addTab(
    gBrowser,
    TEST_3RD_PARTY_PARTITIONED_PAGE
  );
  gBrowser.selectedTab = tabThirdParty;

  let browserThirdParty = gBrowser.getBrowserForTab(tabThirdParty);
  await BrowserTestUtils.browserLoaded(browserThirdParty);

  let tabFirstParty = BrowserTestUtils.addTab(gBrowser, TEST_TOP_PAGE);
  gBrowser.selectedTab = tabFirstParty;

  let browserFirstParty = gBrowser.getBrowserForTab(tabFirstParty);
  await BrowserTestUtils.browserLoaded(browserFirstParty);

  info("initializing...");
  await Promise.all([
    checkData(browserFirstParty, { firstParty: "", thirdParty: "" }),
    checkData(browserThirdParty, { firstParty: "" }),
  ]);

  info("fill default data");
  await Promise.all([
    createDataInFirstParty(browserFirstParty, "firstParty"),
    createDataInThirdParty(browserFirstParty, "thirdParty"),
    createDataInFirstParty(browserThirdParty, "ExceptionListFirstParty"),
  ]);

  info("check data");
  await Promise.all([
    checkData(browserFirstParty, {
      firstParty: "firstParty",
      thirdParty: "thirdParty",
    }),
    checkData(browserThirdParty, { firstParty: "ExceptionListFirstParty" }),
  ]);

  info("set exception list remote settings");

  // set records
  promise = waitForEvent(updateEvent, "update");
  await RemoteSettings(COLLECTION_NAME).emit("sync", {
    data: {
      current: [
        {
          id: "1",
          last_modified: 100000000000000000001,
          firstPartyOrigin: TEST_DOMAIN,
          thirdPartyOrigin: TEST_3RD_PARTY_DOMAIN,
        },
      ],
    },
  });

  let list = await promise;
  is(
    list,
    `${TEST_DOMAIN},${TEST_3RD_PARTY_DOMAIN}`,
    "exception list is correctly set"
  );

  info("check data");
  await Promise.all([
    checkData(browserFirstParty, {
      firstParty: "firstParty",
      thirdParty: "ExceptionListFirstParty",
    }),
    checkData(browserThirdParty, { firstParty: "ExceptionListFirstParty" }),
  ]);

  info("Removing the tab");
  BrowserTestUtils.removeTab(tabFirstParty);
  BrowserTestUtils.removeTab(tabThirdParty);

  promise = waitForEvent(updateEvent, "update");
  await RemoteSettings(COLLECTION_NAME).emit("sync", {
    data: {
      current: [],
    },
  });
  is(await promise, "", "Exception list is cleared");

  peuService.unregisterExceptionListObserver(obs);
  await cleanup();
});
