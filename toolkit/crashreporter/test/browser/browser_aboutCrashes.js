// load our utility script
var scriptLoader = Components.classes["@mozilla.org/moz/jssubscript-loader;1"]
                             .getService(Components.interfaces.mozIJSSubScriptLoader);

var rootDir = getRootDirectory(gTestPath);
scriptLoader.loadSubScript(rootDir + "/aboutcrashes_utils.js", this);

function check_crash_list(tab, crashes) {
  let doc = gBrowser.getBrowserForTab(tab).contentDocument;
  let crashlinks = doc.getElementById("tbody").getElementsByTagName("a");
  is(crashlinks.length, crashes.length, "about:crashes lists correct number of crash reports");
  for(let i = 0; i < crashes.length; i++) {
    is(crashlinks[i].firstChild.textContent, crashes[i].id, i + ": crash ID is correct");
  }
  cleanup_fake_appdir();
  gBrowser.removeTab(tab);
  finish();
}

function test() {
  waitForExplicitFinish();
  let appD = make_fake_appdir();
  let crD = appD.clone();
  crD.append("Crash Reports");
  let crashes = add_fake_crashes(crD, 5);
  // sanity check
  let dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                         .getService(Components.interfaces.nsIProperties);
  let appDtest = dirSvc.get("UAppData", Components.interfaces.nsILocalFile);
  ok(appD.equals(appDtest), "directory service provider registered ok");
  let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");
  let browser = gBrowser.getBrowserForTab(tab);
  browser.addEventListener("load", function() {
      ok(true, "about:crashes loaded");
      executeSoon(function() { check_crash_list(tab, crashes); });
    }, true);
  browser.loadURI("about:crashes", null, null);
}
