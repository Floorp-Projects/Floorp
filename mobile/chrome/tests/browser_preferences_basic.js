// Bug 489158 - --browser-chrome Mochitests on Fennec [preferences]


var gTests = [];
var gCurrentTest = null;
var initialDragOffset = null;
var finalDragOffset = null;
let x = {};
let y = {};


function dragElement(element, x1, y1, x2, y2) {
  EventUtils.synthesizeMouse(element, x1, y1, { type: "mousedown" });
  EventUtils.synthesizeMouse(element, x2, y2, { type: "mousemove" });
  EventUtils.synthesizeMouse(element, x2, y2, { type: "mouseup" });
}

function doubleClick(element, x, y) {
  EventUtils.synthesizeMouse(element, x, y, {});
  EventUtils.synthesizeMouse(element, x, y, {});
}

function test() {
  // The "runNextTest" approach is async, so we need to call "waitForExplicitFinish()"
  // We call "finish()" when the tests are finished
  waitForExplicitFinish();

  // Start the tests
  runNextTest();
}

//------------------------------------------------------------------------------
// Iterating tests by shifting test out one by one as runNextTest is called.
function runNextTest() {
  // Run the next test until all tests completed
  if (gTests.length > 0) {
    gCurrentTest = gTests.shift();
    info(gCurrentTest.desc);
    gCurrentTest.run();
  }
  else {
    // Cleanup. All tests are completed at this point
    finish();
  }
}

