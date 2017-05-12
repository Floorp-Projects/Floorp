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

var exercisePrefs = async function(source, highlightable) {
  let win = await loadViewSourceWindow(source);
  let wrapMenuItem = win.document.getElementById("menu_wrapLongLines");
  let syntaxMenuItem = win.document.getElementById("menu_highlightSyntax");

  // Strip checked="false" attributes, since we're not interested in them.
  if (wrapMenuItem.getAttribute("checked") == "false") {
    wrapMenuItem.removeAttribute("checked");
  }
  if (syntaxMenuItem.getAttribute("checked") == "false") {
    syntaxMenuItem.removeAttribute("checked");
  }

  // Test the default states of these menu items.
  is(wrapMenuItem.hasAttribute("checked"), false,
     "Wrap menu item not checked by default");
  is(syntaxMenuItem.hasAttribute("checked"), true,
     "Syntax menu item checked by default");

  await checkStyle(win, "-moz-tab-size", 4);
  await checkStyle(win, "white-space", "pre");

  // Next, test that the Wrap Long Lines menu item works.
  let prefReady = waitForPrefChange("view_source.wrap_long_lines");
  simulateClick(wrapMenuItem);
  is(wrapMenuItem.hasAttribute("checked"), true, "Wrap menu item checked");
  await prefReady;
  is(SpecialPowers.getBoolPref("view_source.wrap_long_lines"), true, "Wrap pref set");

  await checkStyle(win, "white-space", "pre-wrap");

  prefReady = waitForPrefChange("view_source.wrap_long_lines");
  simulateClick(wrapMenuItem);
  is(wrapMenuItem.hasAttribute("checked"), false, "Wrap menu item unchecked");
  await prefReady;
  is(SpecialPowers.getBoolPref("view_source.wrap_long_lines"), false, "Wrap pref set");
  await checkStyle(win, "white-space", "pre");

  // Check that the Syntax Highlighting menu item works.
  prefReady = waitForPrefChange("view_source.syntax_highlight");
  simulateClick(syntaxMenuItem);
  is(syntaxMenuItem.hasAttribute("checked"), false, "Syntax menu item unchecked");
  await prefReady;
  is(SpecialPowers.getBoolPref("view_source.syntax_highlight"), false, "Syntax highlighting pref set");
  await checkHighlight(win, false);

  prefReady = waitForPrefChange("view_source.syntax_highlight");
  simulateClick(syntaxMenuItem);
  is(syntaxMenuItem.hasAttribute("checked"), true, "Syntax menu item checked");
  await prefReady;
  is(SpecialPowers.getBoolPref("view_source.syntax_highlight"), true, "Syntax highlighting pref set");
  await checkHighlight(win, highlightable);
  await BrowserTestUtils.closeWindow(win);

  // Open a new view-source window to check that the prefs are obeyed.
  SpecialPowers.setIntPref("view_source.tab_size", 2);
  SpecialPowers.setBoolPref("view_source.wrap_long_lines", true);
  SpecialPowers.setBoolPref("view_source.syntax_highlight", false);

  win = await loadViewSourceWindow(source);
  wrapMenuItem = win.document.getElementById("menu_wrapLongLines");
  syntaxMenuItem = win.document.getElementById("menu_highlightSyntax");

  // Strip checked="false" attributes, since we're not interested in them.
  if (wrapMenuItem.getAttribute("checked") == "false") {
    wrapMenuItem.removeAttribute("checked");
  }
  if (syntaxMenuItem.getAttribute("checked") == "false") {
    syntaxMenuItem.removeAttribute("checked");
  }

  is(wrapMenuItem.hasAttribute("checked"), true, "Wrap menu item checked");
  is(syntaxMenuItem.hasAttribute("checked"), false, "Syntax menu item unchecked");
  await checkStyle(win, "-moz-tab-size", 2);
  await checkStyle(win, "white-space", "pre-wrap");
  await checkHighlight(win, false);

  SpecialPowers.clearUserPref("view_source.tab_size");
  SpecialPowers.clearUserPref("view_source.wrap_long_lines");
  SpecialPowers.clearUserPref("view_source.syntax_highlight");

  await BrowserTestUtils.closeWindow(win);
};

// Simulate a menu item click, including toggling the checked state.
// This saves us from opening the menu and trying to click on the item,
// which doesn't work on Mac OS X.
function simulateClick(aMenuItem) {
  if (aMenuItem.hasAttribute("checked"))
    aMenuItem.removeAttribute("checked");
  else
    aMenuItem.setAttribute("checked", "true");

  aMenuItem.click();
}

var checkStyle = async function(win, styleProperty, expected) {
  let browser = win.gBrowser;
  let value = await ContentTask.spawn(browser, styleProperty, async function(styleProperty) {
    let style = content.getComputedStyle(content.document.body);
    return style.getPropertyValue(styleProperty);
  });
  is(value, expected, "Correct value of " + styleProperty);
};

var checkHighlight = async function(win, expected) {
  let browser = win.gBrowser;
  let highlighted = await ContentTask.spawn(browser, {}, async function() {
    let spans = content.document.getElementsByTagName("span");
    return Array.some(spans, (span) => {
      let style = content.getComputedStyle(span);
      return style.getPropertyValue("color") !== "rgb(0, 0, 0)";
    });
  });
  is(highlighted, expected, "Syntax highlighting " + (expected ? "on" : "off"));
};
