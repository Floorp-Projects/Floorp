/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

"use strict";

const { ContentTaskUtils } = Cu.import("resource://testing-common/ContentTaskUtils.jsm", {});
const { TelemetryUtils } = Cu.import("resource://gre/modules/TelemetryUtils.jsm", {});
const { ObjectUtils } = Cu.import("resource://gre/modules/ObjectUtils.jsm", {});

const HC_PERMISSION = "hc_telemetry";

async function waitForProcessesEvents(aProcesses,
                                      aAdditionalCondition = data => true) {
  await ContentTaskUtils.waitForCondition(() => {
    const events =
      Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);
    return aProcesses.every(p => Object.keys(events).includes(p))
           && aAdditionalCondition(events);
  });
}

/**
 * Wait for a specific event to appear in the given process data.
 * @param {String} aProcess the name of the process we expect the event to appear.
 * @param {Array} aEventData the event data to look for.
 * @return {Promise} Resolved when the event is found or rejected if the search
 *         times out.
 */
async function waitForEvent(aProcess, aEventData) {
  await waitForProcessesEvents([aProcess], events => {
    let contentEvents = events.content.map(e => e.slice(1));
    if (contentEvents.length == 0) {
      return false;
    }

    return contentEvents.find(e => ObjectUtils.deepEqual(e, aEventData));
  });
}

/**
 * Remove the trailing null/undefined from an event definition.
 * This is useful for comparing the sample events (that might
 * contain null/undefined) to the data from the snapshot (which might
 * filter them).
 */
function removeTrailingInvalidEntry(aEvent) {
  while (aEvent[aEvent.length - 1] === undefined ||
         aEvent[aEvent.length - 1] === null) {
    aEvent.pop();
  }
  return aEvent;
}

add_task(async function test_setup() {
  // Make sure the newly spawned content processes will have extended Telemetry and
  // hybrid content telemetry enabled.
  await SpecialPowers.pushPrefEnv({
    set: [
      [TelemetryUtils.Preferences.OverridePreRelease, true],
      [TelemetryUtils.Preferences.HybridContentEnabled, true],
      [TelemetryUtils.Preferences.LogLevel, "Trace"]
    ]
  });
  // And take care of the already initialized one as well.
  let canRecordExtended = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(() => Services.telemetry.canRecordExtended = canRecordExtended);
});

add_task(async function test_untrusted_http_origin() {
  Services.telemetry.clearEvents();

  // Install a custom handler that intercepts hybrid content telemetry messages
  // and makes the test fail. We don't expect any message from non secure contexts.
  const messageName = "HybridContentTelemetry:onTelemetryMessage";
  let makeTestFail = () => ok(false, `Received an unexpected ${messageName}.`);
  Services.mm.addMessageListener(messageName, makeTestFail);

  // Try to use the API on a non-secure host.
  const testHost = "http://example.org";
  let testHttpUri = Services.io.newURI(testHost);
  Services.perms.add(testHttpUri, HC_PERMISSION, Services.perms.ALLOW_ACTION);
  let url = getRootDirectory(gTestPath) + "hybrid_content.html";
  url = url.replace("chrome://mochitests/content", testHost);
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  // Try to use the API. Also record a content event from outside HCT: we'll
  // use this event to know when we can stop waiting for the hybrid content data.
  const TEST_CONTENT_EVENT = ["telemetry.test", "content_only", "object1"];
  Services.telemetry.setEventRecordingEnabled("telemetry.test", true);
  await ContentTask.spawn(newTab.linkedBrowser, [TEST_CONTENT_EVENT],
                          ([testContentEvent]) => {
    // Call the hybrid content telemetry API.
    let contentWin = Components.utils.waiveXrays(content);
    contentWin.testRegisterEvents(testContentEvent[0], JSON.stringify({}));
    // Record from the usual telemetry API a "canary" event.
    Services.telemetry.recordEvent(...testContentEvent);
  });

  await waitForEvent("content", TEST_CONTENT_EVENT);

  // This is needed otherwise the test will fail due to missing test passes.
  ok(true, "The untrusted HTTP page was not able to use the API.");

  // Finally clean up the listener.
  await BrowserTestUtils.removeTab(newTab);
  Services.perms.remove(testHttpUri, HC_PERMISSION);
  Services.mm.removeMessageListener(messageName, makeTestFail);
  Services.telemetry.setEventRecordingEnabled("telemetry.test", false);
});

