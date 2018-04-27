/* import-globals-from ../widgets/popup_shared.js */

var gMenuPopup = null;
var gTrigger = null;
var gIsMenu = false;
var gScreenX = -1, gScreenY = -1;
var gCachedEvent = null;
var gCachedEvent2 = null;

function cacheEvent(modifiers) {
  var cachedEvent = null;

  var mouseFn = function(event) {
    cachedEvent = event;
  };

  window.addEventListener("mousedown", mouseFn);
  synthesizeMouse(document.documentElement, 0, 0, modifiers);
  window.removeEventListener("mousedown", mouseFn);

  return cachedEvent;
}

function runTests() {
  if (screen.height < 768) {
    ok(false, "popup tests are likely to fail for screen heights less than 768 pixels");
  }

  gMenuPopup = document.getElementById("thepopup");
  gTrigger = document.getElementById("trigger");

  gIsMenu = gTrigger.boxObject instanceof MenuBoxObject;

  // a hacky way to get the screen position of the document. Cache the event
  // so that we can use it in calls to openPopup.
  gCachedEvent = cacheEvent({ shiftKey: true });
  gScreenX = gCachedEvent.screenX;
  gScreenY = gCachedEvent.screenY;
  gCachedEvent2 = cacheEvent({ altKey: true, ctrlKey: true, shiftKey: true, metaKey: true });

  startPopupTests(popupTests);
}

