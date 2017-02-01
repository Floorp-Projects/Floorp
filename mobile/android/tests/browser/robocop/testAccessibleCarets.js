// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");
Cu.import('resource://gre/modules/Geometry.jsm');

const ACCESSIBLECARET_PREF = "layout.accessiblecaret.enabled";
const BASE_TEST_URL = "http://mochi.test:8888/tests/robocop/testAccessibleCarets.html";
const DESIGNMODE_TEST_URL = "http://mochi.test:8888/tests/robocop/testAccessibleCarets2.html";

// Ensures Tabs are completely loaded, viewport and zoom constraints updated, etc.
const TAB_CHANGE_EVENT = "testAccessibleCarets:TabChange";
const TAB_STOP_EVENT = "STOP";

const gChromeWin = Services.wm.getMostRecentWindow("navigator:browser");

/**
 * Wait for and return, when an expected tab change event occurs.
 *
 * @param tabId, The id of the target tab we're observing.
 * @param eventType, The event type we expect.
 * @return {Promise}
 * @resolves The tab change object, including the matched tab id and event.
 */
function do_promiseTabChangeEvent(tabId, eventType) {
  return new Promise(resolve => {
    EventDispatcher.instance.registerListener(function listener(event, message, callback) {
      if (message.event === eventType && message.tabId === tabId) {
        EventDispatcher.instance.unregisterListener(listener, TAB_CHANGE_EVENT);
        resolve();
      }
    }, TAB_CHANGE_EVENT);
  });
}

/**
 * Selection methods vary if we have an input / textarea element,
 * or if we have basic content.
 */
function isInputOrTextarea(element) {
  return ((element instanceof Ci.nsIDOMHTMLInputElement) ||
          (element instanceof Ci.nsIDOMHTMLTextAreaElement));
}

/**
 * Return the selection controller based on element.
 */
function elementSelection(element) {
  return (isInputOrTextarea(element)) ?
    element.editor.selection :
    element.ownerGlobal.getSelection();
}

/**
 * Select the requested character of a target element, w/o affecting focus.
 */
function selectElementChar(doc, element, char) {
  if (isInputOrTextarea(element)) {
    element.setSelectionRange(char, char + 1);
    return;
  }

  // Simple test cases designed firstChild == #text node.
  let range = doc.createRange();
  range.setStart(element.firstChild, char);
  range.setEnd(element.firstChild, char + 1);

  let selection = elementSelection(element);
  selection.removeAllRanges();
  selection.addRange(range);
}

/**
 * Get longpress point. Determine the midpoint in the requested character of
 * the content in the element. X will be midpoint from left to right.
 * Y will be 1/3 of the height up from the bottom to account for both
 * LTR and smaller RTL characters. ie: |X| vs. |א|
 */
function getCharPressPoint(doc, element, char, expected) {
  // Select the first char in the element.
  selectElementChar(doc, element, char);

  // Reality check selected char to expected.
  let selection = elementSelection(element);
  is(selection.toString(), expected, "Selected char should match expected char.");

  // Return a point where long press should select entire word.
  let rect = selection.getRangeAt(0).getBoundingClientRect();
  let r = new Point(rect.left + (rect.width / 2), rect.bottom - (rect.height / 3));

  return r;
}

/**
 * Long press an element (RTL/LTR) at its calculated first character
 * position, and return the result.
 *
 * @param midPoint, The screen coord for the longpress.
 * @return Selection state helper-result object.
 */
function getLongPressResult(browser, midPoint) {
  let domWinUtils = browser.contentWindow.
    QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);

  // AccessibleCarets expect longtap between touchstart/end.
  domWinUtils.sendTouchEventToWindow("touchstart", [0], [midPoint.x], [midPoint.y],
                                     [1], [1], [0], [1], 1, 0);
  domWinUtils.sendMouseEventToWindow("mouselongtap", midPoint.x, midPoint.y,
                                     0, 1, 0);
  domWinUtils.sendTouchEventToWindow("touchend", [0], [midPoint.x], [midPoint.y],
                                     [1], [1], [0], [1], 1, 0);

  let ActionBarHandler = gChromeWin.ActionBarHandler;
  return { focusedElement: ActionBarHandler._targetElement,
           text: ActionBarHandler._getSelectedText(),
           selectionID: ActionBarHandler._selectionID,
  };
}

/**
 * Checks the Selection UI (ActionBar or FloatingToolbar)
 * for the availability of an expected action.
 *
 * @param expectedActionID, The Selection UI action we expect to be available.
 * @return Result boolean.
 */