add_task(async function test_secure_non_whitelisted_origin() {
  Services.telemetry.clearEvents();

  // Install a custom handler that intercepts hybrid content telemetry messages
  // and makes the test fail. We don't expect any message from non whitelisted pages.
  const messageName = "HybridContentTelemetry:onTelemetryMessage";
  let makeTestFail = () => ok(false, `Received an unexpected ${messageName}.`);
  Services.mm.addMessageListener(messageName, makeTestFail);

  // Try to use the API on a secure host but don't give the page enough privileges.
  const testHost = "https://example.org";
  let url = getRootDirectory(gTestPath) + "hybrid_content.html";
  url = url.replace("chrome://mochitests/content", testHost);
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  // Try to use the API. Also record a content event from outside HCT: we'll
  // use this event to know when we can stop waiting for the hybrid content data.
  const TEST_CONTENT_EVENT = ["telemetry.test", "content_only", "object1"];
  Services.telemetry.setEventRecordingEnabled("telemetry.test", true);
  await ContentTask.spawn(newTab.linkedBrowser, [TEST_CONTENT_EVENT],
                          ([testContentEvent]) => {
    // Call the hybrid content telemetry API.
    let contentWin = Components.utils.waiveXrays(content);
    contentWin.testRegisterEvents(testContentEvent[0], JSON.stringify({}));
    // Record from the usual telemetry API a "canary" event.
    Services.telemetry.recordEvent(...testContentEvent);
  });

  await waitForEvent("content", TEST_CONTENT_EVENT);

  // This is needed otherwise the test will fail due to missing test passes.
  ok(true, "The HTTPS page without permission was not able to use the API.");

  // Finally clean up the listener.
  await BrowserTestUtils.removeTab(newTab);
  Services.mm.removeMessageListener(messageName, makeTestFail);
  Services.telemetry.setEventRecordingEnabled("telemetry.test", false);
});

add_task(async function test_trusted_disabled_hybrid_telemetry() {
  Services.telemetry.clearEvents();

  // This test requires hybrid content telemetry to be disabled.
  await SpecialPowers.pushPrefEnv({
    set: [[TelemetryUtils.Preferences.HybridContentEnabled, false]]
  });

  // Install a custom handler that intercepts hybrid content telemetry messages
  // and makes the test fail. We don't expect any message when the API is disabled.
  const messageName = "HybridContentTelemetry:onTelemetryMessage";
  let makeTestFail = () => ok(false, `Received an unexpected ${messageName}.`);
  Services.mm.addMessageListener(messageName, makeTestFail);

  // Try to use the API on a secure host.
  const testHost = "https://example.org";
  let testHttpsUri = Services.io.newURI(testHost);
  Services.perms.add(testHttpsUri, HC_PERMISSION, Services.perms.ALLOW_ACTION);
  let url = getRootDirectory(gTestPath) + "hybrid_content.html";
  url = url.replace("chrome://mochitests/content", testHost);
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  // Try to use the API. Also record a content event from outside HCT: we'll
  // use this event to know when we can stop waiting for the hybrid content data.
  const TEST_CONTENT_EVENT = ["telemetry.test", "content_only", "object1"];
  Services.telemetry.setEventRecordingEnabled("telemetry.test", true);
  await ContentTask.spawn(newTab.linkedBrowser, [TEST_CONTENT_EVENT],
                          ([testContentEvent]) => {
    // Call the hybrid content telemetry API.
    let contentWin = Components.utils.waiveXrays(content);
    contentWin.testRegisterEvents(testContentEvent[0], JSON.stringify({}));
    // Record from the usual telemetry API a "canary" event.
    Services.telemetry.recordEvent(...testContentEvent);
  });

  await waitForEvent("content", TEST_CONTENT_EVENT);

  // This is needed otherwise the test will fail due to missing test passes.
  ok(true, "There were no unintended hybrid content API usages.");

  // Finally clean up the listener.
  await SpecialPowers.popPrefEnv();
  await BrowserTestUtils.removeTab(newTab);
  Services.perms.remove(testHttpsUri, HC_PERMISSION);
  Services.mm.removeMessageListener(messageName, makeTestFail);
  Services.telemetry.setEventRecordingEnabled("telemetry.test", false);
});

