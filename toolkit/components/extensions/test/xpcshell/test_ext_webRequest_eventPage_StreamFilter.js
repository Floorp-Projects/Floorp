/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

AddonTestUtils.init(this);
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

const server = createHttpServer({ hosts: ["example.com"] });

let clearLastPendingRequest;

server.registerPathHandler("/pending_request", (request, response) => {
  response.processAsync();
  response.setHeader("Content-Length", "10000", false);
  response.write("somedata\n");
  let intervalID = setInterval(() => response.write("continue\n"), 50);

  const clearPendingRequest = () => {
    try {
      clearInterval(intervalID);
      response.finish();
    } catch (e) {
      // This will throw, but we don't care at this point.
    }
  };

  clearLastPendingRequest = clearPendingRequest;
  registerCleanupFunction(clearPendingRequest);
});

server.registerPathHandler("/completed_request", (request, response) => {
  response.write("somedata\n");
});

add_setup(async () => {
  await AddonTestUtils.promiseStartupManager();
});

async function test_idletimeout_on_streamfilter({
  manifest_version,
  expectResetIdle,
  expectStreamFilterStop,
  requestUrlPath,
}) {
  const extension = ExtensionTestUtils.loadExtension({
    background: `(${async function(urlPath) {
      browser.webRequest.onBeforeRequest.addListener(
        request => {
          browser.test.log(`webRequest request intercepted: ${request.url}`);
          const filter = browser.webRequest.filterResponseData(
            request.requestId
          );
          const decoder = new TextDecoder("utf-8");
          const encoder = new TextEncoder();
          filter.onstart = () => {
            browser.test.sendMessage("streamfilter:started");
          };
          filter.ondata = event => {
            let str = decoder.decode(event.data, { stream: true });
            filter.write(encoder.encode(str));
          };
          filter.onstop = () => {
            filter.close();
            browser.test.sendMessage("streamfilter:stopped");
          };
        },
        {
          urls: [`http://example.com/${urlPath}`],
        },
        ["blocking"]
      );
      browser.test.sendMessage("bg:ready");
    }})("${requestUrlPath}")`,

    useAddonManager: "temporary",
    manifest: {
      manifest_version,
      background: manifest_version >= 3 ? {} : { persistent: false },
      granted_host_permissions: manifest_version >= 3,
      permissions: ["webRequest", "webRequestBlocking"],
      // host_permissions are merged with permissions on a MV2 test extension.
      host_permissions: ["http://example.com/*"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg:ready");
  const { contextId } = extension.extension.backgroundContext;
  notEqual(contextId, undefined, "Got a contextId for the background context");

  info("Trigger a webRequest");
  const testURL = `http://example.com/${requestUrlPath}`;
  const promiseRequestCompleted = ExtensionTestUtils.fetch(
    "http://example.com/",
    testURL
  ).catch(err => {
    // This request is expected to be aborted when cleared after the test is exiting,
    // otherwise rethrow the error to trigger an explicit failure.
    if (/The operation was aborted/.test(err.message)) {
      info(`Test webRequest fetching "${testURL}" aborted`);
    } else {
      ok(
        false,
        `Unexpected rejection triggered by the test webRequest fetching "${testURL}": ${err.message}`
      );
      throw err;
    }
  });

  info("Wait for the stream filter to be started");
  await extension.awaitMessage("streamfilter:started");

  if (expectStreamFilterStop) {
    await extension.awaitMessage("streamfilter:stopped");
  }

  info("Terminate the background script (simulated idle timeout)");

  if (expectResetIdle) {
    const promiseResetIdle = promiseExtensionEvent(
      extension,
      "background-script-reset-idle"
    );

    clearHistograms();
    assertHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT);
    assertKeyedHistogramEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID);

    await extension.terminateBackground();
    info("Wait for 'background-script-reset-idle' event to be emitted");
    await promiseResetIdle;
    equal(
      extension.extension.backgroundContext.contextId,
      contextId,
      "Initial background context is still available as expected"
    );

    assertHistogramCategoryNotEmpty(WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT, {
      category: "reset_streamfilter",
      categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
    });

    assertHistogramCategoryNotEmpty(
      WEBEXT_EVENTPAGE_IDLE_RESULT_COUNT_BY_ADDONID,
      {
        keyed: true,
        key: extension.id,
        category: "reset_streamfilter",
        categories: HISTOGRAM_EVENTPAGE_IDLE_RESULT_CATEGORIES,
      }
    );
  } else {
    const { Management } = ChromeUtils.import(
      "resource://gre/modules/Extension.jsm"
    );
    const promiseProxyContextUnloaded = new Promise(resolve => {
      function listener(evt, context) {
        if (context.extension.id === extension.id) {
          Management.off("proxy-context-unload", listener);
          resolve();
        }
      }
      Management.on("proxy-context-unload", listener);
    });
    await extension.terminateBackground();
    await promiseProxyContextUnloaded;
    equal(
      extension.extension.backgroundContext,
      undefined,
      "Initial background context should have been terminated as expected"
    );
  }

  await extension.unload();
  clearLastPendingRequest();
  await promiseRequestCompleted;
}

