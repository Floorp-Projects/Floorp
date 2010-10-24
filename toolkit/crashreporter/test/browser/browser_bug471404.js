// load our utility script
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(gTestPath);
scriptLoader.loadSubScript(rootDir + "aboutcrashes_utils.js", this);

function check_clear_visible(tab, aVisible) {
  let doc = gBrowser.getBrowserForTab(tab).contentDocument;
  let visible = false;
  let button = doc.getElementById("clear-reports");
  if (button) {
    let style = doc.defaultView.getComputedStyle(button, "");
    if (style.display != "none" &&
        style.visibility == "visible")
      visible = true;
  }
  is(visible, aVisible,
     "clear reports button is " + (aVisible ? "visible" : "hidden"));
}

// each test here has a setup (run before loading about:crashes) and onload (run after about:crashes loads)
let _tests = [{setup: null, onload: function(tab) { check_clear_visible(tab, false); }},
              {setup: function(crD) { add_fake_crashes(crD, 1); },
               onload: function(tab) { check_clear_visible(tab, true); }}
              ];
let _current_test = 0;

function run_test_setup(crD) {
  if (_tests[_current_test].setup) {
    _tests[_current_test].setup(crD);
  }
}

function run_test_onload(tab) {
  if (_tests[_current_test].onload) {
    _tests[_current_test].onload(tab);
  }
  _current_test++;

  if (_current_test == _tests.length) {
    cleanup_fake_appdir();
    gBrowser.removeTab(tab);
    finish();
    return false;
  }
  return true;
}

function test() {
  waitForExplicitFinish();
  let appD = make_fake_appdir();
  let crD = appD.clone();
  crD.append("Crash Reports");

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  let browser = gBrowser.getBrowserForTab(tab);
  browser.addEventListener("load", function() {
      executeSoon(function() {
          if (run_test_onload(tab)) {
            // prep and run the next test
            run_test_setup(crD);
            executeSoon(function() { browser.loadURI("about:crashes", null, null); });
          }
        });
    }, true);
  // kick things off
  run_test_setup(crD);
  browser.loadURI("about:crashes", null, null);
}