add_task(async function test_hybrid_content_with_iframe() {
  Services.telemetry.clearEvents();

  // Open a trusted page that can use in the HCT in a new tab.
  const testOuterPageHost = "https://example.com";
  let testHttpsUri = Services.io.newURI(testOuterPageHost);
  Services.perms.add(testHttpsUri, HC_PERMISSION, Services.perms.ALLOW_ACTION);
  let url = getRootDirectory(gTestPath) + "hybrid_content.html";
  let outerUrl = url.replace("chrome://mochitests/content", testOuterPageHost);
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, outerUrl);

  // Install a custom handler that intercepts hybrid content telemetry messages
  // and makes the test fail. This needs to be done after the tab is opened.
  const messageName = "HybridContentTelemetry:onTelemetryMessage";
  let makeTestFail = () => ok(false, `Received an unexpected ${messageName}.`);
  Services.mm.addMessageListener(messageName, makeTestFail);

  // Enable recording the canary event.
  const TEST_CONTENT_EVENT = ["telemetry.test", "content_only", "object1"];
  Services.telemetry.setEventRecordingEnabled("telemetry.test", true);

  // Add an iframe to the test page. The URI in the iframe should not be able
  // to use HCT to the the missing privileges.
  const testHost = "https://example.org";
  let iframeUrl = url.replace("chrome://mochitests/content", testHost);
  await ContentTask.spawn(newTab.linkedBrowser,
                          [iframeUrl, TEST_CONTENT_EVENT],
                          async function([iframeUrl, testContentEvent]) {
    let doc = content.document;
    let iframe = doc.createElement("iframe");
    let promiseIframeLoaded = ContentTaskUtils.waitForEvent(iframe, "load", false);
    iframe.src = iframeUrl;
    doc.body.insertBefore(iframe, doc.body.firstChild);
    await promiseIframeLoaded;

    // Call the hybrid content telemetry API.
    let contentWin = Components.utils.waiveXrays(iframe.contentWindow);
    contentWin.testRegisterEvents(testContentEvent[0], JSON.stringify({}));

    // Record from the usual telemetry API a "canary" event.
    Services.telemetry.recordEvent(...testContentEvent);
  });

  await waitForEvent("content", TEST_CONTENT_EVENT);

  // This is needed otherwise the test will fail due to missing test passes.
  ok(true, "There were no unintended hybrid content API usages from the iframe.");

  // Cleanup permissions and remove the tab.
  await BrowserTestUtils.removeTab(newTab);
  Services.mm.removeMessageListener(messageName, makeTestFail);
  Services.perms.remove(testHttpsUri, HC_PERMISSION);
  Services.telemetry.setEventRecordingEnabled("telemetry.test", false);
});

