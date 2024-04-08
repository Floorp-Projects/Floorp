/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FormHistory } = ChromeUtils.importESModule(
  "resource://gre/modules/FormHistory.sys.mjs"
);

function padLeft(number, length) {
  let str = number + "";
  while (str.length < length) {
    str = "0" + str;
  }
  return str;
}

function getFormExpiryDays() {
  if (Services.prefs.prefHasUserValue("browser.formfill.expire_days")) {
    return Services.prefs.getIntPref("browser.formfill.expire_days");
  }
  return DEFAULT_EXPIRE_DAYS;
}

async function countEntries(name, value) {
  let obj = {};
  if (name) {
    obj.fieldname = name;
  }
  if (value) {
    obj.value = value;
  }

  return await FormHistory.count(obj);
}

const DEFAULT_EXPIRE_DAYS = 180;
let gTimeGroupingSize;
let gNumRecords;
let gNow;

add_setup(async function () {
  gTimeGroupingSize =
    Services.prefs.getIntPref("browser.formfill.timeGroupingSize") *
    1000 *
    1000;

  const maxTimeGroupings = Services.prefs.getIntPref(
    "browser.formfill.maxTimeGroupings"
  );
  const bucketSize = Services.prefs.getIntPref("browser.formfill.bucketSize");

  // ===== Tests with constant timesUsed and varying lastUsed date =====
  // insert 2 records per bucket to check alphabetical sort within
  gNow = 1000 * Date.now();
  gNumRecords = Math.ceil(maxTimeGroupings / bucketSize) * 2;

  let changes = [];
  for (let i = 0; i < gNumRecords; i += 2) {
    let useDate = gNow - (i / 2) * bucketSize * gTimeGroupingSize;

    changes.push({
      op: "add",
      fieldname: "field1",
      value: "value" + padLeft(gNumRecords - 1 - i, 2),
      timesUsed: 1,
      firstUsed: useDate,
      lastUsed: useDate,
    });
    changes.push({
      op: "add",
      fieldname: "field1",
      value: "value" + padLeft(gNumRecords - 2 - i, 2),
      timesUsed: 1,
      firstUsed: useDate,
      lastUsed: useDate,
    });
  }

  await FormHistory.update(changes);

  Assert.ok(
    (await countEntries("field1", null)) > 0,
    "Check initial state is as expected"
  );

  registerCleanupFunction(async () => {
    await FormHistory.update([
      {
        op: "remove",
        firstUsedStart: 0,
      },
    ]);
  });
});

async function focusAndWaitForPopupOpen(browser) {
  await SpecialPowers.spawn(browser, [], async function () {
    const input = content.document.querySelector("input");
    input.focus();
  });

  await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);

  await BrowserTestUtils.waitForCondition(() => {
    return browser.autoCompletePopup.popupOpen;
  });
}

async function unfocusAndWaitForPopupClose(browser) {
  await SpecialPowers.spawn(browser, [], async function () {
    const input = content.document.querySelector("input");
    input.blur();
  });

  await BrowserTestUtils.waitForCondition(() => {
    return !browser.autoCompletePopup.popupOpen;
  });
}

add_task(async function test_search_contains_all_entries() {
  const url = `data:text/html,<input type="text" name="field1">`;
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      const { autoCompletePopup } = browser;

      await focusAndWaitForPopupOpen(browser);

      Assert.equal(
        gNumRecords,
        autoCompletePopup.matchCount,
        "Check search contains all entries"
      );

      info("Check search result ordering with empty search term");
      let lastFound = gNumRecords;
      for (let i = 0; i < gNumRecords; i += 2) {
        Assert.equal(
          parseInt(autoCompletePopup.view.getValueAt(i + 1).substr(5), 10),
          --lastFound
        );
        Assert.equal(
          parseInt(autoCompletePopup.view.getValueAt(i).substr(5), 10),
          --lastFound
        );
      }

      await unfocusAndWaitForPopupClose(browser);
      await SpecialPowers.spawn(browser, [], async function () {
        content.document.querySelector("input").setUserInput("v");
      });

      await focusAndWaitForPopupOpen(browser);

      info('Check search result ordering with "v"');
      lastFound = gNumRecords;
      for (let i = 0; i < gNumRecords; i += 2) {
        Assert.equal(
          parseInt(autoCompletePopup.view.getValueAt(i + 1).substr(5), 10),
          --lastFound
        );
        Assert.equal(
          parseInt(autoCompletePopup.view.getValueAt(i).substr(5), 10),
          --lastFound
        );
      }
    }
  );
});

