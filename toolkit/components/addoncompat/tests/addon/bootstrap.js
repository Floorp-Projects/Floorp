var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const baseURL = "http://mochi.test:8888/browser/" +
  "toolkit/components/addoncompat/tests/browser/";

function forEachWindow(f)
{
  let wins = Services.ww.getWindowEnumerator("navigator:browser");
  while (wins.hasMoreElements()) {
    let win = wins.getNext();
    if (win.gBrowser) {
      f(win);
    }
  }
}

function addLoadListener(target, listener)
{
  target.addEventListener("load", function handler(event) {
    target.removeEventListener("load", handler, true);
    return listener(event);
  }, true);
}

let gWin;
let gBrowser;
let ok, is, info;

// Make sure that the shims for window.content, browser.contentWindow,
// and browser.contentDocument are working.
function testContentWindow()
{
  return new Promise(function(resolve, reject) {
    const url = baseURL + "browser_addonShims_testpage.html";
    let tab = gBrowser.addTab(url);
    gBrowser.selectedTab = tab;
    let browser = tab.linkedBrowser;
    addLoadListener(browser, function handler() {
      ok(gWin.content, "content is defined on chrome window");
      ok(browser.contentWindow, "contentWindow is defined");
      ok(browser.contentDocument, "contentWindow is defined");
      is(gWin.content, browser.contentWindow, "content === contentWindow");

      ok(browser.contentDocument.getElementById("link"), "link present in document");

      // FIXME: Waiting on bug 1073631.
      //is(browser.contentWindow.wrappedJSObject.global, 3, "global available on document");

      gBrowser.removeTab(tab);
      resolve();
    });
  });
}

// Test for bug 1060046 and bug 1072607. We want to make sure that
// adding and removing listeners works as expected.
function testListeners()
{
  return new Promise(function(resolve, reject) {
    const url1 = baseURL + "browser_addonShims_testpage.html";
    const url2 = baseURL + "browser_addonShims_testpage2.html";

    let tab = gBrowser.addTab(url2);
    let browser = tab.linkedBrowser;
    addLoadListener(browser, function handler() {
      function dummyHandler() {}

      // Test that a removed listener stays removed (bug
      // 1072607). We're looking to make sure that adding and removing
      // a listener here doesn't cause later listeners to fire more
      // than once.
      for (let i = 0; i < 5; i++) {
        gBrowser.addEventListener("load", dummyHandler, true);
        gBrowser.removeEventListener("load", dummyHandler, true);
      }

      // We also want to make sure that this listener doesn't fire
      // after it's removed.
      let loadWithRemoveCount = 0;
      addLoadListener(browser, function handler1() {
        loadWithRemoveCount++;
        is(event.target.documentURI, url1, "only fire for first url");
      });

      // Load url1 and then url2. We want to check that:
      // 1. handler1 only fires for url1.
      // 2. handler2 only fires once for url1 (so the second time it
      //    fires should be for url2).
      let loadCount = 0;
      browser.addEventListener("load", function handler2(event) {
        loadCount++;
        if (loadCount == 1) {
          is(event.target.documentURI, url1, "first load is for first page loaded");
          browser.loadURI(url2);
        } else {
          gBrowser.removeEventListener("load", handler2, true);

          is(event.target.documentURI, url2, "second load is for second page loaded");
          is(loadWithRemoveCount, 1, "load handler is only called once");

          gBrowser.removeTab(tab);
          resolve();
        }
      }, true);

      browser.loadURI(url1);
    });
  });
}