add_task(async function test_hybrid_content_recording() {
  const testHost = "https://example.org";
  const TEST_EVENT_CATEGORY = "telemetry.test.hct";
  const RECORDED_TEST_EVENTS = [
    [TEST_EVENT_CATEGORY, "test1", "object1"],
    [TEST_EVENT_CATEGORY, "test2", "object1", null, {"key1": "foo", "key2": "bar"}],
    [TEST_EVENT_CATEGORY, "test2", "object1", "some value"],
    [TEST_EVENT_CATEGORY, "test1", "object1", null, null],
    [TEST_EVENT_CATEGORY, "test1", "object1", "", null],
  ];
  const NON_RECORDED_TEST_EVENTS = [
    [TEST_EVENT_CATEGORY, "unknown", "unknown"],
  ];

  Services.telemetry.clearEvents();

  // Give the test host enough privileges to use the API and open the test page.
  let testHttpsUri = Services.io.newURI(testHost);
  Services.perms.add(testHttpsUri, HC_PERMISSION, Services.perms.ALLOW_ACTION);
  let url = getRootDirectory(gTestPath) + "hybrid_content.html";
  url = url.replace("chrome://mochitests/content", testHost);
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  // Register some events and record them in Telemetry.
  await ContentTask.spawn(newTab.linkedBrowser,
                          [TEST_EVENT_CATEGORY, RECORDED_TEST_EVENTS, NON_RECORDED_TEST_EVENTS],
                          ([eventCategory, recordedTestEvents, nonRecordedTestEvents]) => {
    let contentWin = Components.utils.waiveXrays(content);

    // If we tried to call contentWin.Mozilla.ContentTelemetry.* functions
    // and pass non-string parameters, |waiveXrays| would complain and not
    // let us access them. To work around this, we generate test functions
    // in the test HTML file and unwrap the passed JSON blob there.
    contentWin.testRegisterEvents(eventCategory, JSON.stringify({
      // Event with only required fields.
      "test1": {
        methods: ["test1"],
        objects: ["object1"],
      },
      // Event with extra_keys.
      "test2": {
        methods: ["test2", "test2b"],
        objects: ["object1"],
        extra_keys: ["key1", "key2"],
      },
    }));

    // Record some valid events.
    recordedTestEvents.forEach(e => contentWin.testRecordEvents(JSON.stringify(e)));

    // Test recording an unknown event. The Telemetry API itself is supposed to throw,
    // but we catch that in hybrid content telemetry and log an error message.
    nonRecordedTestEvents.forEach(e => contentWin.testRecordEvents(JSON.stringify(e)));
  });

  // Wait for the data to be in the snapshot, then get the Telemetry data.
  await waitForProcessesEvents(["dynamic"]);
  let snapshot =
      Services.telemetry.snapshotEvents(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN);

  // Check that the dynamically register events made it to the snapshot.
  ok("dynamic" in snapshot,
     "The snapshot must contain the 'dynamic' process section");
  let dynamicEvents = snapshot.dynamic.map(e => e.slice(1));
  is(dynamicEvents.length, RECORDED_TEST_EVENTS.length, "Should match expected event count.");
  for (let i = 0; i < RECORDED_TEST_EVENTS.length; ++i) {
    SimpleTest.isDeeply(dynamicEvents[i],
                        removeTrailingInvalidEntry(RECORDED_TEST_EVENTS[i]),
                        "Should have recorded the expected event.");
  }

  // Cleanup permissions and remove the tab.
  await BrowserTestUtils.removeTab(newTab);
  Services.perms.remove(testHttpsUri, HC_PERMISSION);
});

add_task(async function test_can_upload() {
  const testHost = "https://example.org";

  await SpecialPowers.pushPrefEnv({set: [[TelemetryUtils.Preferences.FhrUploadEnabled, false]]});

  // Give the test host enough privileges to use the API and open the test page.
  let testHttpsUri = Services.io.newURI(testHost);
  Services.perms.add(testHttpsUri, HC_PERMISSION, Services.perms.ALLOW_ACTION);
  let url = getRootDirectory(gTestPath) + "hybrid_content.html";
  url = url.replace("chrome://mochitests/content", testHost);
  let newTab = await BrowserTestUtils.openNewForegroundTab(gBrowser, url);

  // Check that CanUpload reports the correct value.
  await ContentTask.spawn(newTab.linkedBrowser, {}, () => {
    let contentWin = Components.utils.waiveXrays(content);
    // We don't need to pass any parameter, we can safely call Mozilla.ContentTelemetry.
    let canUpload = contentWin.Mozilla.ContentTelemetry.canUpload();
    ok(!canUpload, "CanUpload must report 'false' if the preference has that value.");
  });

  // Flip the pref and check again.
  await SpecialPowers.pushPrefEnv({set: [[TelemetryUtils.Preferences.FhrUploadEnabled, true]]});
  await ContentTask.spawn(newTab.linkedBrowser, {}, () => {
    let contentWin = Components.utils.waiveXrays(content);
    let canUpload = contentWin.Mozilla.ContentTelemetry.canUpload();
    ok(canUpload, "CanUpload must report 'true' if the preference has that value.");
  });

  // Cleanup permissions and remove the tab.
  await BrowserTestUtils.removeTab(newTab);
  Services.perms.remove(testHttpsUri, HC_PERMISSION);
});
