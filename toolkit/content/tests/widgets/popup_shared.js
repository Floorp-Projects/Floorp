/*
 * This script is used for menu and popup tests. Call startPopupTests to start
 * the tests, passing an array of tests as an argument. Each test is an object
 * with the following properties:
 *   testname - name of the test
 *   test - function to call to perform the test
 *   events - a list of events that are expected to be fired in sequence
 *            as a result of calling the 'test' function. This list should be
 *            an array of strings of the form "eventtype targetid" where
 *            'eventtype' is the event type and 'targetid' is the id of
 *            target of the event. This function will be passed two
 *            arguments, the testname and the step argument.
 *            Alternatively, events may be a function which returns the array
 *            of events. This can be used when the events vary per platform.
 *   result - function to call after all the events have fired to check
 *            for additional results. May be null. This function will be
 *            passed two arguments, the testname and the step argument.
 *   steps - optional array of values. The test will be repeated for
 *           each step, passing each successive value within the array to
 *           the test and result functions
 *   autohide - if set, should be set to the id of a popup to hide after
 *              the test is complete. This is a convenience for some tests.
 *   condition - an optional function which, if it returns false, causes the
 *               test to be skipped.
 *   end - used for debugging. Set to true to stop the tests after running
 *         this one.
 */

const menuactiveAttribute = "_moz-menuactive";

var gPopupTests = null;
var gTestIndex = -1;
var gTestStepIndex = 0;
var gTestEventIndex = 0;
var gAutoHide = false;
var gExpectedEventDetails = null;
var gExpectedTriggerNode = null;
var gWindowUtils;
var gPopupWidth = -1, gPopupHeight = -1;

function startPopupTests(tests) {
  document.addEventListener("popupshowing", eventOccurred, false);
  document.addEventListener("popupshown", eventOccurred, false);
  document.addEventListener("popuphiding", eventOccurred, false);
  document.addEventListener("popuphidden", eventOccurred, false);
  document.addEventListener("command", eventOccurred, false);
  document.addEventListener("DOMMenuItemActive", eventOccurred, false);
  document.addEventListener("DOMMenuItemInactive", eventOccurred, false);
  document.addEventListener("DOMMenuInactive", eventOccurred, false);
  document.addEventListener("DOMMenuBarActive", eventOccurred, false);
  document.addEventListener("DOMMenuBarInactive", eventOccurred, false);

  gPopupTests = tests;
  gWindowUtils = SpecialPowers.getDOMWindowUtils(window);

  goNext();
}

function finish() {
  if (window.opener) {
    window.close();
    window.opener.SimpleTest.finish();
    return;
  }
  SimpleTest.finish();
}

function ok(condition, message) {
  if (window.opener)
    window.opener.SimpleTest.ok(condition, message);
  else
    SimpleTest.ok(condition, message);
}

function is(left, right, message) {
  if (window.opener)
    window.opener.SimpleTest.is(left, right, message);
  else
    SimpleTest.is(left, right, message);
}

function disableNonTestMouse(aDisable) {
  gWindowUtils.disableNonTestMouseEvents(aDisable);
}

function eventOccurred(event) {
  if (gPopupTests.length <= gTestIndex) {
    ok(false, "Extra " + event.type + " event fired");
    return;
  }

  var test = gPopupTests[gTestIndex];
  if ("autohide" in test && gAutoHide) {
    if (event.type == "DOMMenuInactive") {
      gAutoHide = false;
      setTimeout(goNextStep, 0);
    }
    return;
  }

  var events = test.events;
  if (typeof events == "function")
    events = events();
  if (events) {
    if (events.length <= gTestEventIndex) {
      ok(false, "Extra " + event.type + " event fired for " + event.target.id +
                  " " + gPopupTests[gTestIndex].testname);
      return;
    }

    var eventitem = events[gTestEventIndex].split(" ");
    var matches;
    if (eventitem[1] == "#tooltip") {
      is(event.originalTarget.localName, "tooltip",
         test.testname + " event.originalTarget.localName is 'tooltip'");
      is(event.originalTarget.getAttribute("default"), "true",
         test.testname + " event.originalTarget default attribute is 'true'");
      matches = event.originalTarget.localName == "tooltip" &&
          event.originalTarget.getAttribute("default") == "true";
    } else {
      is(event.type, eventitem[0],
         test.testname + " event type " + event.type + " fired");
      is(event.target.id, eventitem[1],
         test.testname + " event target ID " + event.target.id);
      matches = eventitem[0] == event.type && eventitem[1] == event.target.id;
    }

    var modifiersMask = eventitem[2];
    if (modifiersMask) {
      var m = "";
      m += event.altKey ? '1' : '0';
      m += event.ctrlKey ? '1' : '0';
      m += event.shiftKey ? '1' : '0';
      m += event.metaKey ? '1' : '0';
      is(m, modifiersMask, test.testname + " modifiers mask matches");
    }

    var expectedState;
    switch (event.type) {
      case "popupshowing": expectedState = "showing"; break;
      case "popupshown": expectedState = "open"; break;
      case "popuphiding": expectedState = "hiding"; break;
      case "popuphidden": expectedState = "closed"; break;
    }

    if (gExpectedTriggerNode && event.type == "popupshowing") {
      if (gExpectedTriggerNode == "notset") // check against null instead
        gExpectedTriggerNode = null;

      is(event.originalTarget.triggerNode, gExpectedTriggerNode, test.testname + " popupshowing triggerNode");
      var isTooltip = (event.target.localName == "tooltip");
      is(document.popupNode, isTooltip ? null : gExpectedTriggerNode,
         test.testname + " popupshowing document.popupNode");
      is(document.tooltipNode, isTooltip ? gExpectedTriggerNode : null,
         test.testname + " popupshowing document.tooltipNode");
    }

    if (expectedState)
      is(event.originalTarget.state, expectedState,
         test.testname + " " + event.type + " state");

    if (matches) {
      gTestEventIndex++
      if (events.length <= gTestEventIndex)
        setTimeout(checkResult, 0);
    }
  }
}