add_task(async function test_times_used_samples() {
  info("Begin tests with constant use dates and varying timesUsed");

  const timesUsedSamples = 20;

  let changes = [];
  for (let i = 0; i < timesUsedSamples; i++) {
    let timesUsed = timesUsedSamples - i;
    let change = {
      op: "add",
      fieldname: "field2",
      value: "value" + (timesUsedSamples - 1 - i),
      timesUsed: timesUsed * gTimeGroupingSize,
      firstUsed: gNow,
      lastUsed: gNow,
    };
    changes.push(change);
  }
  await FormHistory.update(changes);

  const url = `data:text/html,<input type="text" name="field2">`;
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      await focusAndWaitForPopupOpen(browser);

      info("Check search result ordering with empty search term");
      let lastFound = timesUsedSamples;
      for (let i = 0; i < timesUsedSamples; i++) {
        Assert.equal(
          parseInt(browser.autoCompletePopup.view.getValueAt(i).substr(5), 10),
          --lastFound
        );
      }

      await unfocusAndWaitForPopupClose(browser);
      await SpecialPowers.spawn(browser, [], async function () {
        content.document.querySelector("input").setUserInput("v");
      });
      await focusAndWaitForPopupOpen(browser);

      info('Check search result ordering with "v"');
      lastFound = timesUsedSamples;
      for (let i = 0; i < timesUsedSamples; i++) {
        Assert.equal(
          parseInt(browser.autoCompletePopup.view.getValueAt(i).substr(5), 10),
          --lastFound
        );
      }
    }
  );
});

add_task(async function test_age_bonus() {
  info(
    'Check that "senior citizen" entries get a bonus (browser.formfill.agedBonus)'
  );

  const agedDate =
    1000 * (Date.now() - getFormExpiryDays() * 24 * 60 * 60 * 1000);

  let changes = [];
  changes.push({
    op: "add",
    fieldname: "field3",
    value: "old but not senior",
    timesUsed: 100,
    firstUsed: agedDate + 60 * 1000 * 1000,
    lastUsed: gNow,
  });
  changes.push({
    op: "add",
    fieldname: "field3",
    value: "senior citizen",
    timesUsed: 100,
    firstUsed: agedDate - 60 * 1000 * 1000,
    lastUsed: gNow,
  });
  await FormHistory.update(changes);

  const url = `data:text/html,<input type="text" name="field3">`;
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      const { autoCompletePopup } = browser;

      await focusAndWaitForPopupOpen(browser);

      Assert.equal(autoCompletePopup.view.getValueAt(0), "senior citizen");
      Assert.equal(autoCompletePopup.view.getValueAt(1), "old but not senior");
    }
  );
});

add_task(async function test_search_entry_past_future() {
  info("Check entries that are really old or in the future");

  let changes = [];
  changes.push({
    op: "add",
    fieldname: "field4",
    value: "date of 0",
    timesUsed: 1,
    firstUsed: 0,
    lastUsed: 0,
  });
  changes.push({
    op: "add",
    fieldname: "field4",
    value: "in the future 1",
    timesUsed: 1,
    firstUsed: 0,
    lastUsed: gNow * 2,
  });
  changes.push({
    op: "add",
    fieldname: "field4",
    value: "in the future 2",
    timesUsed: 1,
    firstUsed: gNow * 2,
    lastUsed: gNow * 2,
  });

  await FormHistory.update(changes);

  const url = `data:text/html,<input type="text" name="field4">`;
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      await focusAndWaitForPopupOpen(browser);

      Assert.equal(browser.autoCompletePopup.matchCount, 3);
    }
  );
});

add_task(async function test_old_synchronous_api() {
  info("Check old synchronous api");

  const syncValues = ["sync1", "sync1a", "sync2", "sync3"];
  let changes = [];
  for (const value of syncValues) {
    changes.push({ op: "add", fieldname: "field5", value });
  }
  await FormHistory.update(changes);
});

add_task(async function test_token_limit_db() {
  let changes = [];
  changes.push({
    op: "add",
    fieldname: "field6",
    // value with 15 unique tokens
    value: "a b c d e f g h i j k l m n o",
    timesUsed: 1,
    firstUsed: 0,
    lastUsed: 0,
  });
  changes.push({
    op: "add",
    fieldname: "field6",
    value: "a b c d e f g h i j .",
    timesUsed: 1,
    firstUsed: 0,
    lastUsed: gNow * 2,
  });

  await FormHistory.update(changes);

  const url = `data:text/html,<input type="text" name="field6">`;
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      info(
        "Check that the number of tokens used in a search is capped to MAX_SEARCH_TOKENS for performance when querying the DB"
      );

      await SpecialPowers.spawn(browser, [], async function () {
        const input = content.document.querySelector("input[name=field6]");
        input.setUserInput("a b c d e f g h i j .");
      });

      await focusAndWaitForPopupOpen(browser);

      Assert.equal(
        browser.autoCompletePopup.matchCount,
        2,
        "Only the first MAX_SEARCH_TOKENS tokens should be used for DB queries"
      );

      await unfocusAndWaitForPopupClose(browser);

      info(
        "Check that the number of tokens used in a search is not capped to MAX_SEARCH_TOKENS when using a previousResult"
      );

      await focusAndWaitForPopupOpen(browser);

      Assert.equal(
        browser.autoCompletePopup.matchCount,
        1,
        "All search tokens should be used with previous results"
      );
    }
  );
});

add_task(async function test_can_search_escape_marker() {
  await FormHistory.update({
    op: "add",
    fieldname: "field7",
    value: "/* Further reading */ test",
    timesUsed: 1,
    firstUsed: gNow,
    lastUsed: gNow,
  });

  const url = `data:text/html,<input type="text" name="field7">`;
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      await SpecialPowers.spawn(browser, [], async function () {
        const input = content.document.querySelector("input");
        input.setUserInput("/* Further reading */ t");
      });

      await focusAndWaitForPopupOpen(browser);

      Assert.equal(browser.autoCompletePopup.matchCount, 1);
    }
  );
});
