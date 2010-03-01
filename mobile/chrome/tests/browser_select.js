let testURL = "chrome://mochikit/content/browser/mobile/chrome/browser_select.html";
let new_tab = null;

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
  // This test is async
  waitForExplicitFinish();
  
  // Add new tab to hold the <select> page
  new_tab = Browser.addTab(testURL, true);
  ok(new_tab, "Tab Opened");	

  // Wait for the tab to load, then do the test
  new_tab.browser.addEventListener("load", onPageLoaded, true);
}

function onPageLoaded() {
  let combo = new_tab.browser.contentDocument.getElementById("combobox");
  isnot(combo, null, "Get the select from web content");

  // XXX Sending a synthesized event to the combo is not working
  //EventUtils.synthesizeMouse(combo, combo.clientWidth / 2, combo.clientHeight / 2, {}, combo.ownerDocument.defaultView);
  SelectHelper.show(combo);
  
  waitFor(onUIReady, function() { return document.getElementById("select-container").hidden == false; });
}
  
function onUIReady() {    
  let selectui = document.getElementById("select-container");
  is(selectui.hidden, false, "Select UI should be open");
  
  let doneButton = document.getElementById("select-buttons-done");
  doneButton.click();

  // Close our tab when finished
  Browser.closeTab(new_tab);
  
  // We must finialize the tests
  finish();
}
