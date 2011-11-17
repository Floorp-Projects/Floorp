let testURL_01 = chromeRoot + "browser_blank_01.html";
let testURL_02 = chromeRoot + "browser_blank_02.html";
let testURL_03 = chromeRoot + "browser_blank_01.html#tab3";

let new_tab_01;
let new_tab_02;
let new_tab_03;
let new_tab_04;
let new_tab_05;

// XXX This is synced with the value of the COLUMN_MARGIN const in chrome/content/tabs.xml
const COLUMN_MARGIN = 20;

function checkExpectedSize() {
  let tabs = document.getElementById("tabs");
  let tabRect = tabs.children.firstChild.getBoundingClientRect();
  let expectedSize = (tabs._columnsCount * (COLUMN_MARGIN + tabRect.width));

  let tabsRect = tabs.children.getBoundingClientRect();
  is(tabsRect.width, expectedSize, "Tabs container size should be equal to the number of columns and margins");
}

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();
  
  //Add new tab
  var new_tab = Browser.addTab(testURL_01,true);
  ok(new_tab, "Tab Opened");	

  // Ensure columnsCount is not equal to 0
  isnot(document.getElementById("tabs")._columnsCount, 0, "Tabs columns count should not be equal to 0");

  // Check the size of the tabs container sidebar
  checkExpectedSize();

  //Check currentURI.spec
  new_tab.browser.addEventListener("load", load_tabs , true);
}

function load_tabs() {
  var tabs_size_old = Browser._tabs.length -1; 
  var new_tab = Browser.getTabAtIndex(1); 
  var new_tab_URI = Browser.selectedTab.browser.currentURI.spec;
  is(new_tab_URI, testURL_01, "URL Matches newly created Tab");

  //Close new tab 
  var close_tab = Browser.closeTab(new_tab, { forceClose: true });
  var tabs_size_new = Browser._tabs.length; 	
  is(tabs_size_new, tabs_size_old, "Tab Closed");

  //Add new tab
  new_tab_01 = Browser.addTab(testURL_01,false);
  checkExpectedSize();

  //Check tab switch
  new_tab_01.browser.addEventListener("load", tab_switch_01, true);
}

function tab_switch_01() {
  BrowserUI.selectTab(new_tab_01);
  is(Browser.selectedTab.browser.currentURI.spec, testURL_01, "Tab Switch 01 URL Matches");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  //Add new tab
  new_tab_02 =  Browser.addTab(testURL_02,false);
  checkExpectedSize();

  new_tab_02.browser.addEventListener("load", tab_switch_02, true);
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");
}

function tab_switch_02() {
  BrowserUI.selectTab(new_tab_02);
  is(Browser.selectedTab.browser.currentURI.spec, testURL_02, "Tab Switch 02 URL Matches");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  BrowserUI.selectTab(new_tab_01);
  is(Browser.selectedTab.browser.currentURI.spec, testURL_01, "Tab Switch 01 URL Matches");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  //Add new tab
  new_tab_03 = Browser.addTab(testURL_03, true, new_tab_01);
  new_tab_04 = Browser.addTab(testURL_03, false, new_tab_01);

  checkExpectedSize();
  new_tab_03.browser.addEventListener("load", tab_switch_03, true);
}

function tab_switch_03() {
  is(Browser.selectedTab.browser.currentURI.spec, testURL_03, "Tab Switch 03 URL Matches"); 
  is(new_tab_03.owner, new_tab_01, "Tab 03 owned by tab 01");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  Browser.closeTab(new_tab_03, { forceClose: true });
  is(Browser.selectedTab, new_tab_04, "Closing tab 03 goes to sibling");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  Browser.closeTab(new_tab_04, { forceClose: true });
  is(Browser.selectedTab, new_tab_01, "Closing tab 04 returns to owner");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  new_tab_03 = Browser.addTab(testURL_03, true, new_tab_01);
  checkExpectedSize();
  new_tab_03.browser.addEventListener("load", tab_switch_04, true);
}

function tab_switch_04() {
  is(Browser.selectedTab.browser.currentURI.spec, testURL_03, "Tab Switch 03 URL Matches"); 
  is(new_tab_03.owner, new_tab_01, "Tab 03 owned by tab 01");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  Browser.closeTab(new_tab_01, { forceClose: true });
  is(Browser.selectedTab, new_tab_03, "Closing tab 01 keeps selectedTab");
  is(new_tab_03.owner, null, "Closing tab 01 nulls tab3 owner");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  function callback() {
    new_tab_04.browser.removeEventListener("load", arguments.callee, true);
    Browser.closeTab(new_tab_04, { forceClose: true });
    tab_undo();
  };

  // Add a tab then close it
  new_tab_04 = Browser.addTab("about:home", true);
  waitFor(callback, function() {
    // Ensure the tab is not empty
    return !new_tab_04.chromeTab.thumbnail.hasAttribute("empty");
  });

  checkExpectedSize();
}

function tab_undo() {
  let undoBox = document.getElementById("tabs")._tabsUndo;
  ok(undoBox.firstChild, "It should be a tab in the undo box");

  undoBox.firstChild._onUndo();
  new_tab_04 = Browser.selectedTab;
  new_tab_05 = Browser.addTab("about:blank", true);
  checkExpectedSize();
  tab_on_undo();
}

function tab_on_undo() {
  let undoBox = document.getElementById("tabs")._tabsUndo;
  is(undoBox.firstChild, null, "It should be no tab in the undo box");

  Browser.loadURI("about:home");
  is(undoBox.firstChild, null, "It should be no tab in the undo box when opening a new local page");

  // loadURI will open a new tab so ensure new_tab_05 point to the newly opened tab
  new_tab_05 = Browser.selectedTab;

  let tabs = [new_tab_01, new_tab_02, new_tab_03, new_tab_04, new_tab_05];
  while (tabs.length)
    Browser.closeTab(tabs.shift(), { forceClose: true });
  checkExpectedSize();

  tab_about_empty();
}

function tab_about_empty() {
  let tabCount = Browser.tabs.length;
  new_tab_01 = Browser.addTab("about:empty", true);
  ok(new_tab_01, "Tab Opened");
  is(new_tab_01.browser.getAttribute("remote"), "true", "about:empty opened in a remote tab");

  Browser.loadURI("about:");
  let tab = Browser.selectedTab;
  isnot(tab, new_tab_01, "local page opened in a new tab");
  isnot(tab.browser.getAttribute("remote"), "true", "local page opened in a local tab");
  is(Browser.tabs.length, tabCount + 1, "Old empty tab is closed");

  let undoBox = document.getElementById("tabs")._tabsUndo;
  is(undoBox.firstChild, null, "about:empty is not in the undo close tab box");

  Browser.closeTab(tab, { forceClose: true });

  done();
}

function done() {
  // For some reason, this test is causing the sidebar to appear.
  // Clean up the UI for later tests (see bug 598962).
  Browser.hideSidebars();

  // We must finialize the tests
  finish();
}
