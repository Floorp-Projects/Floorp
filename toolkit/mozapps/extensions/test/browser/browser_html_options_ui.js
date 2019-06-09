/* eslint max-len: ["error", 80] */

const {AddonTestUtils} =
  ChromeUtils.import("resource://testing-common/AddonTestUtils.jsm");
const {ExtensionParent} =
  ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm");

AddonTestUtils.initMochitest(this);

// This test function helps to detect when an addon options browser have been
// inserted in the about:addons page.
function waitOptionsBrowserInserted() {
  return new Promise(resolve => {
    async function listener(eventName, browser) {
      // wait for a webextension XUL browser element that is owned by the
      // "about:addons" page.
      if (browser.ownerGlobal.top.location.href == "about:addons") {
        ExtensionParent.apiManager.off("extension-browser-inserted", listener);
        resolve(browser);
      }
    }
    ExtensionParent.apiManager.on("extension-browser-inserted", listener);
  });
}

add_task(async function enableHtmlViews() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.htmlaboutaddons.enabled", true],
      ["extensions.htmlaboutaddons.inline-options.enabled", true],
    ],
  });
});

add_task(async function testInlineOptions() {
  const HEIGHT_SHORT = 300;
  const HEIGHT_TALL = 600;

  let id = "inline@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id}},
      options_ui: {
        page: "options.html",
      },
    },
    files: {
      "options.html": `
        <html>
          <head>
            <style type="text/css">
              body > p { height: ${HEIGHT_SHORT}px; margin: 0; }
              body.bigger > p { height: ${HEIGHT_TALL}px; }
            </style>
            <script src="options.js"></script>
          </head>
          <body>
            <p>Some text</p>
          </body>
        </html>
      `,
      "options.js": () => {
        browser.test.onMessage.addListener(msg => {
          if (msg == "toggle-class") {
            document.body.classList.toggle("bigger");
          } else if (msg == "get-height") {
            browser.test.sendMessage("height", document.body.clientHeight);
          }
        });

        browser.test.sendMessage("options-loaded", window.location.href);
      },
    },
    useAddonManager: "temporary",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Make sure we found the right card.
  let card = doc.querySelector(`addon-card[addon-id="${id}"]`);
  ok(card, "Found the card");

  // The preferences option should be visible.
  let preferences = card.querySelector('[action="preferences"]');
  ok(!preferences.hidden, "The preferences option is visible");

  // Open the preferences page.
  let loaded = waitForViewLoad(win);
  preferences.click();
  await loaded;

  // Verify we're on the preferences tab.
  card = doc.querySelector("addon-card");
  is(card.addon.id, id, "The right page was loaded");
  let {deck, tabGroup} = card.details;
  let {selectedViewName} = deck;
  is(selectedViewName, "preferences", "The preferences tab is shown");

  info("Check that there are two buttons and they're visible");
  let detailsBtn = tabGroup.querySelector('[name="details"]');
  ok(!detailsBtn.hidden, "The details button is visible");
  let prefsBtn = tabGroup.querySelector('[name="preferences"]');
  ok(!prefsBtn.hidden, "The preferences button is visible");

  // Wait for the browser to load.
  let url = await extension.awaitMessage("options-loaded");

  // Check the attributes of the options browser.
  let browser = card.querySelector("inline-options-browser browser");
  ok(browser, "The visible view has a browser");
  is(browser.currentURI.spec, card.addon.optionsURL,
     "The browser has the expected options URL");
  is(url, card.addon.optionsURL, "Browser has the expected options URL loaded");
  let stack = browser.closest("stack");
  is(browser.clientWidth, stack.clientWidth,
     "Browser should be the same width as its direct parent");
  ok(stack.clientWidth > 0, "The stack has a width");
  ok(card.querySelector('[action="preferences"]').hidden,
     "The preferences option is hidden now");

  let lastHeight;
  let waitForHeightChange = () => TestUtils.waitForCondition(() => {
    if (browser.clientHeight !== lastHeight) {
      lastHeight = browser.clientHeight;
      return true;
    }
    return false;
  });

  // The expected heights are 1px taller, to work around bug 1548687.
  const EXPECTED_HEIGHT_SHORT = HEIGHT_SHORT + 1;
  const EXPECTED_HEIGHT_TALL = HEIGHT_TALL + 1;

  await waitForHeightChange();

  // Check resizing the browser through extension CSS.
  await extension.sendMessage("get-height");
  let height = await extension.awaitMessage("height");
  is(height, EXPECTED_HEIGHT_SHORT, "The height is smaller to start");
  is(height, browser.clientHeight, "The browser is the same size");

  info("Resize the browser to be taller");
  await extension.sendMessage("toggle-class");
  await waitForHeightChange();
  await extension.sendMessage("get-height");
  height = await extension.awaitMessage("height");
  is(height, EXPECTED_HEIGHT_TALL, "The height is bigger now");
  is(height, browser.clientHeight, "The browser is the same size");

  info("Shrink the browser again");
  await extension.sendMessage("toggle-class");
  await waitForHeightChange();
  await extension.sendMessage("get-height");
  height = await extension.awaitMessage("height");
  is(height, EXPECTED_HEIGHT_SHORT, "The browser shrunk back");
  is(height, browser.clientHeight, "The browser is the same size");

  info("Switching to details view");
  detailsBtn.click();

  info("Check the browser dimensions to make sure it's hidden");
  is(browser.clientWidth, 0, "The browser is hidden now");

  info("Switch back, check browser is shown");
  prefsBtn.click();

  is(browser.clientWidth, stack.clientWidth, "The browser width is set again");
  ok(stack.clientWidth > 0, "The stack has a width");

  await closeView(win);
  await extension.unload();
});