var popupTests = [
{
  testname: "mouse click on trigger",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test() {
    // for menus, no trigger will be set. For non-menus using the popup
    // attribute, the trigger will be set to the node with the popup attribute
    gExpectedTriggerNode = gIsMenu ? "notset" : gTrigger;
    synthesizeMouse(gTrigger, 4, 4, { });
  },
  result(testname) {
    gExpectedTriggerNode = null;
    // menus are the anchor but non-menus are opened at screen coordinates
    is(gMenuPopup.anchorNode, gIsMenu ? gTrigger : null, testname + " anchorNode");
    // menus are opened internally, but non-menus have a mouse event which
    // triggered them
    is(gMenuPopup.triggerNode, gIsMenu ? null : gTrigger, testname + " triggerNode");
    is(document.popupNode, gIsMenu ? null : gTrigger, testname + " document.popupNode");
    is(document.tooltipNode, null, testname + " document.tooltipNode");
    // check to ensure the popup node for a different document isn't used
    if (window.opener)
      is(window.opener.document.popupNode, null, testname + " opener.document.popupNode");

    // this will be used in some tests to ensure the size doesn't change
    var popuprect = gMenuPopup.getBoundingClientRect();
    gPopupWidth = Math.round(popuprect.width);
    gPopupHeight = Math.round(popuprect.height);

    checkActive(gMenuPopup, "", testname);
    checkOpen("trigger", testname);
    // if a menu, the popup should be opened underneath the menu in the
    // 'after_start' position, otherwise it is opened at the mouse position
    if (gIsMenu)
      compareEdge(gTrigger, gMenuPopup, "after_start", 0, 0, testname);
  }
},
{
  // check that pressing cursor down while there is no selection
  // highlights the first item
  testname: "cursor down no selection",
  events: [ "DOMMenuItemActive item1" ],
  test() { synthesizeKey("KEY_ArrowDown"); },
  result(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // check that pressing cursor up wraps and highlights the last item
  testname: "cursor up wrap",
  events() {
    // No wrapping on menus on Mac
    return platformIsMac() ? [] : [ "DOMMenuItemInactive item1", "DOMMenuItemActive last" ];
  },
  test() { synthesizeKey("KEY_ArrowUp"); },
  result(testname) {
    checkActive(gMenuPopup, platformIsMac() ? "item1" : "last", testname);
  }
},
{
  // check that pressing cursor down wraps and highlights the first item
  testname: "cursor down wrap",
  condition() { return !platformIsMac(); },
  events: ["DOMMenuItemInactive last", "DOMMenuItemActive item1" ],
  test() { synthesizeKey("KEY_ArrowDown"); },
  result(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // check that pressing cursor down highlights the second item
  testname: "cursor down",
  events: [ "DOMMenuItemInactive item1", "DOMMenuItemActive item2" ],
  test() { synthesizeKey("KEY_ArrowDown"); },
  result(testname) { checkActive(gMenuPopup, "item2", testname); }
},
{
  // check that pressing cursor up highlights the second item
  testname: "cursor up",
  events: [ "DOMMenuItemInactive item2", "DOMMenuItemActive item1" ],
  test() { synthesizeKey("KEY_ArrowUp"); },
  result(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // cursor left should not do anything
  testname: "cursor left",
  test() { synthesizeKey("KEY_ArrowLeft"); },
  result(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // cursor right should not do anything
  testname: "cursor right",
  test() { synthesizeKey("KEY_ArrowRight"); },
  result(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // check cursor down when a disabled item exists in the menu
  testname: "cursor down disabled",
  events() {
    // On Windows, disabled items are included when navigating, but on
    // other platforms, disabled items are skipped over
    if (navigator.platform.indexOf("Win") == 0) {
      return [ "DOMMenuItemInactive item1", "DOMMenuItemActive item2" ];
    }
    return [ "DOMMenuItemInactive item1", "DOMMenuItemActive amenu" ];
  },
  test() {
    document.getElementById("item2").disabled = true;
    synthesizeKey("KEY_ArrowDown");
  }
},
{
  // check cursor up when a disabled item exists in the menu
  testname: "cursor up disabled",
  events() {
    if (navigator.platform.indexOf("Win") == 0) {
      return [ "DOMMenuItemInactive item2", "DOMMenuItemActive amenu",
               "DOMMenuItemInactive amenu", "DOMMenuItemActive item2",
               "DOMMenuItemInactive item2", "DOMMenuItemActive item1" ];
    }
    return [ "DOMMenuItemInactive amenu", "DOMMenuItemActive item1" ];
  },
  test() {
    if (navigator.platform.indexOf("Win") == 0)
      synthesizeKey("KEY_ArrowDown");
    synthesizeKey("KEY_ArrowUp");
    if (navigator.platform.indexOf("Win") == 0)
      synthesizeKey("KEY_ArrowUp");
  }
},
{
  testname: "mouse click outside",
  events: [ "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuItemInactive item1", "DOMMenuInactive thepopup" ],
  test() {
    gMenuPopup.hidePopup();
    // XXXndeakin event simulation fires events outside of the platform specific
    // widget code so the popup capturing isn't handled. Thus, the menu won't
    // rollup this way.
    // synthesizeMouse(gTrigger, 0, -12, { });
  },
  result(testname, step) {
    is(gMenuPopup.anchorNode, null, testname + " anchorNode");
    is(gMenuPopup.triggerNode, null, testname + " triggerNode");
    is(document.popupNode, null, testname + " document.popupNode");
    checkClosed("trigger", testname);
  }
},
{
  // these tests check to ensure that passing an anchor and position
  // puts the popup in the right place
  testname: "open popup anchored",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  steps: ["before_start", "before_end", "after_start", "after_end",
          "start_before", "start_after", "end_before", "end_after", "after_pointer", "overlap",
          "topleft topleft", "topcenter topleft", "topright topleft",
          "leftcenter topright", "rightcenter topright",
          "bottomleft bottomleft", "bottomcenter bottomleft", "bottomright bottomleft",
          "topleft bottomright", "bottomcenter bottomright", "rightcenter topright"],
  test(testname, step) {
    gExpectedTriggerNode = "notset";
    gMenuPopup.openPopup(gTrigger, step, 0, 0, false, false);
  },
  result(testname, step) {
    // no triggerNode because it was opened without passing an event
    gExpectedTriggerNode = null;
    is(gMenuPopup.anchorNode, gTrigger, testname + " anchorNode");
    is(gMenuPopup.triggerNode, null, testname + " triggerNode");
    is(document.popupNode, null, testname + " document.popupNode");
    compareEdge(gTrigger, gMenuPopup, step, 0, 0, testname);
  }
},
{
  // these tests check the same but with a 10 pixel margin on the popup
  testname: "open popup anchored with margin",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  steps: ["before_start", "before_end", "after_start", "after_end",
          "start_before", "start_after", "end_before", "end_after", "after_pointer", "overlap",
          "topleft topleft", "topcenter topleft", "topright topleft",
          "leftcenter topright", "rightcenter topright",
          "bottomleft bottomleft", "bottomcenter bottomleft", "bottomright bottomleft",
          "topleft bottomright", "bottomcenter bottomright", "rightcenter topright"],
  test(testname, step) {
    gMenuPopup.setAttribute("style", "margin: 10px;");
    gMenuPopup.openPopup(gTrigger, step, 0, 0, false, false);
  },
  result(testname, step) {
    var rightmod = step == "before_end" || step == "after_end" ||
                   step == "start_before" || step == "start_after" ||
                   step.match(/topright$/) || step.match(/bottomright$/);
    var bottommod = step == "before_start" || step == "before_end" ||
                    step == "start_after" || step == "end_after" ||
                   step.match(/bottomleft$/) || step.match(/bottomright$/);
    compareEdge(gTrigger, gMenuPopup, step, rightmod ? -10 : 10, bottommod ? -10 : 10, testname);
    gMenuPopup.removeAttribute("style");
  }
},
{
  // these tests check the same but with a -8 pixel margin on the popup
  testname: "open popup anchored with negative margin",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  steps: ["before_start", "before_end", "after_start", "after_end",
          "start_before", "start_after", "end_before", "end_after", "after_pointer", "overlap"],
  test(testname, step) {
    gMenuPopup.setAttribute("style", "margin: -8px;");
    gMenuPopup.openPopup(gTrigger, step, 0, 0, false, false);
  },
  result(testname, step) {
    var rightmod = step == "before_end" || step == "after_end" ||
                   step == "start_before" || step == "start_after";
    var bottommod = step == "before_start" || step == "before_end" ||
                    step == "start_after" || step == "end_after";
    compareEdge(gTrigger, gMenuPopup, step, rightmod ? 8 : -8, bottommod ? 8 : -8, testname);
    gMenuPopup.removeAttribute("style");
  }
},
 {
  testname: "open popup with large positive margin",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  steps: ["before_start", "before_end", "after_start", "after_end",
          "start_before", "start_after", "end_before", "end_after", "after_pointer", "overlap"],
  test(testname, step) {
    gMenuPopup.setAttribute("style", "margin: 1000px;");
    gMenuPopup.openPopup(gTrigger, step, 0, 0, false, false);
  },
  result(testname, step) {
    var popuprect = gMenuPopup.getBoundingClientRect();
    // as there is more room on the 'end' or 'after' side, popups will always
    // appear on the right or bottom corners, depending on which side they are
    // allowed to be flipped by.
    var expectedleft = step == "before_end" || step == "after_end" ?
                       0 : Math.round(window.innerWidth - gPopupWidth);
    var expectedtop = step == "start_after" || step == "end_after" ?
                      0 : Math.round(window.innerHeight - gPopupHeight);
    is(Math.round(popuprect.left), expectedleft, testname + " x position " + step);
    is(Math.round(popuprect.top), expectedtop, testname + " y position " + step);
    gMenuPopup.removeAttribute("style");
  }
},
{
  testname: "open popup with large negative margin",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  steps: ["before_start", "before_end", "after_start", "after_end",
          "start_before", "start_after", "end_before", "end_after", "after_pointer", "overlap"],
  test(testname, step) {
    gMenuPopup.setAttribute("style", "margin: -1000px;");
    gMenuPopup.openPopup(gTrigger, step, 0, 0, false, false);
  },
  result(testname, step) {
    var popuprect = gMenuPopup.getBoundingClientRect();
    // using negative margins causes the reverse of positive margins, and
    // popups will appear on the left or top corners.
    var expectedleft = step == "before_end" || step == "after_end" ?
                       Math.round(window.innerWidth - gPopupWidth) : 0;
    var expectedtop = step == "start_after" || step == "end_after" ?
                      Math.round(window.innerHeight - gPopupHeight) : 0;
    is(Math.round(popuprect.left), expectedleft, testname + " x position " + step);
    is(Math.round(popuprect.top), expectedtop, testname + " y position " + step);
    gMenuPopup.removeAttribute("style");
  }
},
{
  testname: "popup with unknown step",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test() {
    gMenuPopup.openPopup(gTrigger, "other", 0, 0, false, false);
  },
  result(testname) {
    var triggerrect = gMenuPopup.getBoundingClientRect();
    var popuprect = gMenuPopup.getBoundingClientRect();
    is(Math.round(popuprect.left), triggerrect.left, testname + " x position ");
    is(Math.round(popuprect.top), triggerrect.top, testname + " y position ");
  }
},
{
  // these tests check to ensure that the position attribute can be used
  // to set the position of a popup instead of passing it as an argument
  testname: "open popup anchored with attribute",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  steps: ["before_start", "before_end", "after_start", "after_end",
          "start_before", "start_after", "end_before", "end_after", "after_pointer", "overlap",
          "topcenter topleft", "topright bottomright", "leftcenter topright"],
  test(testname, step) {
    gMenuPopup.setAttribute("position", step);
    gMenuPopup.openPopup(gTrigger, "", 0, 0, false, false);
  },
  result(testname, step) { compareEdge(gTrigger, gMenuPopup, step, 0, 0, testname); }
},
{
  // this test checks to ensure that the attributes override flag to openPopup
  // can be used to override the popup's position. This test also passes an
  // event to openPopup to check the trigger node.
  testname: "open popup anchored with override",
  events: [ "popupshowing thepopup 0010", "popupshown thepopup" ],
  test(testname, step) {
    // attribute overrides the position passed in
    gMenuPopup.setAttribute("position", "end_after");
    gExpectedTriggerNode = gCachedEvent.target;
    gMenuPopup.openPopup(gTrigger, "before_start", 0, 0, false, true, gCachedEvent);
  },
  result(testname, step) {
    gExpectedTriggerNode = null;
    is(gMenuPopup.anchorNode, gTrigger, testname + " anchorNode");
    is(gMenuPopup.triggerNode, gCachedEvent.target, testname + " triggerNode");
    is(document.popupNode, gCachedEvent.target, testname + " document.popupNode");
    compareEdge(gTrigger, gMenuPopup, "end_after", 0, 0, testname);
  }
},
{
  testname: "close popup with escape",
  events: [ "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuInactive thepopup", ],
  test(testname, step) {
    synthesizeKey("KEY_Escape");
    checkClosed("trigger", testname);
  }
},
{
  // check that offsets may be supplied to the openPopup method
  testname: "open popup anchored with offsets",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test(testname, step) {
    // attribute is empty so does not override
    gMenuPopup.setAttribute("position", "");
    gMenuPopup.openPopup(gTrigger, "before_start", 5, 10, true, true);
  },
  result(testname, step) { compareEdge(gTrigger, gMenuPopup, "before_start", 5, 10, testname); }
},
{
  // if no anchor is supplied to openPopup, it should be opened relative
  // to the viewport.
  testname: "open popup unanchored",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test(testname, step) { gMenuPopup.openPopup(null, "after_start", 6, 8, false); },
  result(testname, step) {
    var rect = gMenuPopup.getBoundingClientRect();
    ok(rect.left == 6 && rect.top == 8 && rect.right && rect.bottom, testname);
  }
},
{
  testname: "activate menuitem with mouse",
  events: [ "DOMMenuInactive thepopup", "command item3",
            "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuItemInactive item3" ],
  test(testname, step) {
    var item3 = document.getElementById("item3");
    synthesizeMouse(item3, 4, 4, { });
  },
  result(testname, step) { checkClosed("trigger", testname); }
},
{
  testname: "close popup",
  condition() { return false; },
  events: [ "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuInactive thepopup" ],
  test(testname, step) { gMenuPopup.hidePopup(); }
},
{
  testname: "open popup at screen",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test(testname, step) {
    gExpectedTriggerNode = "notset";
    gMenuPopup.openPopupAtScreen(gScreenX + 24, gScreenY + 20, false);
  },
  result(testname, step) {
    gExpectedTriggerNode = null;
    is(gMenuPopup.anchorNode, null, testname + " anchorNode");
    is(gMenuPopup.triggerNode, null, testname + " triggerNode");
    is(document.popupNode, null, testname + " document.popupNode");
    var rect = gMenuPopup.getBoundingClientRect();
    is(rect.left, 24, testname + " left");
    is(rect.top, 20, testname + " top");
    ok(rect.right, testname + " right is " + rect.right);
    ok(rect.bottom, testname + " bottom is " + rect.bottom);
  }
},
{
  // check that pressing a menuitem's accelerator selects it. Note that
  // the menuitem with the M accesskey overrides the earlier menuitem that
  // begins with M.
  testname: "menuitem accelerator",
  events: [ "DOMMenuItemActive amenu", "DOMMenuItemInactive amenu",
            "DOMMenuInactive thepopup",
            "command amenu", "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuItemInactive amenu"
           ],
  test() { sendString("M"); },
  result(testname) { checkClosed("trigger", testname); }
},
{
  testname: "open context popup at screen",
  events: [ "popupshowing thepopup 0010", "popupshown thepopup" ],
  test(testname, step) {
    gExpectedTriggerNode = gCachedEvent.target;
    gMenuPopup.openPopupAtScreen(gScreenX + 8, gScreenY + 16, true, gCachedEvent);
  },
  result(testname, step) {
    gExpectedTriggerNode = null;
    is(gMenuPopup.anchorNode, null, testname + " anchorNode");
    is(gMenuPopup.triggerNode, gCachedEvent.target, testname + " triggerNode");
    is(document.popupNode, gCachedEvent.target, testname + " document.popupNode");

    var childframe = document.getElementById("childframe");
    if (childframe) {
      for (var t = 0; t < 2; t++) {
        var child = childframe.contentDocument;
        var evt = child.createEvent("Event");
        evt.initEvent("click", true, true);
        child.documentElement.dispatchEvent(evt);
        is(child.documentElement.getAttribute("data"), "xnull",
           "cannot get popupNode from other document");
        child.documentElement.setAttribute("data", "none");
        // now try again with document.popupNode set explicitly
        document.popupNode = gCachedEvent.target;
      }
    }

    var openX = 8;
    var openY = 16;
    var rect = gMenuPopup.getBoundingClientRect();
    is(rect.left, openX + (platformIsMac() ? 1 : 2), testname + " left");
    is(rect.top, openY + (platformIsMac() ? -6 : 2), testname + " top");
    ok(rect.right, testname + " right is " + rect.right);
    ok(rect.bottom, testname + " bottom is " + rect.bottom);
  }
},
{
  // pressing a letter that doesn't correspond to an accelerator, but does
  // correspond to the first letter in a menu's label. The menu should not
  // close because there is more than one item corresponding to that letter
  testname: "menuitem with non accelerator",
  events: [ "DOMMenuItemActive one" ],
  test() { sendString("O"); },
  result(testname) {
    checkOpen("trigger", testname);
    checkActive(gMenuPopup, "one", testname);
  }
},
{
  // pressing the letter again should select the next one that starts with
  // that letter
  testname: "menuitem with non accelerator again",
  events: [ "DOMMenuItemInactive one", "DOMMenuItemActive submenu" ],
  test() { sendString("O"); },
  result(testname) {
    // 'submenu' is a menu but it should not be open
    checkOpen("trigger", testname);
    checkClosed("submenu", testname);
    checkActive(gMenuPopup, "submenu", testname);
  }
},
{
  // open the submenu with the cursor right key
  testname: "open submenu with cursor right",
  events: [ "popupshowing submenupopup", "DOMMenuItemActive submenuitem",
            "popupshown submenupopup" ],
  test() { synthesizeKey("KEY_ArrowRight"); },
  result(testname) {
    checkOpen("trigger", testname);
    checkOpen("submenu", testname);
    checkActive(gMenuPopup, "submenu", testname);
    checkActive(document.getElementById("submenupopup"), "submenuitem", testname);
  }
},
{
  // close the submenu with the cursor left key
  testname: "close submenu with cursor left",
  events: [ "popuphiding submenupopup", "popuphidden submenupopup",
            "DOMMenuItemInactive submenuitem", "DOMMenuInactive submenupopup",
            "DOMMenuItemActive submenu" ],
  test() { synthesizeKey("KEY_ArrowLeft"); },
  result(testname) {
    checkOpen("trigger", testname);
    checkClosed("submenu", testname);
    checkActive(gMenuPopup, "submenu", testname);
    checkActive(document.getElementById("submenupopup"), "", testname);
  }
},
{
  // open the submenu with the enter key
  testname: "open submenu with enter",
  events: [ "popupshowing submenupopup", "DOMMenuItemActive submenuitem",
            "popupshown submenupopup" ],
  test() { synthesizeKey("KEY_Enter"); },
  result(testname) {
    checkOpen("trigger", testname);
    checkOpen("submenu", testname);
    checkActive(gMenuPopup, "submenu", testname);
    checkActive(document.getElementById("submenupopup"), "submenuitem", testname);
  }
},
{
  // close the submenu with the escape key
  testname: "close submenu with escape",
  events: [ "popuphiding submenupopup", "popuphidden submenupopup",
            "DOMMenuItemInactive submenuitem", "DOMMenuInactive submenupopup",
            "DOMMenuItemActive submenu" ],
  test() { synthesizeKey("KEY_Escape"); },
  result(testname) {
    checkOpen("trigger", testname);
    checkClosed("submenu", testname);
    checkActive(gMenuPopup, "submenu", testname);
    checkActive(document.getElementById("submenupopup"), "", testname);
  }
},
{
  // pressing the letter again when the next item is disabled should still
  // select the disabled item on Windows, but select the next item on other
  // platforms
  testname: "menuitem with non accelerator disabled",
  events() {
    if (navigator.platform.indexOf("Win") == 0) {
      return [ "DOMMenuItemInactive submenu", "DOMMenuItemActive other",
               "DOMMenuItemInactive other", "DOMMenuItemActive item1" ];
    }
    return [ "DOMMenuItemInactive submenu", "DOMMenuItemActive last",
             "DOMMenuItemInactive last", "DOMMenuItemActive item1" ];
  },
  test() { sendString("OF"); },
  result(testname) {
    checkActive(gMenuPopup, "item1", testname);
  }
},
{
  // pressing a letter that doesn't correspond to an accelerator nor the
  // first letter of a menu. This should have no effect.
  testname: "menuitem with keypress no accelerator found",
  test() { sendString("G"); },
  result(testname) {
    checkOpen("trigger", testname);
    checkActive(gMenuPopup, "item1", testname);
  }
},
{
  // when only one menuitem starting with that letter exists, it should be
  // selected and the menu closed
  testname: "menuitem with non accelerator single",
  events: [ "DOMMenuItemInactive item1", "DOMMenuItemActive amenu",
            "DOMMenuItemInactive amenu", "DOMMenuInactive thepopup",
            "command amenu", "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuItemInactive amenu",
           ],
  test() { sendString("M"); },
  result(testname) {
    checkClosed("trigger", testname);
    checkActive(gMenuPopup, "", testname);
  }
},
{
  testname: "open context popup at screen with all modifiers set",
  events: [ "popupshowing thepopup 1111", "popupshown thepopup" ],
  autohide: "thepopup",
  test(testname, step) {
    gMenuPopup.openPopupAtScreen(gScreenX + 8, gScreenY + 16, true, gCachedEvent2);
  }
},
{
  testname: "open popup with open property",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test(testname, step) { openMenu(gTrigger); },
  result(testname, step) {
    checkOpen("trigger", testname);
    if (gIsMenu)
      compareEdge(gTrigger, gMenuPopup, "after_start", 0, 0, testname);
  }
},
{
  testname: "open submenu with open property",
  events: [ "popupshowing submenupopup", "DOMMenuItemActive submenu",
            "popupshown submenupopup" ],
  test(testname, step) { openMenu(document.getElementById("submenu")); },
  result(testname, step) {
    checkOpen("trigger", testname);
    checkOpen("submenu", testname);
    // XXXndeakin
    // getBoundingClientRect doesn't seem to working right for submenus
    // so disable this test for now
    // compareEdge(document.getElementById("submenu"),
    //             document.getElementById("submenupopup"), "end_before", 0, 0, testname);
  }
},
{
  testname: "hidePopup hides entire chain",
  events: [ "popuphiding submenupopup", "popuphidden submenupopup",
            "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuInactive submenupopup",
            "DOMMenuItemInactive submenu", "DOMMenuItemInactive submenu",
            "DOMMenuInactive thepopup", ],
  test() { gMenuPopup.hidePopup(); },
  result(testname, step) {
    checkClosed("trigger", testname);
    checkClosed("submenu", testname);
  }
},
{
  testname: "open submenu with open property without parent open",
  test(testname, step) { openMenu(document.getElementById("submenu")); },
  result(testname, step) {
    checkClosed("trigger", testname);
    checkClosed("submenu", testname);
  }
},
{
  testname: "open popup with open property and position",
  condition() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test(testname, step) {
    gMenuPopup.setAttribute("position", "before_start");
    openMenu(gTrigger);
  },
  result(testname, step) {
    compareEdge(gTrigger, gMenuPopup, "before_start", 0, 0, testname);
  }
},
{
  testname: "close popup with open property",
  condition() { return gIsMenu; },
  events: [ "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuInactive thepopup" ],
  test(testname, step) { closeMenu(gTrigger, gMenuPopup); },
  result(testname, step) { checkClosed("trigger", testname); }
},
{
  testname: "open popup with open property, position, anchor and alignment",
  condition() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test(testname, step) {
    gMenuPopup.setAttribute("position", "start_after");
    gMenuPopup.setAttribute("popupanchor", "topright");
    gMenuPopup.setAttribute("popupalign", "bottomright");
    openMenu(gTrigger);
  },
  result(testname, step) {
    compareEdge(gTrigger, gMenuPopup, "start_after", 0, 0, testname);
  }
},
{
  testname: "open popup with open property, anchor and alignment",
  condition() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test(testname, step) {
    gMenuPopup.removeAttribute("position");
    gMenuPopup.setAttribute("popupanchor", "bottomright");
    gMenuPopup.setAttribute("popupalign", "topright");
    openMenu(gTrigger);
  },
  result(testname, step) {
    compareEdge(gTrigger, gMenuPopup, "after_end", 0, 0, testname);
    gMenuPopup.removeAttribute("popupanchor");
    gMenuPopup.removeAttribute("popupalign");
  }
},
{
  testname: "focus and cursor down on trigger",
  condition() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test(testname, step) {
    gTrigger.focus();
    synthesizeKey("KEY_ArrowDown", {altKey: !platformIsMac()});
  },
  result(testname, step) {
    checkOpen("trigger", testname);
    checkActive(gMenuPopup, "", testname);
  }
},
{
  testname: "focus and cursor up on trigger",
  condition() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test(testname, step) {
    gTrigger.focus();
    synthesizeKey("KEY_ArrowUp", {altKey: !platformIsMac()});
  },
  result(testname, step) {
    checkOpen("trigger", testname);
    checkActive(gMenuPopup, "", testname);
  }
},
{
  testname: "select and enter on menuitem",
  condition() { return gIsMenu; },
  events: [ "DOMMenuItemActive item1", "DOMMenuItemInactive item1",
            "DOMMenuInactive thepopup", "command item1",
            "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuItemInactive item1" ],
  test(testname, step) {
    synthesizeKey("KEY_ArrowDown");
    synthesizeKey("KEY_Enter");
  },
  result(testname, step) { checkClosed("trigger", testname); }
},
{
  testname: "focus trigger and key to open",
  condition() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test(testname, step) {
    gTrigger.focus();
    synthesizeKey(platformIsMac() ? " " : "KEY_F4");
  },
  result(testname, step) {
    checkOpen("trigger", testname);
    checkActive(gMenuPopup, "", testname);
  }
},
{
  // the menu should only open when the meta or alt key is not pressed
  testname: "focus trigger and key wrong modifier",
  condition() { return gIsMenu; },
  test(testname, step) {
    gTrigger.focus();
    if (platformIsMac())
      synthesizeKey("KEY_F4", {altKey: true});
    else
      synthesizeKey("", { metaKey: true });
  },
  result(testname, step) {
    checkClosed("trigger", testname);
  }
},
{
  testname: "mouse click on disabled menu",
  condition() { return gIsMenu; },
  test(testname, step) {
    gTrigger.setAttribute("disabled", "true");
    synthesizeMouse(gTrigger, 4, 4, { });
  },
  result(testname, step) {
    checkClosed("trigger", testname);
    gTrigger.removeAttribute("disabled");
  }
},
{
  // openPopup using object as position argument
  testname: "openPopup with object argument",
  events: [ "popupshowing thepopup 0000", "popupshown thepopup" ],
  autohide: "thepopup",
  test(testname, step) {
    gMenuPopup.openPopup(gTrigger, { position: "before_start", x: 5, y: 7 });
    checkOpen("trigger", testname);
  },
  result(testname, step) {
    var triggerrect = gTrigger.getBoundingClientRect();
    var popuprect = gMenuPopup.getBoundingClientRect();
    is(Math.round(popuprect.left), Math.round(triggerrect.left + 5), testname + " x position ");
    is(Math.round(popuprect.bottom), Math.round(triggerrect.top + 7), testname + " y position ");
  }
},
{
  // openPopup using object as position argument with event
  testname: "openPopup with object argument with event",
  events: [ "popupshowing thepopup 1000", "popupshown thepopup" ],
  autohide: "thepopup",
  test(testname, step) {
    gMenuPopup.openPopup(gTrigger, { position: "after_start", x: 0, y: 0,
                                     triggerEvent: new MouseEvent("mousedown", { altKey: true })
                                    });
    checkOpen("trigger", testname);
  }
},
{
  // openPopup with no arguments
  testname: "openPopup with no arguments",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test(testname, step) {
    gMenuPopup.openPopup();
  },
  result(testname, step) {
    let isMenu = gTrigger.type == "menu";
    // With no arguments, open in default menu position
    var triggerrect = gTrigger.getBoundingClientRect();
    var popuprect = gMenuPopup.getBoundingClientRect();
    is(Math.round(popuprect.left), isMenu ? Math.round(triggerrect.left) : 0, testname + " x position ");
    is(Math.round(popuprect.top), isMenu ? Math.round(triggerrect.bottom) : 0, testname + " y position ");
  }
},
{
  // openPopup should open the menu synchronously, however popupshown
  // is fired asynchronously
  testname: "openPopup synchronous",
  events: [ "popupshowing thepopup", "popupshowing submenupopup",
            "popupshown thepopup", "DOMMenuItemActive submenu",
            "popupshown submenupopup" ],
  test(testname, step) {
    gMenuPopup.openPopup(gTrigger, "after_start", 0, 0, false, true);
    document.getElementById("submenupopup").
      openPopup(gTrigger, "end_before", 0, 0, false, true);
    checkOpen("trigger", testname);
    checkOpen("submenu", testname);
  }
},
{
  // remove the content nodes for the popup
  testname: "remove content",
  test(testname, step) {
    var submenupopup = document.getElementById("submenupopup");
    submenupopup.remove();
    var popup = document.getElementById("thepopup");
    popup.remove();
  }
}

];

function platformIsMac() {
    return navigator.platform.indexOf("Mac") > -1;
}
