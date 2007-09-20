var gMenuPopup = null;
var gTrigger = null;
var gIsMenu = false;
var gScreenX = -1, gScreenY = -1;

function runTests()
{
  gMenuPopup = document.getElementById("thepopup");
  gTrigger = document.getElementById("trigger");

  gIsMenu = gTrigger.boxObject instanceof Components.interfaces.nsIMenuBoxObject;

  var mouseFn = function(event) {
    gScreenX = event.screenX;
    gScreenY = event.screenY;
  }

  // a hacky way to get the screen position of the document
  window.addEventListener("mousedown", mouseFn, false);
  synthesizeMouse(document.documentElement, 0, 0, { });
  window.removeEventListener("mousedown", mouseFn, false);
  startPopupTests(popupTests);
}

var popupTests = [
{
  testname: "mouse click on trigger",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test: function() { synthesizeMouse(gTrigger, 4, 4, { }); },
  result: function (testname) {
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
  test: function() { synthesizeKey("VK_DOWN", { }); },
  result: function(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // check that pressing cursor up wraps and highlights the last item
  testname: "cursor up wrap",
  events: [ "DOMMenuItemInactive item1", "DOMMenuItemActive last" ],
  test: function() { synthesizeKey("VK_UP", { }); },
  result: function(testname) {
    checkActive(gMenuPopup, "last", testname);
  }
},
{
  // check that pressing cursor down wraps and highlights the first item
  testname: "cursor down wrap",
  events: [ "DOMMenuItemInactive last", "DOMMenuItemActive item1" ],
  test: function() { synthesizeKey("VK_DOWN", { }); },
  result: function(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // check that pressing cursor down highlights the second item
  testname: "cursor down",
  events: [ "DOMMenuItemInactive item1", "DOMMenuItemActive item2" ],
  test: function() { synthesizeKey("VK_DOWN", { }); },
  result: function(testname) { checkActive(gMenuPopup, "item2", testname); }
},
{
  // check that pressing cursor up highlights the second item
  testname: "cursor up",
  events: [ "DOMMenuItemInactive item2", "DOMMenuItemActive item1" ],
  test: function() { synthesizeKey("VK_UP", { }); },
  result: function(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // cursor left should not do anything
  testname: "cursor left",
  test: function() { synthesizeKey("VK_LEFT", { }); },
  result: function(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // cursor right should not do anything
  testname: "cursor right",
  test: function() { synthesizeKey("VK_RIGHT", { }); },
  result: function(testname) { checkActive(gMenuPopup, "item1", testname); }
},
{
  // check cursor down when a disabled item exists in the menu
  testname: "cursor down disabled",
  events: function() {
    // On Windows, disabled items are included when navigating, but on
    // other platforms, disabled items are skipped over
    if (navigator.platform.indexOf("Win") == 0)
      return [ "DOMMenuItemInactive item1", "DOMMenuItemActive item2" ];
    else
      return [ "DOMMenuItemInactive item1", "DOMMenuItemActive amenu" ];
  },
  test: function() {
    document.getElementById("item2").disabled = true;
    synthesizeKey("VK_DOWN", { });
  }
},
{
  // check cursor up when a disabled item exists in the menu
  testname: "cursor up disabled",
  events: function() {
    if (navigator.platform.indexOf("Win") == 0)
      return [ "DOMMenuItemInactive item2", "DOMMenuItemActive amenu",
               "DOMMenuItemInactive amenu", "DOMMenuItemActive item2",
               "DOMMenuItemInactive item2", "DOMMenuItemActive item1" ];
    else
      return [ "DOMMenuItemInactive amenu", "DOMMenuItemActive item1" ];
  },
  test: function() {
    if (navigator.platform.indexOf("Win") == 0)
      synthesizeKey("VK_DOWN", { });
    synthesizeKey("VK_UP", { });
    if (navigator.platform.indexOf("Win") == 0)
      synthesizeKey("VK_UP", { });
  }
},
{
  testname: "mouse click outside",
  events: [ "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuItemInactive item1", "DOMMenuInactive thepopup" ],
  test: function() {
    gMenuPopup.hidePopup();
    // XXXndeakin event simulation fires events outside of the platform specific
    // widget code so the popup capturing isn't handled. Thus, the menu won't
    // rollup this way.
    // synthesizeMouse(gTrigger, 0, -12, { });
  },
  result: function(testname, step) { checkClosed("trigger", testname); }
},
{
  // these tests check to ensure that passing an anchor and position
  // puts the popup in the right place
  testname: "open popup anchored",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  steps: ["before_start", "before_end", "after_start", "after_end",
          "start_before", "start_after", "end_before", "end_after", "overlap"],
  test: function(testname, step) { gMenuPopup.openPopup(gTrigger, step, 0, 0, false, false); },
  result: function(testname, step) { compareEdge(gTrigger, gMenuPopup, step, 0, 0, testname); }
},
{
  // these tests check to ensure that the position attribute can be used
  // to set the position of a popup instead of passing it as an argument
  testname: "open popup anchored with attribute",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  steps: ["before_start", "before_end", "after_start", "after_end",
          "start_before", "start_after", "end_before", "end_after", "overlap"],
  test: function(testname, step) {
    gMenuPopup.setAttribute("position", step);
    gMenuPopup.openPopup(gTrigger, "", 0, 0, false, false);
  },
  result: function(testname, step) { compareEdge(gTrigger, gMenuPopup, step, 0, 0, testname); }
},
{
  // this test checks to ensure that attributes override flag to openPopup
  // can be used to override the popup's position
  testname: "open popup anchored with override",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test: function(testname, step) {
    // attribute overrides the position passed in
    gMenuPopup.setAttribute("position", "end_after");
    gMenuPopup.openPopup(gTrigger, "before_start", 0, 0, false, true);
  },
  result: function(testname, step) { compareEdge(gTrigger, gMenuPopup, "end_after", 0, 0, testname); }
},
{
  testname: "close popup with escape",
  events: [ "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuInactive thepopup", ],
  test: function(testname, step) {
    synthesizeKey("VK_ESCAPE", { });
    checkClosed("trigger", testname);
  }
},
{
  // check that offsets may be supplied to the openPopup method
  testname: "open popup anchored with offsets",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test: function(testname, step) {
    // attribute is empty so does not override
    gMenuPopup.setAttribute("position", "");
    gMenuPopup.openPopup(gTrigger, "before_start", 5, 10, true, true);
  },
  result: function(testname, step) { compareEdge(gTrigger, gMenuPopup, "before_start", 5, 10, testname); }
},
{
  // these tests check to ensure that passing an anchor and position
  // puts the popup in the right place
  testname: "show popup anchored",
  condition: function() {
    // only perform this test for popups not in a menu, such as those using
    // the popup attribute, as the showPopup implementation in popup.xml
    // calls openMenu if the popup is inside a menu
    return !gIsMenu;
  },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  steps: [["topleft", "topleft"],
          ["topleft", "topright"], ["topleft", "bottomleft"],
          ["topright", "topleft"], ["topright", "bottomright"],
          ["bottomleft", "bottomright"], ["bottomleft", "topleft"],
          ["bottomright", "bottomleft"], ["bottomright", "topright"]],
  test: function(testname, step) {
    // the attributes should be ignored
    gMenuPopup.setAttribute("popupanchor", "topright");
    gMenuPopup.setAttribute("popupalign", "bottomright");
    gMenuPopup.setAttribute("position", "end_after");
    gMenuPopup.showPopup(gTrigger, -1, -1, "popup", step[0], step[1]);
  },
  result: function(testname, step) {
    var pos = convertPosition(step[0], step[1]);
    compareEdge(gTrigger, gMenuPopup, pos, 0, 0, testname);
    gMenuPopup.removeAttribute("popupanchor");
    gMenuPopup.removeAttribute("popupalign");
    gMenuPopup.removeAttribute("position");
  }
},
{
  testname: "show popup with position",
  condition: function() { return !gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test: function(testname, step) {
    gMenuPopup.showPopup(gTrigger, gScreenX + 60, gScreenY + 15,
                         "context", "topleft", "bottomright");
  },
  result: function(testname, step) {
    var rect = gMenuPopup.getBoundingClientRect();
    ok(rect.left == 60 && rect.top == 15 && rect.right && rect.bottom, testname);
  }
},
{
  // if no anchor is supplied to openPopup, it should be opened relative
  // to the viewport.
  testname: "open popup unanchored",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test: function(testname, step) { gMenuPopup.openPopup(null, "after_start", 6, 8, false); },
  result: function(testname, step) {
    var rect = gMenuPopup.getBoundingClientRect();
    ok(rect.left == 6 && rect.top == 8 && rect.right && rect.bottom, testname);
  }
},
{
  testname: "activate menuitem with mouse",
  events: [ "DOMMenuInactive thepopup", "command item3",
            "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuItemInactive item3" ],
  test: function(testname, step) {
    var item3 = document.getElementById("item3");
    synthesizeMouse(item3, 4, 4, { });
  },
  result: function(testname, step) { checkClosed("trigger", testname); }
},
{
  testname: "close popup",
  condition: function() { return false; },
  events: [ "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuInactive thepopup" ],
  test: function(testname, step) { gMenuPopup.hidePopup(); }
},
{
  testname: "open popup at screen",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test: function(testname, step) {
    gMenuPopup.openPopupAtScreen(gScreenX + 24, gScreenY + 20, false);
  },
  result: function(testname, step) {
    var rect = gMenuPopup.getBoundingClientRect();
    ok(rect.left == 24 && rect.top == 20 && rect.right && rect.bottom, testname);
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
  test: function() { synthesizeKey("M", { }); },
  result: function(testname) { checkClosed("trigger", testname); }
},
{
  testname: "open context popup at screen",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test: function(testname, step) {
    gMenuPopup.openPopupAtScreen(gScreenX + 8, gScreenY + 16, false);
  },
  result: function(testname, step) {
    var rect = gMenuPopup.getBoundingClientRect();
    ok(rect.left == 8 && rect.top == 16 && rect.right && rect.bottom, testname);
  }
},
{
  // pressing a letter that doesn't correspond to an accelerator, but does
  // correspond to the first letter in a menu's label. The menu should not
  // close because there is more than one item corresponding to that letter
  testname: "menuitem with non accelerator",
  events: [ "DOMMenuItemActive one" ],
  test: function() { synthesizeKey("O", { }); },
  result: function(testname) {
    checkOpen("trigger", testname);
    checkActive(gMenuPopup, "one", testname);
  }
},
{
  // pressing the letter again should select the next one that starts with
  // that letter
  testname: "menuitem with non accelerator again",
  events: [ "DOMMenuItemInactive one", "DOMMenuItemActive submenu" ],
  test: function() { synthesizeKey("O", { }); },
  result: function(testname) {
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
  test: function() { synthesizeKey("VK_RIGHT", { }); },
  result: function(testname) {
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
  test: function() { synthesizeKey("VK_LEFT", { }); },
  result: function(testname) {
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
  test: function() { synthesizeKey("VK_ENTER", { }); },
  result: function(testname) {
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
  test: function() { synthesizeKey("VK_ESCAPE", { }); },
  result: function(testname) {
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
  events: function() {
    if (navigator.platform.indexOf("Win") == 0)
      return [ "DOMMenuItemInactive submenu", "DOMMenuItemActive other",
               "DOMMenuItemInactive other", "DOMMenuItemActive item1" ];
    else
      return [ "DOMMenuItemInactive submenu", "DOMMenuItemActive last",
               "DOMMenuItemInactive last", "DOMMenuItemActive item1" ];
  },
  test: function() { synthesizeKey("O", { }); synthesizeKey("F", { }); },
  result: function(testname) {
    checkActive(gMenuPopup, "item1", testname);
  }
},
{
  // pressing a letter that doesn't correspond to an accelerator nor the
  // first letter of a menu. This should have no effect.
  testname: "menuitem with keypress no accelerator found",
  test: function() { synthesizeKey("G", { }); },
  result: function(testname) {
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
  test: function() { synthesizeKey("M", { }); },
  result: function(testname) {
    checkClosed("trigger", testname);
    checkActive(gMenuPopup, "", testname);
  }
},
{
  testname: "open popup with open property",
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test: function(testname, step) { openMenu(gTrigger); },
  result: function(testname, step) {
    checkOpen("trigger", testname);
    if (gIsMenu)
      compareEdge(gTrigger, gMenuPopup, "after_start", 0, 0, testname);
  }
},
{
  testname: "open submenu with open property",
  events: [ "popupshowing submenupopup", "DOMMenuItemActive submenu",
            "popupshown submenupopup" ],
  test: function(testname, step) { openMenu(document.getElementById("submenu")); },
  result: function(testname, step) {
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
  end: true,
  testname: "hidePopup hides entire chain",
  events: [ "popuphiding submenupopup", "popuphidden submenupopup",
            "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuInactive submenupopup",
            "DOMMenuItemInactive submenu", "DOMMenuItemInactive submenu",
            "DOMMenuInactive thepopup", ],
  test: function() { gMenuPopup.hidePopup(); },
  result: function(testname, step) {
    checkClosed("trigger", testname);
    checkClosed("submenu", testname);
  }
},
{
  testname: "open submenu with open property without parent open",
  test: function(testname, step) { openMenu(document.getElementById("submenu")); },
  result: function(testname, step) {
    checkClosed("trigger", testname);
    checkClosed("submenu", testname);
  }
},
{
  testname: "open popup with open property and position",
  condition: function() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test: function(testname, step) {
    gMenuPopup.setAttribute("position", "before_start");
    openMenu(gTrigger);
  },
  result: function(testname, step) {
    compareEdge(gTrigger, gMenuPopup, "before_start", 0, 0, testname);
  }
},
{
  testname: "close popup with open property",
  condition: function() { return gIsMenu; },
  events: [ "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuInactive thepopup" ],
  test: function(testname, step) { closeMenu(gTrigger, gMenuPopup); },
  result: function(testname, step) { checkClosed("trigger", testname); }
},
{
  testname: "open popup with open property, position, anchor and alignment",
  condition: function() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test: function(testname, step) {
    gMenuPopup.setAttribute("position", "start_after");
    gMenuPopup.setAttribute("popupanchor", "topright");
    gMenuPopup.setAttribute("popupalign", "bottomright");
    openMenu(gTrigger);
  },
  result: function(testname, step) {
    compareEdge(gTrigger, gMenuPopup, "start_after", 0, 0, testname);
  }
},
{
  testname: "open popup with open property, anchor and alignment",
  condition: function() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test: function(testname, step) {
    gMenuPopup.removeAttribute("position");
    gMenuPopup.setAttribute("popupanchor", "bottomright");
    gMenuPopup.setAttribute("popupalign", "topright");
    openMenu(gTrigger);
  },
  result: function(testname, step) {
    compareEdge(gTrigger, gMenuPopup, "after_end", 0, 0, testname);
    gMenuPopup.removeAttribute("popupanchor");
    gMenuPopup.removeAttribute("popupalign");
  }
},
{
  testname: "focus and cursor down on trigger",
  condition: function() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test: function(testname, step) {
    gTrigger.focus();
    synthesizeKey("VK_DOWN", { altKey: (navigator.platform.indexOf("Mac") == -1) });
  },
  result: function(testname, step) {
    checkOpen("trigger", testname);
    checkActive(gMenuPopup, "", testname);
  }
},
{
  testname: "focus and cursor up on trigger",
  condition: function() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  test: function(testname, step) {
    gTrigger.focus();
    synthesizeKey("VK_UP", { altKey: (navigator.platform.indexOf("Mac") == -1) });
  },
  result: function(testname, step) {
    checkOpen("trigger", testname);
    checkActive(gMenuPopup, "", testname);
  }
},
{
  testname: "select and enter on menuitem",
  condition: function() { return gIsMenu; },
  events: [ "DOMMenuItemActive item1", "DOMMenuItemInactive item1",
            "DOMMenuInactive thepopup", "command item1",
            "popuphiding thepopup", "popuphidden thepopup",
            "DOMMenuItemInactive item1" ],
  test: function(testname, step) {
    synthesizeKey("VK_DOWN", { });
    synthesizeKey("VK_ENTER", { });
  },
  result: function(testname, step) { checkClosed("trigger", testname); }
},
{
  testname: "focus trigger and key to open",
  condition: function() { return gIsMenu; },
  events: [ "popupshowing thepopup", "popupshown thepopup" ],
  autohide: "thepopup",
  test: function(testname, step) {
    gTrigger.focus();
    synthesizeKey((navigator.platform.indexOf("Mac") == -1) ? "VK_F4" : " ", { });
  },
  result: function(testname, step) {
    checkOpen("trigger", testname);
    checkActive(gMenuPopup, "", testname);
  }
},
{
  // the menu should only open when the meta or alt key is not pressed
  testname: "focus trigger and key wrong modifier",
  condition: function() { return gIsMenu; },
  test: function(testname, step) {
    gTrigger.focus();
    if (navigator.platform.indexOf("Mac") == -1)
      synthesizeKey("", { metaKey: true });
    else
      synthesizeKey("VK_F4", { altKey: true });
  },
  result: function(testname, step) {
    checkClosed("trigger", testname);
  }
},
{
  testname: "mouse click on disabled menu",
  condition: function() { return gIsMenu; },
  test: function(testname, step) {
    gTrigger.setAttribute("disabled", "true");
    synthesizeMouse(gTrigger, 4, 4, { });
  },
  result: function(testname, step) {
    checkClosed("trigger", testname);
    gTrigger.removeAttribute("disabled");
  }
},
{
  // openPopup should open the menu synchronously, however popupshown
  // is fired asynchronously
  testname: "openPopup synchronous",
  events: [ "popupshowing thepopup", "popupshowing submenupopup",
            "popupshown thepopup", "DOMMenuItemActive submenu",
            "popupshown submenupopup" ],
  test: function(testname, step) {
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
  test: function(testname, step) {
    var submenupopup = document.getElementById("submenupopup");
    submenupopup.parentNode.removeChild(submenupopup);
    var popup = document.getElementById("thepopup");
    popup.parentNode.removeChild(popup);
  }
},

];
