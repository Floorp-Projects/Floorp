/* eslint max-len: ["error", 80] */
"use strict";

const { AddonTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/AddonTestUtils.sys.mjs"
);

AddonTestUtils.initMochitest(this);
const server = AddonTestUtils.createHttpServer();
const TEST_API_URL = `http://localhost:${server.identity.primaryPort}/discoapi`;

const EXT_ID_EXTENSION = "extension@example.com";
const EXT_ID_THEME = "theme@example.com";

let requestCount = 0;
server.registerPathHandler("/discoapi", (request, response) => {
  // This test is expected to load the results only once, and then cache the
  // results.
  is(++requestCount, 1, "Expect only one discoapi request");

  let results = {
    results: [
      {
        addon: {
          authors: [{ name: "Some author" }],
          current_version: {
            files: [{ platform: "all", url: "data:," }],
          },
          url: "data:,",
          guid: "recommendation@example.com",
          type: "extension",
        },
      },
    ],
  };
  response.write(JSON.stringify(results));
});

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.getAddons.discovery.api_url", TEST_API_URL]],
  });

  let mockProvider = new MockProvider();
  mockProvider.createAddons([
    {
      id: EXT_ID_EXTENSION,
      name: "Mock 1",
      type: "extension",
      userPermissions: {
        origins: ["<all_urls>"],
        permissions: ["tabs"],
      },
    },
    {
      id: EXT_ID_THEME,
      name: "Mock 2",
      type: "theme",
    },
  ]);
});

async function switchToView(win, type, param = "") {
  let loaded = waitForViewLoad(win);
  win.gViewController.loadView(`addons://${type}/${param}`);
  await loaded;
  await waitForStableLayout(win);
}

// delta = -1 = go back.
// delta = +1 = go forwards.
async function historyGo(win, delta, expectedViewType) {
  let loaded = waitForViewLoad(win);
  win.history.go(delta);
  await loaded;
  is(
    win.gViewController.currentViewId,
    expectedViewType,
    "Expected view after history navigation"
  );
  await waitForStableLayout(win);
}

async function waitForStableLayout(win) {
  // In the test, it is important that the layout is fully stable before we
  // consider the view loaded, because those affect the offset calculations.
  await TestUtils.waitForCondition(
    () => isLayoutStable(win),
    "Waiting for layout to stabilize"
  );
}

function isLayoutStable(win) {
  // <message-bar> elements may affect the layout of a page, and therefore we
  // should check whether its embedded style sheet has finished loading.
  for (let bar of win.document.querySelectorAll("message-bar")) {
    // Check for the existence of a CSS property from message-bar.css.
    if (!win.getComputedStyle(bar).getPropertyValue("--message-bar-icon-url")) {
      return false;
    }
  }
  return true;
}

function getScrollOffset(win) {
  let { scrollTop: top, scrollLeft: left } = win.document.documentElement;
  return { top, left };
}

// Scroll an element into view. The purpose of this is to simulate a real-world
// scenario where the user has moved part of the UI is in the viewport.
function scrollTopLeftIntoView(elem) {
  elem.scrollIntoView({ block: "start", inline: "start" });
  // Sanity check: In this test, a large padding has been added to the top and
  // left of the document. So when an element has been scrolled into view, the
  // top and left offsets must be non-zero.
  assertNonZeroScrollOffsets(getScrollOffset(elem.ownerGlobal));
}

function assertNonZeroScrollOffsets(offsets) {
  ok(offsets.left, "Should have scrolled to the right");
  ok(offsets.top, "Should have scrolled down");
}

function checkScrollOffset(win, expected, msg = "") {
  let actual = getScrollOffset(win);
  let fuzz = AppConstants.platform == "macosx" ? 3 : 1;
  isfuzzy(actual.top, expected.top, fuzz, `Top scroll offset - ${msg}`);
  isfuzzy(actual.left, expected.left, fuzz, `Left scroll offset - ${msg}`);
}

