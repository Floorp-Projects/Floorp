/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from head.js */

"use strict";

const TEST_REDIRECT_TOP_PAGE =
  TEST_3RD_PARTY_DOMAIN + TEST_PATH + "redirect.sjs?" + TEST_TOP_PAGE;
const TEST_REDIRECT_3RD_PARTY_PAGE =
  TEST_DOMAIN + TEST_PATH + "redirect.sjs?" + TEST_3RD_PARTY_PAGE;

async function cleanup() {
  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value =>
      resolve()
    );
  });
}

add_task(async () => {
  await SpecialPowers.flushPrefEnv();
  await SpecialPowers.pushPrefEnv({
    set: [
      [
        "network.cookie.cookieBehavior",
        Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
      ],
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
        let runnableStr = `(() => {return (${obj.callback});})();`;
        let runnable = eval(runnableStr); // eslint-disable-line no-eval

        if (obj.page) {
          // third-party
          let ifr = content.document.createElement("iframe");
          ifr.onload = async () => {
            resolve(await runnable.call(content, ifr.contentWindow, obj.value));
          };
          content.document.body.appendChild(ifr);
          ifr.src = obj.page;
        } else {
          // first-party
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
    page: TEST_3RD_PARTY_PAGE,
    value,
  });
}
function getDataFromThirdParty(browser) {
  return executeContentScript(browser, readNetworkCookie, {
    page: TEST_3RD_PARTY_PAGE,
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
      "currect first-party data"
    );
  }
  if ("thirdParty" in options) {
    is(
      await getDataFromThirdParty(browser),
      options.thirdParty,
      "currect third-party data"
    );
  }
}

add_task(async () => {
  info("Starting Dynamic FPI Redirect Heuristic test...");

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
    thirdParty: "thirdParty",
  });

  info("load third-party content as first-party");
  await redirectWithUserInteraction(
    browser,
    TEST_REDIRECT_3RD_PARTY_PAGE,
    TEST_3RD_PARTY_PAGE
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

  info("third-party page should able to access first-party data");
  await checkData(browser, {
    firstParty: "firstParty",
    thirdParty: "heuristicFirstParty",
  });

  info("Removing the tab");
  BrowserTestUtils.removeTab(tab);
});