function UIhasActionByID(expectedActionID) {
  let actions = gChromeWin.ActionBarHandler._actionBarActions;
  return actions.some(action => {
    return action.id === expectedActionID;
  });
}

/**
 * Messages the ActionBarHandler to close the Selection UI.
 */
function closeSelectionUI() {
  gChromeWin.WindowEventDispatcher.dispatch("TextSelection:End",
    {selectionID: gChromeWin.ActionBarHandler._selectionID});
}

/**
 * Main test method.
 */
add_task(function* testAccessibleCarets() {
  // Wait to start loading our test page until after the initial browser tab is
  // completely loaded. This allows each tab to complete its layer initialization,
  // importantly, its viewport and zoomContraints info.
  let BrowserApp = gChromeWin.BrowserApp;
  yield do_promiseTabChangeEvent(BrowserApp.selectedTab.id, TAB_STOP_EVENT);

  // Ensure Gecko Selection and Touch carets are enabled.
  Services.prefs.setBoolPref(ACCESSIBLECARET_PREF, true);

  // Load test page, wait for load completion, register cleanup.
  let browser = BrowserApp.addTab(BASE_TEST_URL).browser;
  let tab = BrowserApp.getTabForBrowser(browser);
  yield do_promiseTabChangeEvent(tab.id, TAB_STOP_EVENT);

  do_register_cleanup(function cleanup() {
    BrowserApp.closeTab(tab);
    Services.prefs.clearUserPref(ACCESSIBLECARET_PREF);
  });

  // References to test document elements.
  let doc = browser.contentDocument;
  let ce_LTR_elem = doc.getElementById("LTRcontenteditable");
  let tc_LTR_elem = doc.getElementById("LTRtextContent");
  let i_LTR_elem = doc.getElementById("LTRinput");
  let ta_LTR_elem = doc.getElementById("LTRtextarea");

  let ce_RTL_elem = doc.getElementById("RTLcontenteditable");
  let tc_RTL_elem = doc.getElementById("RTLtextContent");
  let i_RTL_elem = doc.getElementById("RTLinput");
  let ta_RTL_elem = doc.getElementById("RTLtextarea");

  let ip_LTR_elem = doc.getElementById("LTRphone");
  let ip_RTL_elem = doc.getElementById("RTLphone");
  let bug1265750_elem = doc.getElementById("bug1265750");

  // Locate longpress midpoints for test elements, ensure expactations.
  let ce_LTR_midPoint = getCharPressPoint(doc, ce_LTR_elem, 0, "F");
  let tc_LTR_midPoint = getCharPressPoint(doc, tc_LTR_elem, 0, "O");
  let i_LTR_midPoint = getCharPressPoint(doc, i_LTR_elem, 0, "T");
  let ta_LTR_midPoint = getCharPressPoint(doc, ta_LTR_elem, 0, "W");

  let ce_RTL_midPoint = getCharPressPoint(doc, ce_RTL_elem, 0, "א");
  let tc_RTL_midPoint = getCharPressPoint(doc, tc_RTL_elem, 0, "ת");
  let i_RTL_midPoint = getCharPressPoint(doc, i_RTL_elem, 0, "ל");
  let ta_RTL_midPoint = getCharPressPoint(doc, ta_RTL_elem, 0, "ה");

  let ip_LTR_midPoint = getCharPressPoint(doc, ip_LTR_elem, 8, "2");
  let ip_RTL_midPoint = getCharPressPoint(doc, ip_RTL_elem, 9, "2");
  let bug1265750_midPoint = getCharPressPoint(doc, bug1265750_elem, 2, "7");

  // Longpress various LTR content elements. Test focused element against
  // expected, and selected text against expected.
  let result = getLongPressResult(browser, ce_LTR_midPoint);
  is(result.focusedElement, ce_LTR_elem, "Focused element should match expected.");
  is(result.text, "Find", "Selected text should match expected text.");

  result = getLongPressResult(browser, tc_LTR_midPoint);
  is(result.focusedElement, null, "No focused element is expected.");
  is(result.text, "Open", "Selected text should match expected text.");

  result = getLongPressResult(browser, i_LTR_midPoint);
  is(result.focusedElement, i_LTR_elem, "Focused element should match expected.");
  is(result.text, "Type", "Selected text should match expected text.");

  result = getLongPressResult(browser, ta_LTR_midPoint);
  is(result.focusedElement, ta_LTR_elem, "Focused element should match expected.");
  is(result.text, "Words", "Selected text should match expected text.");

  result = getLongPressResult(browser, ip_LTR_midPoint);
  is(result.focusedElement, ip_LTR_elem, "Focused element should match expected.");
  is(result.text, "09876543210 .-.)(wp#*103410341",
    "Selected phone number should match expected text.");
  is(result.text.length, 30,
    "Selected phone number length should match expected maximum.");

  result = getLongPressResult(browser, bug1265750_midPoint);
  is(result.focusedElement, null, "Focused element should match expected.");
  is(result.text, "3 45 678 90",
    "Selected phone number should match expected text.");

  // Longpress various RTL content elements. Test focused element against
  // expected, and selected text against expected.
  result = getLongPressResult(browser, ce_RTL_midPoint);
  is(result.focusedElement, ce_RTL_elem, "Focused element should match expected.");
  is(result.text, "איפה", "Selected text should match expected text.");

  result = getLongPressResult(browser, tc_RTL_midPoint);
  is(result.focusedElement, null, "No focused element is expected.");
  is(result.text, "תן", "Selected text should match expected text.");

  result = getLongPressResult(browser, i_RTL_midPoint);
  is(result.focusedElement, i_RTL_elem, "Focused element should match expected.");
  is(result.text, "לרוץ", "Selected text should match expected text.");

  result = getLongPressResult(browser, ta_RTL_midPoint);
  is(result.focusedElement, ta_RTL_elem, "Focused element should match expected.");
  is(result.text, "הספר", "Selected text should match expected text.");

  result = getLongPressResult(browser, ip_RTL_midPoint);
  is(result.focusedElement, ip_RTL_elem, "Focused element should match expected.");
  is(result.text, "+972 3 7347514 ",
    "Selected phone number should match expected text.");

  // Close Selection UI (ActionBar or FloatingToolbar) and complete test.
  closeSelectionUI();
  ok(true, "Finished testAccessibleCarets tests.");
});

