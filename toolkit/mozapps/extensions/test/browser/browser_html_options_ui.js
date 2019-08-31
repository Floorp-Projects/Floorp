/* eslint max-len: ["error", 80] */

const { AddonTestUtils } = ChromeUtils.import(
  "resource://testing-common/AddonTestUtils.jsm"
);
const { ExtensionParent } = ChromeUtils.import(
  "resource://gre/modules/ExtensionParent.jsm"
);

AddonTestUtils.initMochitest(this);

function getAddonCard(doc, id) {
  return doc.querySelector(`addon-card[addon-id="${id}"]`);
}

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
    set: [["extensions.htmlaboutaddons.inline-options.enabled", true]],
  });
});

add_task(async function testInlineOptions() {
  const HEIGHT_SHORT = 300;
  const HEIGHT_TALL = 600;

  let id = "inline@mochi.test";
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      applications: { gecko: { id } },
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
  let card = getAddonCard(doc, id);
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
  let { deck, tabGroup } = card.details;
  let { selectedViewName } = deck;
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
  is(
    browser.currentURI.spec,
    card.addon.optionsURL,
    "The browser has the expected options URL"
  );
  is(url, card.addon.optionsURL, "Browser has the expected options URL loaded");
  let stack = browser.closest("stack");
  is(
    browser.clientWidth,
    stack.clientWidth,
    "Browser should be the same width as its direct parent"
  );
  ok(stack.clientWidth > 0, "The stack has a width");
  ok(
    card.querySelector('[action="preferences"]').hidden,
    "The preferences option is hidden now"
  );

  let waitForHeightChange = expectedHeight =>
    TestUtils.waitForCondition(() => browser.clientHeight === expectedHeight);

  // The expected heights are 1px taller, to work around bug 1548687.
  const EXPECTED_HEIGHT_SHORT = HEIGHT_SHORT + 1;
  const EXPECTED_HEIGHT_TALL = HEIGHT_TALL + 1;

  await waitForHeightChange(EXPECTED_HEIGHT_SHORT);

  // Check resizing the browser through extension CSS.
  await extension.sendMessage("get-height");
  let height = await extension.awaitMessage("height");
  is(height, EXPECTED_HEIGHT_SHORT, "The height is smaller to start");
  is(height, browser.clientHeight, "The browser is the same size");

  info("Resize the browser to be taller");
  await extension.sendMessage("toggle-class");
  await waitForHeightChange(EXPECTED_HEIGHT_TALL);
  await extension.sendMessage("get-height");
  height = await extension.awaitMessage("height");
  is(height, EXPECTED_HEIGHT_TALL, "The height is bigger now");
  is(height, browser.clientHeight, "The browser is the same size");

  info("Shrink the browser again");
  await extension.sendMessage("toggle-class");
  await waitForHeightChange(EXPECTED_HEIGHT_SHORT);
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
      applications: { gecko: { id } },
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

  let card = getAddonCard(doc, id);
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = doc.querySelector("addon-card");

  let browserAdded = waitOptionsBrowserInserted();
  card.querySelector('named-deck-button[name="preferences"]').click();
  await browserAdded;

  is(
    doc.querySelectorAll("inline-options-browser").length,
    1,
    "There is 1 inline-options-browser"
  );
  is(doc.querySelectorAll("browser").length, 1, "There is 1 browser");

  info("Reload the add-on and ensure there's still only one browser");
  let updated = BrowserTestUtils.waitForEvent(card, "update");
  card.addon.reload();
  await updated;

  // Since the add-on was disabled, we'll be on the details tab.
  is(card.details.deck.selectedViewName, "details", "View changed to details");
  is(
    doc.querySelectorAll("inline-options-browser").length,
    1,
    "There is 1 inline-options-browser"
  );
  is(doc.querySelectorAll("browser").length, 0, "The browser was destroyed");

  // Load the permissions tab again.
  browserAdded = waitOptionsBrowserInserted();
  card.querySelector('named-deck-button[name="preferences"]').click();
  await browserAdded;

  // Switching to preferences will create a new browser element.
  is(
    card.details.deck.selectedViewName,
    "preferences",
    "View switched to preferences"
  );
  is(
    doc.querySelectorAll("inline-options-browser").length,
    1,
    "There is 1 inline-options-browser"
  );
  is(doc.querySelectorAll("browser").length, 1, "There is a new browser");

  info("Re-rendering card to ensure a second browser isn't added");
  updated = BrowserTestUtils.waitForEvent(card, "update");
  card.render();
  await updated;

  is(
    card.details.deck.selectedViewName,
    "details",
    "Rendering reverted to the details view"
  );
  is(
    doc.querySelectorAll("inline-options-browser").length,
    1,
    "There is still only 1 inline-options-browser after re-render"
  );
  is(doc.querySelectorAll("browser").length, 0, "There is no browser");

  let newBrowserAdded = waitOptionsBrowserInserted();
  card.showPrefs();
  await newBrowserAdded;

  is(
    doc.querySelectorAll("inline-options-browser").length,
    1,
    "There is still only 1 inline-options-browser after opening preferences"
  );
  is(doc.querySelectorAll("browser").length, 1, "There is 1 browser");

  await closeView(win);
  await extension.unload();
});