add_task(
  {
    pref_set: [["extensions.eventPages.enabled", true]],
  },
  async function test_idletimeout_on_active_streamfilter_mv2_eventpage() {
    await test_idletimeout_on_streamfilter({
      manifest_version: 2,
      requestUrlPath: "pending_request",
      expectStreamFilterStop: false,
      expectResetIdle: true,
    });
  }
);

add_task(
  {
    pref_set: [["extensions.manifestV3.enabled", true]],
  },
  async function test_idletimeout_on_active_streamfilter_mv3() {
    await test_idletimeout_on_streamfilter({
      manifest_version: 3,
      requestUrlPath: "pending_request",
      expectStreamFilterStop: false,
      expectResetIdle: true,
    });
  }
);

add_task(
  {
    pref_set: [["extensions.eventPages.enabled", true]],
  },
  async function test_idletimeout_on_inactive_streamfilter_mv2_eventpage() {
    await test_idletimeout_on_streamfilter({
      manifest_version: 2,
      requestUrlPath: "completed_request",
      expectStreamFilterStop: true,
      expectResetIdle: false,
    });
  }
);

add_task(
  {
    pref_set: [["extensions.manifestV3.enabled", true]],
  },
  async function test_idletimeout_on_inactive_streamfilter_mv3() {
    await test_idletimeout_on_streamfilter({
      manifest_version: 3,
      requestUrlPath: "completed_request",
      expectStreamFilterStop: true,
      expectResetIdle: false,
    });
  }
);

async function test_create_new_streamfilter_while_suspending({
  manifest_version,
}) {
  const extension = ExtensionTestUtils.loadExtension({
    async background() {
      let interceptedRequestId;
      let resolvePendingWebRequest;

      browser.runtime.onSuspend.addListener(async () => {
        await browser.test.assertThrows(
          () => browser.webRequest.filterResponseData(interceptedRequestId),
          /forbidden while background extension global is suspending/,
          "Got the expected exception raised from filterResponseData calls while suspending"
        );
        browser.test.sendMessage("suspend-listener");
      });

      browser.runtime.onSuspendCanceled.addListener(async () => {
        // Once onSuspendCanceled is emitted, filterResponseData
        // is expected to don't throw.
        const filter = browser.webRequest.filterResponseData(
          interceptedRequestId
        );
        resolvePendingWebRequest();
        filter.onstop = () => {
          filter.disconnect();
          browser.test.sendMessage("suspend-canceled-listener");
        };
      });

      browser.webRequest.onBeforeRequest.addListener(
        request => {
          browser.test.log(`webRequest request intercepted: ${request.url}`);
          interceptedRequestId = request.requestId;
          return new Promise(resolve => {
            resolvePendingWebRequest = resolve;
            browser.test.sendMessage("webrequest-listener:done");
          });
        },
        {
          urls: [`http://example.com/completed_request`],
        },
        ["blocking"]
      );
      browser.test.sendMessage("bg:ready");
    },

    useAddonManager: "temporary",
    manifest: {
      manifest_version,
      background: manifest_version >= 3 ? {} : { persistent: false },
      granted_host_permissions: manifest_version >= 3,
      permissions: ["webRequest", "webRequestBlocking"],
      // host_permissions are merged with permissions on a MV2 test extension.
      host_permissions: ["http://example.com/*"],
    },
  });

  await extension.startup();
  await extension.awaitMessage("bg:ready");
  const { contextId } = extension.extension.backgroundContext;
  notEqual(contextId, undefined, "Got a contextId for the background context");

  info("Trigger a webRequest");
  ExtensionTestUtils.fetch(
    "http://example.com/",
    `http://example.com/completed_request`
  );

  info("Wait for the web request to be intercepted and suspended");
  await extension.awaitMessage("webrequest-listener:done");

  info("Terminate the background script (simulated idle timeout)");

  extension.terminateBackground();
  await extension.awaitMessage("suspend-listener");

  info("Simulated idle timeout canceled");
  extension.extension.emit("background-script-reset-idle");
  await extension.awaitMessage("suspend-canceled-listener");

  await extension.unload();
}

add_task(
  {
    pref_set: [["extensions.eventPages.enabled", true]],
  },
  async function test_error_creating_new_streamfilter_while_suspending_mv2_eventpage() {
    await test_create_new_streamfilter_while_suspending({
      manifest_version: 2,
    });
  }
);

add_task(
  {
    pref_set: [["extensions.manifestV3.enabled", true]],
  },
  async function test_error_creating_new_streamfilter_while_suspending_mv3() {
    await test_create_new_streamfilter_while_suspending({
      manifest_version: 3,
    });
  }
);
