/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let plaintextURL = "data:text/plain,hello+world";
let htmlURL = "about:mozilla";

add_task(function* setup() {
  registerCleanupFunction(function() {
    SpecialPowers.clearUserPref("view_source.tab_size");
    SpecialPowers.clearUserPref("view_source.wrap_long_lines");
    SpecialPowers.clearUserPref("view_source.syntax_highlight");
  });
});

add_task(function*() {
  yield exercisePrefs(plaintextURL, false);
  yield exercisePrefs(htmlURL, true);
});

let exercisePrefs = Task.async(function* (source, highlightable) {
  let win = yield loadViewSourceWindow(source);
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

  yield checkStyle(win, "-moz-tab-size", 4);
  yield checkStyle(win, "white-space", "pre");

  // Next, test that the Wrap Long Lines menu item works.
  simulateClick(wrapMenuItem);
  is(wrapMenuItem.hasAttribute("checked"), true, "Wrap menu item checked");
  is(SpecialPowers.getBoolPref("view_source.wrap_long_lines"), true, "Wrap pref set");

  yield checkStyle(win, "white-space", "pre-wrap");

  simulateClick(wrapMenuItem);
  is(wrapMenuItem.hasAttribute("checked"), false, "Wrap menu item unchecked");
  is(SpecialPowers.getBoolPref("view_source.wrap_long_lines"), false, "Wrap pref set");
  yield checkStyle(win, "white-space", "pre");

  // Check that the Syntax Highlighting menu item works.
  let pageShowPromise = BrowserTestUtils.waitForEvent(win.gBrowser, "pageshow");
  simulateClick(syntaxMenuItem);
  yield pageShowPromise;

  is(syntaxMenuItem.hasAttribute("checked"), false, "Syntax menu item unchecked");
  is(SpecialPowers.getBoolPref("view_source.syntax_highlight"), false, "Syntax highlighting pref set");
  yield checkHighlight(win, false);

  pageShowPromise = BrowserTestUtils.waitForEvent(win.gBrowser, "pageshow");
  simulateClick(syntaxMenuItem);
  yield pageShowPromise;

  is(syntaxMenuItem.hasAttribute("checked"), true, "Syntax menu item checked");
  is(SpecialPowers.getBoolPref("view_source.syntax_highlight"), true, "Syntax highlighting pref set");

  yield checkHighlight(win, highlightable);
  yield BrowserTestUtils.closeWindow(win);

  // Open a new view-source window to check that the prefs are obeyed.
  SpecialPowers.setIntPref("view_source.tab_size", 2);
  SpecialPowers.setBoolPref("view_source.wrap_long_lines", true);
  SpecialPowers.setBoolPref("view_source.syntax_highlight", false);

  win = yield loadViewSourceWindow(source);
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
  yield checkStyle(win, "-moz-tab-size", 2);
  yield checkStyle(win, "white-space", "pre-wrap");
  yield checkHighlight(win, false);

  SpecialPowers.clearUserPref("view_source.tab_size");
  SpecialPowers.clearUserPref("view_source.wrap_long_lines");
  SpecialPowers.clearUserPref("view_source.syntax_highlight");

  yield BrowserTestUtils.closeWindow(win);
});

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

let checkStyle = Task.async(function* (win, styleProperty, expected) {
  let browser = win.gBrowser;
  let value = yield ContentTask.spawn(browser, styleProperty, function* (styleProperty) {
    let style = content.getComputedStyle(content.document.body, null);
    return style.getPropertyValue(styleProperty);
  });
  is(value, expected, "Correct value of " + styleProperty);
});

let checkHighlight = Task.async(function* (win, expected) {
  let browser = win.gBrowser;
  let highlighted = yield ContentTask.spawn(browser, {}, function* () {
    let spans = content.document.getElementsByTagName("span");
    return Array.some(spans, (span) => {
      return span.className != "";
    });
  });
  is(highlighted, expected, "Syntax highlighting " + (expected ? "on" : "off"));
});
