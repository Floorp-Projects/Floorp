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

function startPopupTests(tests)
{
  document.addEventListener("popupshowing", eventOccured, false);
  document.addEventListener("popupshown", eventOccured, false);
  document.addEventListener("popuphiding", eventOccured, false);
  document.addEventListener("popuphidden", eventOccured, false);
  document.addEventListener("command", eventOccured, false);
  document.addEventListener("DOMMenuItemActive", eventOccured, false);
  document.addEventListener("DOMMenuItemInactive", eventOccured, false);
  document.addEventListener("DOMMenuInactive", eventOccured, false);
  document.addEventListener("DOMMenuBarActive", eventOccured, false);
  document.addEventListener("DOMMenuBarInactive", eventOccured, false);

  gPopupTests = tests;

  goNext();
}

function finish()
{
  window.close();
  window.opener.SimpleTest.finish();
}

function ok(condition, message) {
  window.opener.SimpleTest.ok(condition, message);
}

function is(left, right, message) {
  window.opener.SimpleTest.is(left, right, message);
}

function eventOccured(event)
{
   netscape.security.PrivilegeManager.enablePrivilege('UniversalXPConnect');

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
      ok(false, "Extra " + event.type + " event fired " + gPopupTests[gTestIndex].testname);
      return;
    }

    var eventitem = events[gTestEventIndex].split(" ");
    var matches = (eventitem[1] == "#tooltip") ?
                  (event.originalTarget.localName == "tooltip" &&
                   event.originalTarget.getAttribute("default") == "true") :
                  (eventitem[0] == event.type && eventitem[1] == event.target.id);
    ok(matches, test.testname + " " + event.type + " fired");

    var expectedState;
    switch (event.type) {
      case "popupshowing": expectedState = "showing"; break;
      case "popupshown": expectedState = "open"; break;
      case "popuphiding": expectedState = "hiding"; break;
      case "popuphidden": expectedState = "closed"; break;
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

function checkResult()
{
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

function goNextStep()
{
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

function goNext()
{
  if (gTestIndex >= 0 && "end" in gPopupTests[gTestIndex] && gPopupTests[gTestIndex].end) {
    finish();
    return;
  }

  gTestIndex++;
  gTestStepIndex = 0;
  if (gTestIndex < gPopupTests.length) {
    var test = gPopupTests[gTestIndex]

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
    if (!("events" in test))
      checkResult();
  }
  else {
    finish();
  }
}

function openMenu(menu)
{
  if ("open" in menu) {
    menu.open = true;
  }
  else {
    var bo = menu.boxObject;
    if (bo instanceof Components.interfaces.nsIMenuBoxObject)
      bo.openMenu(true);
    else
      synthesizeMouse(menu, 4, 4, { });
  }
}

function closeMenu(menu, popup)
{
  if ("open" in menu) {
    menu.open = false;
  }
  else {
    var bo = menu.boxObject;
    if (bo instanceof Components.interfaces.nsIMenuBoxObject)
      bo.openMenu(false);
    else
      popup.hidePopup();
  }
}

function checkActive(popup, id, testname)
{
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

function checkOpen(menuid, testname)
{
  var menu = document.getElementById(menuid);
  if ("open" in menu)
    ok(menu.open, testname + " " + menuid + " menu is open");
  else if (menu.boxObject instanceof Components.interfaces.nsIMenuBoxObject)
    ok(menu.getAttribute("open") == "true", testname + " " + menuid + " menu is open");
}

function checkClosed(menuid, testname)
{
  var menu = document.getElementById(menuid);
  if ("open" in menu)
    ok(!menu.open, testname + " " + menuid + " menu is open");
  else if (menu.boxObject instanceof Components.interfaces.nsIMenuBoxObject)
    ok(!menu.hasAttribute("open"), testname + " " + menuid + " menu is closed");
}

function convertPosition(anchor, align)
{
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

function compareEdge(anchor, popup, edge, offsetX, offsetY, testname)
{
  testname += " " + edge;

  checkOpen(anchor.id, testname);

  var anchorrect = anchor.getBoundingClientRect();
  var popuprect = popup.getBoundingClientRect();
  var check1 = false, check2 = false;

  ok((Math.round(popuprect.right) - Math.round(popuprect.left)) &&
     (Math.round(popuprect.bottom) - Math.round(popuprect.top)),
     testname + " size");

  if (edge == "overlap") {
    ok(Math.round(anchorrect.left) + offsetY == Math.round(popuprect.left) &&
       Math.round(anchorrect.top) + offsetY == Math.round(popuprect.top),
       testname + " position");
    return;
  }

  if (edge.indexOf("before") == 0)
    check1 = (Math.round(anchorrect.top) + offsetY == Math.round(popuprect.bottom));
  else if (edge.indexOf("after") == 0)
    check1 = (Math.round(anchorrect.bottom) + offsetY == Math.round(popuprect.top));
  else if (edge.indexOf("start") == 0)
    check1 = (Math.round(anchorrect.left) + offsetX == Math.round(popuprect.right));
  else if (edge.indexOf("end") == 0)
    check1 = (Math.round(anchorrect.right) + offsetX == Math.round(popuprect.left));

  if (0 < edge.indexOf("before"))
    check2 = (Math.round(anchorrect.top) + offsetY == Math.round(popuprect.top));
  else if (0 < edge.indexOf("after"))
    check2 = (Math.round(anchorrect.bottom) + offsetY == Math.round(popuprect.bottom));
  else if (0 < edge.indexOf("start"))
    check2 = (Math.round(anchorrect.left) + offsetX == Math.round(popuprect.left));
  else if (0 < edge.indexOf("end"))
    check2 = (Math.round(anchorrect.right) + offsetX == Math.round(popuprect.right));

  ok(check1 && check2, testname + " position");
}