function checkResult() {
  var step = null;
  var test = gPopupTests[gTestIndex];
  if ("steps" in test)
    step = test.steps[gTestStepIndex];

  if ("result" in test)
    test.result(test.testname, step);

  if ("autohide" in test) {
    gAutoHide = true;
    document.getElementById(test.autohide).hidePopup();
    return;
  }

  goNextStep();
}

function goNextStep() {
  gTestEventIndex = 0;

  var step = null;
  var test = gPopupTests[gTestIndex];
  if ("steps" in test) {
    gTestStepIndex++;
    step = test.steps[gTestStepIndex];
    if (gTestStepIndex < test.steps.length) {
      test.test(test.testname, step);
      return;
    }
  }

  goNext();
}

function goNext() {
  // We want to continue after the next animation frame so that
  // we're in a stable state and don't get spurious mouse events at unexpected targets.
  window.requestAnimationFrame(
    function() {
      setTimeout(goNextStepSync, 0);
    }
  );
}

function goNextStepSync() {
  if (gTestIndex >= 0 && "end" in gPopupTests[gTestIndex] && gPopupTests[gTestIndex].end) {
    finish();
    return;
  }

  gTestIndex++;
  gTestStepIndex = 0;
  if (gTestIndex < gPopupTests.length) {
    var test = gPopupTests[gTestIndex];
    // Set the location hash so it's easy to see which test is running
    document.location.hash = test.testname;

    // skip the test if the condition returns false
    if ("condition" in test && !test.condition()) {
      goNext();
      return;
    }

    // start with the first step if there are any
    var step = null;
    if ("steps" in test)
      step = test.steps[gTestStepIndex];

    test.test(test.testname, step);

    // no events to check for so just check the result
    if (!("events" in test)) {
      checkResult();
    } else if (typeof test.events == "function" && !test.events().length) {
      checkResult();
    }
  } else {
    finish();
  }
}

function openMenu(menu) {
  if ("open" in menu) {
    menu.open = true;
  } else {
    var bo = menu.boxObject;
    if (bo instanceof MenuBoxObject)
      bo.openMenu(true);
    else
      synthesizeMouse(menu, 4, 4, { });
  }
}

function closeMenu(menu, popup) {
  if ("open" in menu) {
    menu.open = false;
  } else {
    var bo = menu.boxObject;
    if (bo instanceof MenuBoxObject)
      bo.openMenu(false);
    else
      popup.hidePopup();
  }
}

function checkActive(popup, id, testname) {
  var activeok = true;
  var children = popup.childNodes;
  for (var c = 0; c < children.length; c++) {
    var child = children[c];
    if ((id == child.id && child.getAttribute(menuactiveAttribute) != "true") ||
        (id != child.id && child.hasAttribute(menuactiveAttribute) != "")) {
      activeok = false;
      break;
    }
  }
  ok(activeok, testname + " item " + (id ? id : "none") + " active");
}

function checkOpen(menuid, testname) {
  var menu = document.getElementById(menuid);
  if ("open" in menu)
    ok(menu.open, testname + " " + menuid + " menu is open");
  else if (menu.boxObject instanceof MenuBoxObject)
    ok(menu.getAttribute("open") == "true", testname + " " + menuid + " menu is open");
}

function checkClosed(menuid, testname) {
  var menu = document.getElementById(menuid);
  if ("open" in menu)
    ok(!menu.open, testname + " " + menuid + " menu is open");
  else if (menu.boxObject instanceof MenuBoxObject)
    ok(!menu.hasAttribute("open"), testname + " " + menuid + " menu is closed");
}