add_task(async function test_scroll_restoration() {
  let win = await loadInitialView("discover");

  // Wait until the recommendations have been loaded. These are cached after
  // the first load, so we only need to wait once, at the start of the test.
  await win.document.querySelector("recommended-addon-list").cardsReady;

  // Force scrollbar to appear, by adding enough space around the content.
  win.document.body.style.paddingBlock = "100vh";
  win.document.body.style.paddingInline = "100vw";
  win.document.body.style.width = "300vw";

  checkScrollOffset(win, { top: 0, left: 0 }, "initial page load");

  scrollTopLeftIntoView(win.document.querySelector("recommended-addon-card"));
  let discoOffsets = getScrollOffset(win);
  assertNonZeroScrollOffsets(discoOffsets);

  // Switch from disco pane to extension list

  await switchToView(win, "list", "extension");
  checkScrollOffset(win, { top: 0, left: 0 }, "initial extension list");

  scrollTopLeftIntoView(getAddonCard(win, EXT_ID_EXTENSION));
  let extListOffsets = getScrollOffset(win);
  assertNonZeroScrollOffsets(extListOffsets);

  // Switch from extension list to details view.

  let loaded = waitForViewLoad(win);
  const addonCard = getAddonCard(win, EXT_ID_EXTENSION);
  // Ensure that we send a click on the control that is accessible (while a
  // mouse user could also activate a card by clicking on the entire container):
  const addonCardLink = addonCard.querySelector(".addon-name-link");
  addonCardLink.click();
  await loaded;

  checkScrollOffset(win, { top: 0, left: 0 }, "initial details view");
  scrollTopLeftIntoView(getAddonCard(win, EXT_ID_EXTENSION));
  let detailsOffsets = getScrollOffset(win);
  assertNonZeroScrollOffsets(detailsOffsets);

  // Switch from details view back to extension list.

  await historyGo(win, -1, "addons://list/extension");
  checkScrollOffset(win, extListOffsets, "back to extension list");

  // Now scroll to the bottom-right corner, so we can check whether the scroll
  // offset is correctly restored when the extension view is loaded, even when
  // the recommendations are loaded after the initial render.
  ok(
    win.document.querySelector("recommended-addon-card"),
    "Recommendations have already been loaded"
  );
  win.document.body.scrollIntoView({ block: "end", inline: "end" });
  extListOffsets = getScrollOffset(win);
  assertNonZeroScrollOffsets(extListOffsets);

  // Switch back from the extension list to the details view.

  await historyGo(win, +1, `addons://detail/${EXT_ID_EXTENSION}`);
  checkScrollOffset(win, detailsOffsets, "details view with default tab");

  // Switch from the default details tab to the permissions tab.
  // (this does not change the history).
  win.document.querySelector(".tab-button[name='permissions']").click();

  // Switch back from the details view to the extension list.

  await historyGo(win, -1, "addons://list/extension");
  checkScrollOffset(win, extListOffsets, "bottom-right of extension list");
  ok(
    win.document.querySelector("recommended-addon-card"),
    "Recommendations should have been loaded again"
  );

  // Switch back from extension list to the details view.

  await historyGo(win, +1, `addons://detail/${EXT_ID_EXTENSION}`);
  // Scroll offsets are not remembered for the details view, because at the
  // time of leaving the details view, the non-default tab was selected.
  checkScrollOffset(win, { top: 0, left: 0 }, "details view, non-default tab");

  // Switch back from the details view to the disco pane.

  await historyGo(win, -2, "addons://discover/");
  checkScrollOffset(win, discoOffsets, "after switching back to disco pane");

  // Switch from disco pane to theme list.

  // Verifies that the extension list and theme lists are independent.
  await switchToView(win, "list", "theme");
  checkScrollOffset(win, { top: 0, left: 0 }, "initial theme list");

  let tabClosed = BrowserTestUtils.waitForTabClosing(gBrowser.selectedTab);
  await closeView(win);
  await tabClosed;
});
