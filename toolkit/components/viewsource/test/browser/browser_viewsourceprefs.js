/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var plaintextURL = "data:text/plain,hello+world";
var htmlURL = "about:mozilla";

add_task(async function setup() {
  registerCleanupFunction(function() {
    SpecialPowers.clearUserPref("view_source.tab_size");
    SpecialPowers.clearUserPref("view_source.wrap_long_lines");
    SpecialPowers.clearUserPref("view_source.syntax_highlight");
  });
});

add_task(async function() {
  await exercisePrefs(plaintextURL, false);
  await exercisePrefs(htmlURL, true);
});

async function removeChecked(browser, id) {
  await SpecialPowers.spawn(browser, [id], async function(id) {
    let item = content.document.getElementById(id);
    if (item.getAttribute("checked") == "false") {
      item.removeAttribute("checked");
    }
  });
}

async function hasAttribute(browser, id, attribute) {
  return SpecialPowers.spawn(browser, [{ id, attribute }], async function(arg) {
    let item = content.document.getElementById(arg.id);
    return item.hasAttribute(arg.attribute);
  });
}

var exercisePrefs = async function(source, highlightable) {
  let tab = await openDocument(source);
  let browser = tab.linkedBrowser;

  const wrapMenuItem = "wrapLongLines";
  const syntaxMenuItem = "highlightSyntax";

  // Strip checked="false" attributes, since we're not interested in them.
  await removeChecked(browser, wrapMenuItem);
  await removeChecked(browser, syntaxMenuItem);

  // Test the default states of these menu items.
  is(
    await hasAttribute(browser, wrapMenuItem, "checked"),
    false,
    "Wrap menu item not checked by default"
  );
  is(
    await hasAttribute(browser, syntaxMenuItem, "checked"),
    true,
    "Syntax menu item checked by default"
  );

  await checkStyle(browser, "-moz-tab-size", 4);
  await checkStyle(browser, "white-space", "pre");

  // Next, test that the Wrap Long Lines menu item works.
  let prefReady = waitForPrefChange("view_source.wrap_long_lines");
  await simulateClick(browser, wrapMenuItem);
  is(
    await hasAttribute(browser, wrapMenuItem, "checked"),
    true,
    "Wrap menu item checked"
  );
  await prefReady;
  is(
    SpecialPowers.getBoolPref("view_source.wrap_long_lines"),
    true,
    "Wrap pref set"
  );

  await checkStyle(browser, "white-space", "pre-wrap");

  prefReady = waitForPrefChange("view_source.wrap_long_lines");
  await simulateClick(browser, wrapMenuItem);
  is(
    await hasAttribute(browser, wrapMenuItem, "checked"),
    false,
    "Wrap menu item unchecked"
  );
  await prefReady;
  is(
    SpecialPowers.getBoolPref("view_source.wrap_long_lines"),
    false,
    "Wrap pref set"
  );
  await checkStyle(browser, "white-space", "pre");

  // Check that the Syntax Highlighting menu item works.
  prefReady = waitForPrefChange("view_source.syntax_highlight");
  await simulateClick(browser, syntaxMenuItem);
  is(
    await hasAttribute(browser, syntaxMenuItem, "checked"),
    false,
    "Syntax menu item unchecked"
  );
  await prefReady;
  is(
    SpecialPowers.getBoolPref("view_source.syntax_highlight"),
    false,
    "Syntax highlighting pref set"
  );
  await checkHighlight(browser, false);

  prefReady = waitForPrefChange("view_source.syntax_highlight");
  simulateClick(browser, syntaxMenuItem);
  is(
    await hasAttribute(browser, syntaxMenuItem, "checked"),
    true,
    "Syntax menu item checked"
  );
  await prefReady;
  is(
    SpecialPowers.getBoolPref("view_source.syntax_highlight"),
    true,
    "Syntax highlighting pref set"
  );
  await checkHighlight(browser, highlightable);
  gBrowser.removeTab(tab);

  // Open a new view-source window to check that the prefs are obeyed.
  SpecialPowers.setIntPref("view_source.tab_size", 2);
  SpecialPowers.setBoolPref("view_source.wrap_long_lines", true);
  SpecialPowers.setBoolPref("view_source.syntax_highlight", false);

  tab = await openDocument(source);
  browser = tab.linkedBrowser;

  // Strip checked="false" attributes, since we're not interested in them.
  await removeChecked(browser, wrapMenuItem);
  await removeChecked(browser, syntaxMenuItem);

  is(
    await hasAttribute(browser, wrapMenuItem, "checked"),
    true,
    "Wrap menu item checked"
  );
  is(
    await hasAttribute(browser, syntaxMenuItem, "checked"),
    false,
    "Syntax menu item unchecked"
  );
  await checkStyle(browser, "-moz-tab-size", 2);
  await checkStyle(browser, "white-space", "pre-wrap");
  await checkHighlight(browser, false);

  SpecialPowers.clearUserPref("view_source.tab_size");
  SpecialPowers.clearUserPref("view_source.wrap_long_lines");
  SpecialPowers.clearUserPref("view_source.syntax_highlight");

  gBrowser.removeTab(tab);
};

// Simulate a menu item click, including toggling the checked state.
// This saves us from opening the menu and trying to click on the item,
// which doesn't work on Mac OS X.
async function simulateClick(browser, id) {
  return SpecialPowers.spawn(browser, [id], async function(id) {
    let item = content.document.getElementById(id);
    if (item.hasAttribute("checked")) {
      item.removeAttribute("checked");
    } else {
      item.setAttribute("checked", "true");
    }

    item.click();
  });
}

var checkStyle = async function(browser, styleProperty, expected) {
  let value = await SpecialPowers.spawn(
    browser,
    [styleProperty],
    async function(styleProperty) {
      let style = content.getComputedStyle(content.document.body);
      return style.getPropertyValue(styleProperty);
    }
  );
  is(value, "" + expected, "Correct value of " + styleProperty);
};

var checkHighlight = async function(browser, expected) {
  let highlighted = await SpecialPowers.spawn(browser, [], async function() {
    let spans = content.document.getElementsByTagName("span");
    return Array.prototype.some.call(spans, span => {
      let style = content.getComputedStyle(span);
      return style.getPropertyValue("color") !== "rgb(0, 0, 0)";
    });
  });
  is(highlighted, expected, "Syntax highlighting " + (expected ? "on" : "off"));
};
