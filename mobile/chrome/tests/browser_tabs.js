  let iosvc = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  let testURL_01 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_01.html";
  let testURL_02 = "chrome://mochikit/content/browser/mobile/chrome/browser_blank_02.html";
  var new_tab_01;
  var new_tab_02;

  function load_tabs() {
    var tabs_size_old = window.Browser._tabs.length -1; 
    var new_tab = window.Browser.getTabAtIndex(1); 
    var new_tab_URI = window.Browser.selectedTab.browser.currentURI.spec;
    is(new_tab_URI, testURL_01, "URL Matches newly created Tab");

    //Close new tab 
    var close_tab = window.Browser.closeTab(new_tab);
    var tabs_size_new = window.Browser._tabs.length; 	
    is(tabs_size_new, tabs_size_old, "Tab Closed");

    //Add new tab
    new_tab_01 = window.Browser.addTab(testURL_01,false);

    //Check tab switch
    new_tab_01.browser.addEventListener("load", tab_switch_01, true);
  }

  function tab_switch_01(){
    window.BrowserUI.selectTab(new_tab_01);
    is(window.Browser.selectedTab.browser.currentURI.spec, testURL_01, "Tab Switch 01 URL Matches");

    //Add new tab
    new_tab_02 =  window.Browser.addTab(testURL_02,false);
    new_tab_02.browser.addEventListener("load", tab_switch_02, true);
  }

  function tab_switch_02(){
    window.BrowserUI.selectTab(new_tab_02);
    is(window.Browser.selectedTab.browser.currentURI.spec, testURL_02, "Tab Switch 02 URL Matches");

    window.BrowserUI.selectTab(new_tab_01);
    is(window.Browser.selectedTab.browser.currentURI.spec, testURL_01, "Tab Switch 01 URL Matches"); 

    //Close new tab 
    window.Browser.closeTab(new_tab_01);
    window.Browser.closeTab(new_tab_02);
  }

  function test(){ 
    //Add new tab
    var new_tab = window.Browser.addTab(testURL_01,true);
    ok(new_tab, "Tab Opened");	

    //Check currentURI.spec
    new_tab.browser.addEventListener("load", load_tabs , true);
  }