// ------------------Verifying panning of preferences list----------------------
gTests.push({
  desc: "Test basic panning of Preferences",
  _currentTab: null,
  _contentScrollbox: document.getElementById("controls-scrollbox").boxObject.QueryInterface(Ci.nsIScrollBoxObject),
  _prefsScrollbox: document.getAnonymousElementByAttribute(document.getElementById("prefs-list"), "anonid", "main-box")
                            .boxObject.QueryInterface(Ci.nsIScrollBoxObject),

  run: function() {
    this._currentTab = Browser.addTab("about:blank", true);

    messageManager.addMessageListener("pageshow",
    function(aMessage) {
      messageManager.removeMessageListener(aMessage.name, arguments.callee);
      gCurrentTest.onPageLoad();
    });
  },

  onPageLoad: function() {
    // check whether the right sidebar is invisible
    let controls = document.getElementById("controls-scrollbox");

    // Assign offsets while panning
    initialDragOffset = document.getElementById("tabs-container").getBoundingClientRect().width;
    finalDragOffset = initialDragOffset + document.getElementById("browser-controls").getBoundingClientRect().width;

    gCurrentTest._contentScrollbox.getPosition(x, y);
    ok((x.value == initialDragOffset && y.value == 0), "The right sidebar must be invisible",
      "Got " + x.value + " " + y.value + ", expected " + initialDragOffset + ",0");

    // Reveal right sidebar
    let w = controls.clientWidth;
    let h = controls.clientHeight;
    dragElement(controls, w / 2, h / 2, w / 4, h / 2);

    // check whether the right sidebar has appeared
    gCurrentTest._contentScrollbox.getPosition(x,y);
    ok(x.value == finalDragOffset && y.value == 0, "The right sidebar must be visible",
      "Got " + x.value + " " + y.value + ", expected " + finalDragOffset + ",0");

    // check to see if the preference open button is visible and not depressed
    var prefsOpen = document.getElementById("tool-panel-open");
    is(prefsOpen.hidden, false, "Preferences open button must be visible");
    is(prefsOpen.checked, false, "Preferences open button must not be depressed");

    // check if preferences pane is invisble
    is(BrowserUI.isPanelVisible(), false, "Preferences panel is invisble");

    // click on the prefs button to go the preferences pane
    var prefsClick = document.getElementById("tool-panel-open");
    prefsClick.click();
    waitFor(function() {
      setTimeout(gCurrentTest.onPrefsView, 0);
    }, BrowserUI.isPanelVisible);
  },

  onPrefsView: function() {
    let prefsList = document.getElementById("prefs-list");
    let prefsListRect = prefsList.getBoundingClientRect();
    let w = prefsListRect.width;
    let h = prefsListRect.height;

    //check whether the preferences panel is visible
    ok(BrowserUI.isPanelVisible(), "Preferences panel must now be visble");

    // Check if preferences container is visible
    is(document.getElementById("panel-container").hidden, false, "Preferences panel should be visible");

    // check if side buttons are visible 6.Verify preference button is depressed and others are not
    is(document.getElementById("tool-addons").hidden, false, "Addons button must be visible");
    is(document.getElementById("tool-addons").checked, false, "Addons button must not be pressed");

    is(document.getElementById("tool-downloads").hidden, false, "Downloads button must be visible");
    is(document.getElementById("tool-downloads").checked, false, "Downloads button must not be pressed");

    is(document.getElementById("tool-preferences").hidden, false, "Preferences button must be visible");
    is(document.getElementById("tool-preferences").checked, true, "Preferences button must be pressed");

    // Verify back button is exists, is visible and is depressed
    // This button does not exist on Android.
    if (document.getElementById("tool-panel-close")) {
      is(document.getElementById("tool-panel-close").hidden, false, "Panel close button must be visible");
      is(document.getElementById("tool-panel-close").checked, false, "Panel close button must not be pressed");
    }

    // Now pan preferences pane up/down, left/right
    // check whether it is in correct position
    gCurrentTest._prefsScrollbox.getPosition(x, y);
    ok((x.value == 0 && y.value == 0),"The preferences pane should be visible", "Got " + x.value + " " + y.value + ", expected 0,0");

    // Move the preferences pane right
    dragElement(prefsList, w / 2, h / 2, w / 4, h / 2);

    gCurrentTest._prefsScrollbox.getPosition(x, y);
    ok((x.value == 0 && y.value == 0), "Preferences pane is not panned left", "Got " + x.value + " " + y.value + ", expected 0,0");

    // Move the preferences pane left
    dragElement(prefsList, w / 4, h / 2, w / 2, h / 2);

    gCurrentTest._prefsScrollbox.getPosition(x, y);
    ok((x.value == 0 && y.value ==0 ), "Preferences pane is not panned right", "Got " + x.value + " " + y.value + ", expected 0,0");

    // Move preferences pane up
    let [x1, y1, x2, y2] = [w / 2, h / 2, w / 2, h / 4].map(Math.round);
    EventUtils.synthesizeMouse(prefsList, x1, y1, { type: "mousedown" });
    EventUtils.synthesizeMouse(prefsList, x2, y2, { type: "mousemove" });

    // Need to wait for a paint event before another
    window.addEventListener("MozAfterPaint", function(aEvent) {
      window.removeEventListener("MozAfterPaint", arguments.callee, false);
      // Check whether it is moved up to the correct view
      let distance = y1 - y2;
      gCurrentTest._prefsScrollbox.getPosition(x, y);
      ok((x.value == 0 && y.value == distance), "Preferences pane is panned up", "Got " + x.value + " " + y.value + ", expected 0," + distance);

      // Need to wait for a paint event before another
      window.addEventListener("MozAfterPaint", function(aEvent) {
        window.removeEventListener("MozAfterPaint", arguments.callee, false);

        // Move preferences pane down
        EventUtils.synthesizeMouse(prefsList, x1, y1, { type: "mousemove" });
        EventUtils.synthesizeMouse(prefsList, x1, y1, { type: "mouseup" });

        // Check whether it goes back to old position
        gCurrentTest._prefsScrollbox.getPosition(x, y);
        ok((x.value == 0 && y.value == 0), "Preferences pane is panned down", "Got " + x.value + " " + y.value + ", expected 0,0");

        gCurrentTest.finish();
      }, false);
    }, false);
  },

  finish: function() {
    // Close the preferences pane
    BrowserUI.hidePanel();

    // check whether the preferences pane has disappeared
    is(document.getElementById("panel-container").hidden, true, "Preference pane is now invisible");

    //check if the right sidebar is still visible
    gCurrentTest._contentScrollbox.getPosition(x, y);
    ok(x.value == finalDragOffset && y.value == 0, "The right sidebar is still visible",
       "Got " + x.value + " " + y.value + ", expected " + finalDragOffset + ",0");

    // check whether the preferences open button is not depressed
    let prefsOpen = document.getElementById("tool-panel-open");
    is(prefsOpen.checked, false, "Preferences open button must not be depressed");

    // Reset the UI before the next test starts.
    Browser.hideSidebars();
    Browser.closeTab(gCurrentTest._currentTab);
    runNextTest();
  }
});
