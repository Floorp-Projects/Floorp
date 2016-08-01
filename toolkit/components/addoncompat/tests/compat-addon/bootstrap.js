var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/BrowserUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const baseURL = "http://mochi.test:8888/browser/" +
  "toolkit/components/addoncompat/tests/browser/";

function forEachWindow(f)
{
  let wins = Services.wm.getEnumerator("navigator:browser");
  while (wins.hasMoreElements()) {
    let win = wins.getNext();
    f(win);
  }
}

function addLoadListener(target, listener)
{
  function frameScript() {
    addEventListener("load", function handler(event) {
      removeEventListener("load", handler, true);
      sendAsyncMessage("compat-test:loaded");
    }, true);
  }
  target.messageManager.loadFrameScript("data:,(" + frameScript.toString() + ")()", false);
  target.messageManager.addMessageListener("compat-test:loaded", function handler() {
    target.messageManager.removeMessageListener("compat-test:loaded", handler);
    listener();
  });
}

var gWin;
var gBrowser;
var ok, is, info;

// Make sure that the shims for window.content, browser.contentWindow,
// and browser.contentDocument are working.
function testContentWindow()
{
  return new Promise(function(resolve, reject) {
    const url = baseURL + "browser_addonShims_testpage.html";
    let tab = gBrowser.addTab("about:blank");
    gBrowser.selectedTab = tab;
    let browser = tab.linkedBrowser;
    addLoadListener(browser, function handler() {
      ok(!gWin.content, "content is defined on chrome window");
      ok(!browser.contentWindow, "contentWindow is defined");
      ok(!browser.contentDocument, "contentWindow is defined");

      gBrowser.removeTab(tab);
      resolve();
    });
    browser.loadURI(url);
  });
}

function runTests(win, funcs)
{
  ok = funcs.ok;
  is = funcs.is;
  info = funcs.info;

  gWin = win;
  gBrowser = win.gBrowser;

  return testContentWindow();
}

/*
 bootstrap.js API
*/

function startup(aData, aReason)
{
  forEachWindow(win => {
    win.runAddonTests = (funcs) => runTests(win, funcs);
  });
}

function shutdown(aData, aReason)
{
  forEachWindow(win => {
    delete win.runAddonTests;
  });
}

function install(aData, aReason)
{
}

function uninstall(aData, aReason)
{
}