function convertPosition(anchor, align) {
  if (anchor == "topleft" && align == "topleft") return "overlap";
  if (anchor == "topleft" && align == "topright") return "start_before";
  if (anchor == "topleft" && align == "bottomleft") return "before_start";
  if (anchor == "topright" && align == "topleft") return "end_before";
  if (anchor == "topright" && align == "bottomright") return "before_end";
  if (anchor == "bottomleft" && align == "bottomright") return "start_after";
  if (anchor == "bottomleft" && align == "topleft") return "after_start";
  if (anchor == "bottomright" && align == "bottomleft") return "end_after";
  if (anchor == "bottomright" && align == "topright") return "after_end";
  return "";
}

/*
 * When checking position of the bottom or right edge of the popup's rect,
 * use this instead of strict equality check of rounded values,
 * because we snap the top/left edges to pixel boundaries,
 * which can shift the bottom/right up to 0.5px from its "ideal" location,
 * and could cause it to round differently. (See bug 622507.)
 */
function isWithinHalfPixel(a, b) {
  return Math.abs(a - b) <= 0.5;
}

function compareEdge(anchor, popup, edge, offsetX, offsetY, testname) {
  testname += " " + edge;

  checkOpen(anchor.id, testname);

  var anchorrect = anchor.getBoundingClientRect();
  var popuprect = popup.getBoundingClientRect();
  var check1 = false, check2 = false;

  if (gPopupWidth == -1) {
    ok((Math.round(popuprect.right) - Math.round(popuprect.left)) &&
       (Math.round(popuprect.bottom) - Math.round(popuprect.top)),
       testname + " size");
  } else {
    is(Math.round(popuprect.width), gPopupWidth, testname + " width");
    is(Math.round(popuprect.height), gPopupHeight, testname + " height");
  }

  var spaceIdx = edge.indexOf(" ");
  if (spaceIdx > 0) {
    let cornerX, cornerY;
    let [position, align] = edge.split(" ");
    switch (position) {
      case "topleft": cornerX = anchorrect.left; cornerY = anchorrect.top; break;
      case "topcenter": cornerX = anchorrect.left + anchorrect.width / 2; cornerY = anchorrect.top; break;
      case "topright": cornerX = anchorrect.right; cornerY = anchorrect.top; break;
      case "leftcenter": cornerX = anchorrect.left; cornerY = anchorrect.top + anchorrect.height / 2; break;
      case "rightcenter": cornerX = anchorrect.right; cornerY = anchorrect.top + anchorrect.height / 2; break;
      case "bottomleft": cornerX = anchorrect.left; cornerY = anchorrect.bottom; break;
      case "bottomcenter": cornerX = anchorrect.left + anchorrect.width / 2; cornerY = anchorrect.bottom; break;
      case "bottomright": cornerX = anchorrect.right; cornerY = anchorrect.bottom; break;
    }

    switch (align) {
      case "topleft": cornerX += offsetX; cornerY += offsetY; break;
      case "topright": cornerX += -popuprect.width + offsetX; cornerY += offsetY; break;
      case "bottomleft": cornerX += offsetX; cornerY += -popuprect.height + offsetY; break;
      case "bottomright": cornerX += -popuprect.width + offsetX; cornerY += -popuprect.height + offsetY; break;
    }

    is(Math.round(popuprect.left), Math.round(cornerX), testname + " x position");
    is(Math.round(popuprect.top), Math.round(cornerY), testname + " y position");
    return;
  }

  if (edge == "after_pointer") {
    is(Math.round(popuprect.left), Math.round(anchorrect.left) + offsetX, testname + " x position");
    is(Math.round(popuprect.top), Math.round(anchorrect.top) + offsetY + 21, testname + " y position");
    return;
  }

  if (edge == "overlap") {
    ok(Math.round(anchorrect.left) + offsetY == Math.round(popuprect.left) &&
       Math.round(anchorrect.top) + offsetY == Math.round(popuprect.top),
       testname + " position");
    return;
  }

  if (edge.indexOf("before") == 0)
    check1 = isWithinHalfPixel(anchorrect.top + offsetY, popuprect.bottom);
  else if (edge.indexOf("after") == 0)
    check1 = (Math.round(anchorrect.bottom) + offsetY == Math.round(popuprect.top));
  else if (edge.indexOf("start") == 0)
    check1 = isWithinHalfPixel(anchorrect.left + offsetX, popuprect.right);
  else if (edge.indexOf("end") == 0)
    check1 = (Math.round(anchorrect.right) + offsetX == Math.round(popuprect.left));

  if (0 < edge.indexOf("before"))
    check2 = (Math.round(anchorrect.top) + offsetY == Math.round(popuprect.top));
  else if (0 < edge.indexOf("after"))
    check2 = isWithinHalfPixel(anchorrect.bottom + offsetY, popuprect.bottom);
  else if (0 < edge.indexOf("start"))
    check2 = (Math.round(anchorrect.left) + offsetX == Math.round(popuprect.left));
  else if (0 < edge.indexOf("end"))
    check2 = isWithinHalfPixel(anchorrect.right + offsetX, popuprect.right);

  ok(check1 && check2, testname + " position");
}
