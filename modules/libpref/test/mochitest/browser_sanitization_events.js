"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
);
const { BrowserTestUtils } = ChromeUtils.import(
  "resource://testing-common/BrowserTestUtils.jsm"
);

// eslint-disable-next-line
const ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const PAGE_URL = ROOT + "file_access_sanitized_pref.html";

async function waitForEventCount(
  count,
  process = "content",
  category = "security",
  method = "prefUsage"
) {
  await new Promise(resolve => setTimeout(resolve, 100));
  let returned_events = await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      false
    )[process];

    if (!events) {
      return null;
    }

    events = events.filter(e => e[1] == category && e[2] == method);
    dump(`Waiting for ${count} events, got ${events.length}\n`);
    return events.length >= count ? events : null;
  }, "waiting for telemetry event count of: " + count);
  return returned_events;
}

add_task(async function sanitized_pref_test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["fission.omitBlocklistedPrefsInSubprocesses", true],
      ["fission.enforceBlocklistedPrefsInSubprocesses", false],
    ],
  });

  Services.telemetry.setEventRecordingEnabled("security", true);
  Services.telemetry.clearEvents();

  TelemetryTestUtils.assertNumberOfEvents(0, { process: "content" });

  await BrowserTestUtils.withNewTab({ gBrowser, url: PAGE_URL }, async function(
    browser
  ) {});

  // Needed because otherwise we advance too quickly
  await waitForEventCount(1);

  let events = TelemetryTestUtils.getEvents(
    { category: "security", method: "prefUsage", object: "contentProcess" },
    { process: "content" }
  );

  let count = 0,
    foundIt = false;
  for (let i = 0; i < events.length; i++) {
    if (events[i].value == "security.sandbox.content.tempDirSuffix") {
      foundIt = true;
      count++;
    }
  }

  // We may have more than one event entries because we take two paths in the preference code
  // in this access pattern, but in other patterns we may only take one of those
  // paths, so we happen to count it twice this way.  No big deal. Sometimes we even have 4
  // or 6 based on timing
  dump(
    `We found ${events.length} events, ${count} of which were the tempDirSuffix ones.`
  );

  // eslint-disable-next-line
  Assert.ok(
    foundIt,
    "We did not find an event for security.sandbox.content.tempDirSuffix"
  );

  await SpecialPowers.popPrefEnv();
});
