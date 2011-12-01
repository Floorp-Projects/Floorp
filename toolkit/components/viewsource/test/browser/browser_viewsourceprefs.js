/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let source = "about:robots";
let mWindow, wrapMenuItem, syntaxMenuItem;

// Check the default values are set.
function test() {
  waitForExplicitFinish();

  registerCleanupFunction(function() {
    SpecialPowers.clearUserPref("view_source.tab_size");
    SpecialPowers.clearUserPref("view_source.wrap_long_lines");
    SpecialPowers.clearUserPref("view_source.syntax_highlight");
  });

  openViewSourceWindow(source, function(aWindow) {
    mWindow = aWindow;
    wrapMenuItem = aWindow.document.getElementById('menu_wrapLongLines');
    syntaxMenuItem = aWindow.document.getElementById('menu_highlightSyntax');

    // Strip checked="false" attributes, since we're not interested in them.
    if (wrapMenuItem.getAttribute("checked") == "false")
      wrapMenuItem.removeAttribute("checked");
    if (syntaxMenuItem.getAttribute("checked") == "false")
      syntaxMenuItem.removeAttribute("checked");

    is(wrapMenuItem.hasAttribute("checked"), false, "Wrap menu item not checked by default");
    is(syntaxMenuItem.hasAttribute("checked"), true, "Syntax menu item checked by default");
    checkStyle(aWindow, "-moz-tab-size", 4);
    checkStyle(aWindow, "white-space", "pre");

    test1();
  });
}

// Check that the Wrap Long Lines menu item works.
function test1() {
  simulateClick(wrapMenuItem);

  is(wrapMenuItem.hasAttribute("checked"), true, "Wrap menu item checked");
  is(SpecialPowers.getBoolPref("view_source.wrap_long_lines"), true, "Wrap pref set");
  checkStyle(mWindow, "white-space", "pre-wrap");
  test2();
}

function test2() {
  simulateClick(wrapMenuItem);

  is(wrapMenuItem.hasAttribute("checked"), false, "Wrap menu item unchecked");
  is(SpecialPowers.getBoolPref("view_source.wrap_long_lines"), false, "Wrap pref set");
  checkStyle(mWindow, "white-space", "pre");
  test3();
}

// Check that the Syntax Highlighting menu item works.
function test3() {
  mWindow.gBrowser.addEventListener("pageshow", function test3Handler() {
    mWindow.gBrowser.removeEventListener("pageshow", test3Handler, false);
    is(syntaxMenuItem.hasAttribute("checked"), false, "Syntax menu item unchecked");
    is(SpecialPowers.getBoolPref("view_source.syntax_highlight"), false, "Syntax highlighting pref set");

    checkHighlight(mWindow, false);
    test4();
  }, false);

  simulateClick(syntaxMenuItem);
}

function test4() {
  mWindow.gBrowser.addEventListener("pageshow", function test4Handler() {
    mWindow.gBrowser.removeEventListener("pageshow", test4Handler, false);
    is(syntaxMenuItem.hasAttribute("checked"), true, "Syntax menu item checked");
    is(SpecialPowers.getBoolPref("view_source.syntax_highlight"), true, "Syntax highlighting pref set");

    checkHighlight(mWindow, true);
    closeViewSourceWindow(mWindow, test5);
  }, false);

  simulateClick(syntaxMenuItem);
}

// Open a new view-source window to check prefs are obeyed.
function test5() {
  SpecialPowers.setIntPref("view_source.tab_size", 2);
  SpecialPowers.setBoolPref("view_source.wrap_long_lines", true);
  SpecialPowers.setBoolPref("view_source.syntax_highlight", false);

  executeSoon(function() {
    openViewSourceWindow(source, function(aWindow) {
      wrapMenuItem = aWindow.document.getElementById('menu_wrapLongLines');
      syntaxMenuItem = aWindow.document.getElementById('menu_highlightSyntax');

      // Strip checked="false" attributes, since we're not interested in them.
      if (wrapMenuItem.getAttribute("checked") == "false")
        wrapMenuItem.removeAttribute("checked");
      if (syntaxMenuItem.getAttribute("checked") == "false")
        syntaxMenuItem.removeAttribute("checked");

      is(wrapMenuItem.hasAttribute("checked"), true, "Wrap menu item checked");
      is(syntaxMenuItem.hasAttribute("checked"), false, "Syntax menu item unchecked");
      checkStyle(aWindow, "-moz-tab-size", 2);
      checkStyle(aWindow, "white-space", "pre-wrap");
      checkHighlight(aWindow, false);
      closeViewSourceWindow(aWindow, finish);
    });
  });
}

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

function checkStyle(aWindow, aStyleProperty, aExpectedValue) {
  let gBrowser = aWindow.gBrowser;
  let computedStyle = gBrowser.contentWindow.getComputedStyle(gBrowser.contentDocument.body, null);

  is(computedStyle.getPropertyValue(aStyleProperty), aExpectedValue, "Correct value of " + aStyleProperty);
}

function checkHighlight(aWindow, aExpected) {
  let spans = aWindow.gBrowser.contentDocument.getElementsByTagName("span");
  is(Array.some(spans, function(aSpan) {
    return aSpan.className != "";
  }), aExpected, "Syntax highlighting " + (aExpected ? "on" : "off"));
}
