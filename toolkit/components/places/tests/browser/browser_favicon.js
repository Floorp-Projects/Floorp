/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  PlacesTestUtils: "resource://testing-common/PlacesTestUtils.sys.mjs",
});

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

add_task(async function test_userpass() {
  // Setup the prompt to avoid showing it.
  let mockPromptService = {
    firstTimeCalled: false,
    confirmExBC() {
      if (!this.firstTimeCalled) {
        this.firstTimeCalled = true;
        return 0;
      }

      return 1;
    },
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
  };
  let mockPromptServiceCID = MockRegistrar.register(
    "@mozilla.org/prompter;1",
    mockPromptService
  );
  registerCleanupFunction(() => {
    MockRegistrar.unregister(mockPromptServiceCID);
  });

  const pageUrl =
    "https://user:pass@example.org/tests/toolkit/components/places/tests/browser/favicon.html";
  const faviconUrl =
    "https://user:pass@example.org/tests/toolkit/components/places/tests/browser/favicon-normal32.png";
  const exposableFaviconUrl =
    "https://example.org/tests/toolkit/components/places/tests/browser/favicon-normal32.png";

  let faviconPromise = lazy.PlacesTestUtils.waitForNotification(
    "favicon-changed",
    async () => {
      let faviconForExposable = await lazy.PlacesTestUtils.getDatabaseValue(
        "moz_icons",
        "icon_url",
        {
          icon_url: exposableFaviconUrl,
        }
      );
      Assert.ok(faviconForExposable, "Found the icon for exposable URL");

      let faviconForOriginal = await lazy.PlacesTestUtils.getDatabaseValue(
        "moz_icons",
        "icon_url",
        {
          icon_url: faviconUrl,
        }
      );
      Assert.ok(!faviconForOriginal, "Not found the icon for the original URL");
      return true;
    }
  );

  await BrowserTestUtils.openNewForegroundTab(gBrowser, pageUrl);
  await faviconPromise;

  // Clean up.
  await PlacesUtils.history.clear();
  gBrowser.removeCurrentTab();
});
