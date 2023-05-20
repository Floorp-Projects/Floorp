/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var plaintextURL = "data:text/plain,hello+world";
var htmlURL = "about:mozilla";

add_setup(async function () {
  registerCleanupFunction(function () {
    SpecialPowers.clearUserPref("view_source.tab_size");
    SpecialPowers.clearUserPref("view_source.wrap_long_lines");
    SpecialPowers.clearUserPref("view_source.syntax_highlight");
  });
});

add_task(async function () {
  await exercisePrefs(plaintextURL, false);
  await exercisePrefs(htmlURL, true);
});

const contextMenu = document.getElementById("contentAreaContextMenu");
async function openContextMenu(browser) {
  info("Opening context menu");
  const popupShownPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popupshown"
  );
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "html",
    { type: "contextmenu", button: 2 },
    browser
  );
  await popupShownPromise;
  info("Opened context menu");
}

async function closeContextMenu() {
  const popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.hidePopup();
  await popupHiddenPromise;
}

async function simulateClick(id) {
  const popupHiddenPromise = BrowserTestUtils.waitForEvent(
    contextMenu,
    "popuphidden"
  );
  contextMenu.activateItem(document.getElementById(id));
  await popupHiddenPromise;
}

function getAttribute(id, attribute) {
  let item = document.getElementById(id);
  return item.getAttribute(attribute);
}

var exercisePrefs = async function (source, highlightable) {
  let tab = await openDocument(source);
  let browser = tab.linkedBrowser;

  const wrapMenuItem = "context-viewsource-wrapLongLines";
  const syntaxMenuItem = "context-viewsource-highlightSyntax";

  // Test the default states of these menu items.
  await checkStyle(browser, "-moz-tab-size", 4);
  await openContextMenu(browser);
  await checkStyle(browser, "white-space", "pre");
  await checkHighlight(browser, highlightable);
  is(
    getAttribute(wrapMenuItem, "checked"),
    "false",
    "Wrap menu item not checked by default"
  );
  is(
    getAttribute(syntaxMenuItem, "checked"),
    "true",
    "Syntax menu item checked by default"
  );
  await closeContextMenu();

  // Next, test that the Wrap Long Lines menu item works.
  let prefReady = waitForPrefChange("view_source.wrap_long_lines");
  await openContextMenu(browser);
  await simulateClick(wrapMenuItem);
  await openContextMenu(browser);
  await checkStyle(browser, "white-space", "pre-wrap");
  is(getAttribute(wrapMenuItem, "checked"), "true", "Wrap menu item checked");
  await prefReady;
  is(
    SpecialPowers.getBoolPref("view_source.wrap_long_lines"),
    true,
    "Wrap pref set"
  );
  await closeContextMenu();

  prefReady = waitForPrefChange("view_source.wrap_long_lines");
  await openContextMenu(browser);
  await simulateClick(wrapMenuItem);
  await openContextMenu(browser);
  await checkStyle(browser, "white-space", "pre");
  is(
    getAttribute(wrapMenuItem, "checked"),
    "false",
    "Wrap menu item unchecked"
  );
  await prefReady;
  is(
    SpecialPowers.getBoolPref("view_source.wrap_long_lines"),
    false,
    "Wrap pref set"
  );
  await closeContextMenu();

  // Check that the Syntax Highlighting menu item works.
  prefReady = waitForPrefChange("view_source.syntax_highlight");
  await openContextMenu(browser);
  await simulateClick(syntaxMenuItem);
  await openContextMenu(browser);
  await checkHighlight(browser, false);
  is(
    getAttribute(syntaxMenuItem, "checked"),
    "false",
    "Syntax menu item unchecked"
  );
  await prefReady;
  is(
    SpecialPowers.getBoolPref("view_source.syntax_highlight"),
    false,
    "Syntax highlighting pref set"
  );
  await closeContextMenu();

  prefReady = waitForPrefChange("view_source.syntax_highlight");
  await openContextMenu(browser);
  await simulateClick(syntaxMenuItem);
  await openContextMenu(browser);
  await checkHighlight(browser, highlightable);
  is(
    getAttribute(syntaxMenuItem, "checked"),
    "true",
    "Syntax menu item checked"
  );
  await prefReady;
  is(
    SpecialPowers.getBoolPref("view_source.syntax_highlight"),
    true,
    "Syntax highlighting pref set"
  );
  await closeContextMenu();
  gBrowser.removeTab(tab);

  // Open a new view-source window to check that the prefs are obeyed.
  SpecialPowers.setIntPref("view_source.tab_size", 2);
  SpecialPowers.setBoolPref("view_source.wrap_long_lines", true);
  SpecialPowers.setBoolPref("view_source.syntax_highlight", false);

  tab = await openDocument(source);
  browser = tab.linkedBrowser;

  await checkStyle(browser, "-moz-tab-size", 2);
  await openContextMenu(browser);
  await checkStyle(browser, "white-space", "pre-wrap");
  await checkHighlight(browser, false);
  is(getAttribute(wrapMenuItem, "checked"), "true", "Wrap menu item checked");
  is(
    getAttribute(syntaxMenuItem, "checked"),
    "false",
    "Syntax menu item unchecked"
  );

  SpecialPowers.clearUserPref("view_source.tab_size");
  SpecialPowers.clearUserPref("view_source.wrap_long_lines");
  SpecialPowers.clearUserPref("view_source.syntax_highlight");

  await closeContextMenu();
  gBrowser.removeTab(tab);
};

var checkStyle = async function (browser, styleProperty, expected) {
  let value = await SpecialPowers.spawn(
    browser,
    [styleProperty],
    async function (styleProperty) {
      let style = content.getComputedStyle(content.document.body);
      return style.getPropertyValue(styleProperty);
    }
  );
  is(value, "" + expected, "Correct value of " + styleProperty);
};

var checkHighlight = async function (browser, expected) {
  let highlighted = await SpecialPowers.spawn(browser, [], async function () {
    let spans = content.document.getElementsByTagName("span");
    return Array.prototype.some.call(spans, span => {
      let style = content.getComputedStyle(span);
      return style.getPropertyValue("color") !== "rgb(0, 0, 0)";
    });
  });
  is(highlighted, expected, "Syntax highlighting " + (expected ? "on" : "off"));
};