// Test for bug 1059207. We want to make sure that adding a capturing
// listener and a non-capturing listener to the same element works as
// expected.
function testCapturing()
{
  return new Promise(function(resolve, reject) {
    let capturingCount = 0;
    let nonCapturingCount = 0;

    function capturingHandler(event) {
      is(capturingCount, 0, "capturing handler called once");
      is(nonCapturingCount, 0, "capturing handler called before bubbling handler");
      capturingCount++;
    }

    function nonCapturingHandler(event) {
      is(capturingCount, 1, "bubbling handler called after capturing handler");
      is(nonCapturingCount, 0, "bubbling handler called once");
      nonCapturingCount++;
    }

    gBrowser.addEventListener("mousedown", capturingHandler, true);
    gBrowser.addEventListener("mousedown", nonCapturingHandler, false);

    const url = baseURL + "browser_addonShims_testpage.html";
    let tab = gBrowser.addTab(url);
    let browser = tab.linkedBrowser;
    addLoadListener(browser, function handler() {
      let win = browser.contentWindow;
      let event = win.document.createEvent("MouseEvents");
      event.initMouseEvent("mousedown", true, false, win, 1,
                           1, 0, 0, 0, // screenX, screenY, clientX, clientY
                           false, false, false, false, // ctrlKey, altKey, shiftKey, metaKey
                           0, null); // buttonCode, relatedTarget

      let element = win.document.getElementById("output");
      element.dispatchEvent(event);

      is(capturingCount, 1, "capturing handler fired");
      is(nonCapturingCount, 1, "bubbling handler fired");

      gBrowser.removeEventListener("mousedown", capturingHandler, true);
      gBrowser.removeEventListener("mousedown", nonCapturingHandler, false);

      gBrowser.removeTab(tab);
      resolve();
    });
  });
}

// Make sure we get observer notifications that normally fire in the
// child.
function testObserver()
{
  return new Promise(function(resolve, reject) {
    let observerFired = 0;

    function observer(subject, topic, data) {
      Services.obs.removeObserver(observer, "document-element-inserted");
      observerFired++;
    }
    Services.obs.addObserver(observer, "document-element-inserted", false);

    let count = 0;
    const url = baseURL + "browser_addonShims_testpage.html";
    let tab = gBrowser.addTab(url);
    let browser = tab.linkedBrowser;
    browser.addEventListener("load", function handler() {
      count++;
      if (count == 1) {
        browser.reload();
      } else {
        browser.removeEventListener("load", handler);

        is(observerFired, 1, "got observer notification");

        gBrowser.removeTab(tab);
        resolve();
      }
    }, true);
  });
}

// Test for bug 1072472. Make sure that creating a sandbox to run code
// in the content window works. This is essentially a test for
// Greasemonkey.
function testSandbox()
{
  return new Promise(function(resolve, reject) {
    const url = baseURL + "browser_addonShims_testpage.html";
    let tab = gBrowser.addTab(url);
    let browser = tab.linkedBrowser;
    browser.addEventListener("load", function handler() {
      browser.removeEventListener("load", handler);

      let sandbox = Cu.Sandbox(browser.contentWindow,
                               {sandboxPrototype: browser.contentWindow,
                                wantXrays: false});
      Cu.evalInSandbox("const unsafeWindow = window;", sandbox);
      Cu.evalInSandbox("document.getElementById('output').innerHTML = 'hello';", sandbox);

      is(browser.contentDocument.getElementById("output").innerHTML, "hello",
         "sandbox code ran successfully");

      gBrowser.removeTab(tab);
      resolve();
    }, true);
  });
}

function runTests(win, funcs)
{
  ok = funcs.ok;
  is = funcs.is;
  info = funcs.info;

  gWin = win;
  gBrowser = win.gBrowser;

  return testContentWindow().
    then(testListeners).
    then(testCapturing).
    then(testObserver).
    then(testSandbox);
}

/*
 bootstrap.js API
*/

function startup(aData, aReason)
{
  forEachWindow(win => {
    win.runAddonShimTests = (funcs) => runTests(win, funcs);
  });
}

function shutdown(aData, aReason)
{
  forEachWindow(win => {
    delete win.runAddonShimTests;
  });
}

function install(aData, aReason)
{
}

function uninstall(aData, aReason)
{
}