/**
 * DesignMode test method.
 */
add_task(function* testAccessibleCarets_designMode() {
  let BrowserApp = gChromeWin.BrowserApp;

  // Pre-populate the clipboard to ensure PASTE action available.
  Cc["@mozilla.org/widget/clipboardhelper;1"].
    getService(Ci.nsIClipboardHelper).copyString("somethingMagical");

  // Load test page, wait for load completion.
  let browser = BrowserApp.addTab(DESIGNMODE_TEST_URL).browser;
  let tab = BrowserApp.getTabForBrowser(browser, { selected: true });
  yield do_promiseTabChangeEvent(tab.id, TAB_STOP_EVENT);

  // References to test document elements, ActionBarHandler.
  let doc = browser.contentDocument;
  let tc_LTR_elem = doc.getElementById("LTRtextContent");
  let tc_RTL_elem = doc.getElementById("RTLtextContent");

  // Locate longpress midpoints for test elements, ensure expactations.
  let tc_LTR_midPoint = getCharPressPoint(doc, tc_LTR_elem, 5, "x");
  let tc_RTL_midPoint = getCharPressPoint(doc, tc_RTL_elem, 9, "ת");

  let flavors = ["text/unicode"];
  let clipboardHasText = Services.clipboard.hasDataMatchingFlavors(
    flavors, flavors.length, Ci.nsIClipboard.kGlobalClipboard);
  is(clipboardHasText, true, "There should now be paste-able text in the clipboard.");

  // Toggle designMode on/off/on, check UI expectations.
  ["on", "off"].forEach(designMode => {
    doc.designMode = designMode;

    // Text content in a document, whether in designMode or not, never receives focus.
    // Available ActionBar/FloatingToolbar UI actions should vary depending on mode.

    let result = getLongPressResult(browser, tc_LTR_midPoint);
    is(result.focusedElement, null, "No focused element is expected.");
    is(result.text, "existence", "Selected text should match expected text.");
    is(UIhasActionByID("cut_action"), (designMode === "on"),
      "CUT action UI Visibility should match designMode state.");
    is(UIhasActionByID("paste_action"), (designMode === "on"),
      "PASTE action UI Visibility should match designMode state.");

    result = getLongPressResult(browser, tc_RTL_midPoint);
    is(result.focusedElement, null, "No focused element is expected.");
    is(result.text, "אותו", "Selected text should match expected text.");
    is(UIhasActionByID("cut_action"), (designMode === "on"),
      "CUT action UI Visibility should match designMode state.");
    is(UIhasActionByID("paste_action"), (designMode === "on"),
      "PASTE action UI Visibility should match designMode state.");
  });

  // Close Selection UI (ActionBar or FloatingToolbar) and complete test.
  closeSelectionUI();
  ok(true, "Finished testAccessibleCarets_designMode tests.");
});


// Start all the test tasks.
run_next_test();
