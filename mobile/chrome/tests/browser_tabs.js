let testURL_01 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_01.html";
let testURL_02 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_02.html";

let new_tab_01;
let new_tab_02;

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

  //Add new tab
  new_tab_02 =  Browser.addTab(testURL_02,false);
  new_tab_02.browser.addEventListener("load", tab_switch_02, true);
}

function tab_switch_02() {
  BrowserUI.selectTab(new_tab_02);
  is(Browser.selectedTab.browser.currentURI.spec, testURL_02, "Tab Switch 02 URL Matches");

  BrowserUI.selectTab(new_tab_01);
  is(Browser.selectedTab.browser.currentURI.spec, testURL_01, "Tab Switch 01 URL Matches"); 

  //Close new tab 
  Browser.closeTab(new_tab_01);
  Browser.closeTab(new_tab_02);
  
  // We must finialize the tests
  finish();
}