// Regression test against bug 1409697
add_task(async function testCardRerender() {
  let id = "rerender@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: {gecko: {id}},
      options_ui: {
        page: "options.html",
      },
    },
    files: {
      "options.html": `
        <html>
          <body>
            <p>Some text</p>
          </body>
        </html>
      `,
    },
    useAddonManager: "permanent",
  });
  await extension.startup();

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = doc.querySelector(`addon-card[addon-id="${id}"]`);
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = doc.querySelector("addon-card");

  let browserAdded = waitOptionsBrowserInserted();
  card.querySelector('named-deck-button[name="preferences"]').click();
  await browserAdded;

  is(doc.querySelectorAll("inline-options-browser").length, 1,
     "There is 1 inline-options-browser");
  is(doc.querySelectorAll("browser").length, 1, "There is 1 browser");

  info("Reload the add-on and ensure there's still only one browser");
  let updated = BrowserTestUtils.waitForEvent(card, "update", () => {
    // Wait for the update when the add-on name isn't the disabled text.
    return !card.querySelector(".addon-name").hasAttribute("data-l10n-name");
  });
  card.addon.reload();
  await updated;

  is(doc.querySelectorAll("inline-options-browser").length, 1,
     "There is 1 inline-options-browser");
  is(doc.querySelectorAll("browser").length, 1, "There is 1 browser");

  info("Re-rendering card to ensure a second browser isn't added");
  updated = BrowserTestUtils.waitForEvent(card, "update");
  card.render();
  await updated;

  is(card.details.deck.selectedViewName, "details",
     "Rendering reverted to the details view");

  is(doc.querySelectorAll("inline-options-browser").length, 1,
     "There is still only 1 inline-options-browser after re-render");
  is(doc.querySelectorAll("browser").length, 0, "There is no browser");

  let newBrowserAdded = waitOptionsBrowserInserted();
  card.showPrefs();
  await newBrowserAdded;

  is(doc.querySelectorAll("inline-options-browser").length, 1,
     "There is still only 1 inline-options-browser after opening preferences");
  is(doc.querySelectorAll("browser").length, 1, "There is 1 browser");

  await closeView(win);
  await extension.unload();
});
