// Very basic tests for the main window UI

const Cc = Components.classes;
const Ci = Components.interfaces;

//------------------------------------------------------------------------------
// Entry point (must be named "test")
function test() {
   is(window.location.href, "chrome://browser/content/browser.xul", "Main window should be browser.xul");

   window.focus();

   let browser = Browser.selectedBrowser;
   isnot(browser, null, "Should have a browser");
   
   is(browser.currentURI.spec, Browser.selectedTab.browser.currentURI.spec, "selectedBrowser == selectedTab.browser");
}