add_task(async function testRemovedOnDisable() {
  let id = "disable@mochi.test";
  const xpiFile = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id } },
      options_ui: {
        page: "options.html",
      },
    },
    files: {
      "options.html": "<h1>Options!</h1>",
    },
  });
  let addon = await AddonManager.installTemporaryAddon(xpiFile);

  let win = await loadInitialView("extension");
  let doc = win.document;

  // Opens the prefs page.
  let loaded = waitForViewLoad(win);
  getAddonCard(doc, id)
    .querySelector("[action=preferences]")
    .click();
  await loaded;

  let inlineOptions = doc.querySelector("inline-options-browser");
  ok(inlineOptions, "There's an inline-options-browser element");
  ok(inlineOptions.querySelector("browser"), "The browser exists");

  let card = getAddonCard(doc, id);
  let { deck } = card.details;
  is(deck.selectedViewName, "preferences", "Preferences are the active tab");

  info("Disabling the add-on");
  let updated = BrowserTestUtils.waitForEvent(card, "update");
  await addon.disable();
  await updated;

  is(deck.selectedViewName, "details", "Details are now the active tab");
  ok(inlineOptions, "There's an inline-options-browser element");
  ok(!inlineOptions.querySelector("browser"), "The browser has been removed");

  info("Enabling the add-on");
  updated = BrowserTestUtils.waitForEvent(card, "update");
  await addon.enable();
  await updated;

  is(deck.selectedViewName, "details", "Details are still the active tab");
  ok(inlineOptions, "There's an inline-options-browser element");
  ok(!inlineOptions.querySelector("browser"), "The browser is not created yet");

  info("Switching to preferences tab");
  let changed = BrowserTestUtils.waitForEvent(deck, "view-changed");
  let browserAdded = waitOptionsBrowserInserted();
  deck.selectedViewName = "preferences";
  await changed;
  await browserAdded;

  is(deck.selectedViewName, "preferences", "Preferences are selected");
  ok(inlineOptions, "There's an inline-options-browser element");
  ok(inlineOptions.querySelector("browser"), "The browser is re-created");

  await closeView(win);
  await addon.uninstall();
});

add_task(async function testUpgradeTemporary() {
  let id = "upgrade-temporary@mochi.test";
  async function loadExtension(version) {
    let extension = ExtensionTestUtils.loadExtension({
      manifest: {
        applications: { gecko: { id } },
        version,
        options_ui: {
          page: "options.html",
        },
      },
      files: {
        "options.html": `
          <html>
            <head>
              <script src="options.js"></script>
            </head>
            <body>
              <p>Version <pre>${version}</pre></p>
            </body>
          </html>
        `,
        "options.js": () => {
          browser.test.onMessage.addListener(msg => {
            if (msg === "get-version") {
              let version = document.querySelector("pre").textContent;
              browser.test.sendMessage("version", version);
            }
          });
          window.onload = () => browser.test.sendMessage("options-loaded");
        },
      },
      useAddonManager: "temporary",
    });
    await extension.startup();
    return extension;
  }

  let firstExtension = await loadExtension("1");
  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getAddonCard(doc, id);
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = doc.querySelector("addon-card");
  let browserAdded = waitOptionsBrowserInserted();
  card.querySelector('named-deck-button[name="preferences"]').click();
  await browserAdded;

  await firstExtension.awaitMessage("options-loaded");
  await firstExtension.sendMessage("get-version");
  let version = await firstExtension.awaitMessage("version");
  is(version, "1", "Version 1 page is loaded");

  let updated = BrowserTestUtils.waitForEvent(card, "update");
  browserAdded = waitOptionsBrowserInserted();
  let secondExtension = await loadExtension("2");
  await updated;
  await browserAdded;
  await secondExtension.awaitMessage("options-loaded");

  await secondExtension.sendMessage("get-version");
  version = await secondExtension.awaitMessage("version");
  is(version, "2", "Version 2 page is loaded");
  let { deck } = card.details;
  is(deck.selectedViewName, "preferences", "Preferences are still shown");

  await closeView(win);
  await firstExtension.unload();
  await secondExtension.unload();
});

add_task(async function testReloadExtension() {
  let id = "reload@mochi.test";
  let xpiFile = AddonTestUtils.createTempWebExtensionFile({
    manifest: {
      applications: { gecko: { id } },
      options_ui: {
        page: "options.html",
      },
    },
    files: {
      "options.html": `
        <html>
          <head>
          </head>
          <body>
            <p>Options</p>
          </body>
        </html>
      `,
    },
  });
  let addon = await AddonManager.installTemporaryAddon(xpiFile);

  let win = await loadInitialView("extension");
  let doc = win.document;

  let card = getAddonCard(doc, id);
  let loaded = waitForViewLoad(win);
  card.querySelector('[action="expand"]').click();
  await loaded;

  card = doc.querySelector("addon-card");
  let { deck } = card.details;
  is(deck.selectedViewName, "details", "Details load first");

  let browserAdded = waitOptionsBrowserInserted();
  card.querySelector('named-deck-button[name="preferences"]').click();
  await browserAdded;

  is(deck.selectedViewName, "preferences", "Preferences are shown");

  let updated = BrowserTestUtils.waitForEvent(card, "update");
  browserAdded = waitOptionsBrowserInserted();
  let addonStarted = AddonTestUtils.promiseWebExtensionStartup(id);
  await addon.reload();
  await addonStarted;
  await updated;
  await browserAdded;
  is(deck.selectedViewName, "preferences", "Preferences are still shown");

  await closeView(win);
  await addon.uninstall();
});
