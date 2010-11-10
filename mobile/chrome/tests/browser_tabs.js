let testURL_01 = chromeRoot + "browser_blank_01.html";
let testURL_02 = chromeRoot + "browser_blank_02.html";
let testURL_03 = chromeRoot + "browser_blank_01.html#tab3";

let new_tab_01;
let new_tab_02;
let new_tab_03;

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();
  
  //Add new tab
  var new_tab = Browser.addTab(testURL_01,true);
  ok(new_tab, "Tab Opened");	

  //Check currentURI.spec
  new_tab.browser.addEventListener("load", load_tabs , true);
}

function load_tabs() {
  var tabs_size_old = Browser._tabs.length -1; 
  var new_tab = Browser.getTabAtIndex(1); 
  var new_tab_URI = Browser.selectedTab.browser.currentURI.spec;
  is(new_tab_URI, testURL_01, "URL Matches newly created Tab");

  //Close new tab 
  var close_tab = Browser.closeTab(new_tab);
  var tabs_size_new = Browser._tabs.length; 	
  is(tabs_size_new, tabs_size_old, "Tab Closed");

  //Add new tab
  new_tab_01 = Browser.addTab(testURL_01,false);

  //Check tab switch
  new_tab_01.browser.addEventListener("load", tab_switch_01, true);
}

function tab_switch_01() {
  BrowserUI.selectTab(new_tab_01);
  is(Browser.selectedTab.browser.currentURI.spec, testURL_01, "Tab Switch 01 URL Matches");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  //Add new tab
  new_tab_02 =  Browser.addTab(testURL_02,false);
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
  new_tab_03 =  Browser.addTab(testURL_03, true, new_tab_01);
  new_tab_03.browser.addEventListener("load", tab_switch_03, true);
}

function tab_switch_03() {
  is(Browser.selectedTab.browser.currentURI.spec, testURL_03, "Tab Switch 03 URL Matches"); 
  is(new_tab_03.owner, new_tab_01, "Tab 03 owned by tab 01");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  Browser.closeTab(new_tab_03);
  is(Browser.selectedTab, new_tab_01, "Closing tab 03 returns to owner");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  new_tab_03 =  Browser.addTab(testURL_03, true, new_tab_01);
  new_tab_03.browser.addEventListener("load", tab_switch_04, true);
}

function tab_switch_04() {
  is(Browser.selectedTab.browser.currentURI.spec, testURL_03, "Tab Switch 03 URL Matches"); 
  is(new_tab_03.owner, new_tab_01, "Tab 03 owned by tab 01");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  Browser.closeTab(new_tab_01);
  is(Browser.selectedTab, new_tab_03, "Closing tab 01 keeps selectedTab");
  is(new_tab_03.owner, null, "Closing tab 01 nulls tab3 owner");
  is(Browser.selectedTab.notification, Elements.browsers.selectedPanel, "Deck has correct browser");

  done();
}

function done() {
  //Close new tab 
  Browser.closeTab(new_tab_01);
  Browser.closeTab(new_tab_02);
  Browser.closeTab(new_tab_03);

  // For some reason, this test is causing the sidebar to appear.
  // Clean up the UI for later tests (see bug 598962).
  Browser.hideSidebars();

  // We must finialize the tests
  finish();
}
