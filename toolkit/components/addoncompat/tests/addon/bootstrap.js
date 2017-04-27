// This file also defines a frame script.
/* eslint-env mozilla/frame-script */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const baseURL = "http://mochi.test:8888/browser/" +
  "toolkit/components/addoncompat/tests/browser/";

var contentSecManager = Cc["@mozilla.org/contentsecuritymanager;1"]
                          .getService(Ci.nsIContentSecurityManager);

function forEachWindow(f) {
  let wins = Services.wm.getEnumerator("navigator:browser");
  while (wins.hasMoreElements()) {
    let win = wins.getNext();
    f(win);
  }
}

function addLoadListener(target, listener) {
  target.addEventListener("load", function(event) {
    return listener(event);
  }, {capture: true, once: true});
}

var gWin;
var gBrowser;
var ok, is, info;

function removeTab(tab, done) {
  // Remove the tab in a different turn of the event loop. This way
  // the nested event loop in removeTab doesn't conflict with the
  // event listener shims.
  gWin.setTimeout(() => {
    gBrowser.removeTab(tab);
    done();
  }, 0);
}

// Make sure that the shims for window.content, browser.contentWindow,
// and browser.contentDocument are working.
function testContentWindow() {
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
      ok(browser.webNavigation.sessionHistory, "sessionHistory is defined");

      ok(browser.contentDocument.getElementById("link"), "link present in document");

      // FIXME: Waiting on bug 1073631.
      // is(browser.contentWindow.wrappedJSObject.global, 3, "global available on document");

      removeTab(tab, resolve);
    });
  });
}

// Test for bug 1060046 and bug 1072607. We want to make sure that
// adding and removing listeners works as expected.
function testListeners() {
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
      addLoadListener(browser, function handler1(event) {
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

          removeTab(tab, resolve);
        }
      }, true);

      browser.loadURI(url1);
    });
  });
}

// Test for bug 1059207. We want to make sure that adding a capturing
// listener and a non-capturing listener to the same element works as
// expected.
function testCapturing() {
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
    gBrowser.addEventListener("mousedown", nonCapturingHandler);

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
      gBrowser.removeEventListener("mousedown", nonCapturingHandler);

      removeTab(tab, resolve);
    });
  });
}

// Make sure we get observer notifications that normally fire in the
// child.
function testObserver() {
  return new Promise(function(resolve, reject) {
    let observerFired = 0;

    function observer(subject, topic, data) {
      Services.obs.removeObserver(observer, "document-element-inserted");
      observerFired++;
    }
    Services.obs.addObserver(observer, "document-element-inserted");

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

        removeTab(tab, resolve);
      }
    }, true);
  });
}

// Test for bug 1072472. Make sure that creating a sandbox to run code
// in the content window works. This is essentially a test for
// Greasemonkey.
function testSandbox() {
  return new Promise(function(resolve, reject) {
    const url = baseURL + "browser_addonShims_testpage.html";
    let tab = gBrowser.addTab(url);
    let browser = tab.linkedBrowser;
    browser.addEventListener("load", function() {
      let sandbox = Cu.Sandbox(browser.contentWindow,
                               {sandboxPrototype: browser.contentWindow,
                                wantXrays: false});
      Cu.evalInSandbox("const unsafeWindow = window;", sandbox);
      Cu.evalInSandbox("document.getElementById('output').innerHTML = 'hello';", sandbox);

      is(browser.contentDocument.getElementById("output").innerHTML, "hello",
         "sandbox code ran successfully");

      // Now try a sandbox with expanded principals.
      sandbox = Cu.Sandbox([browser.contentWindow],
                           {sandboxPrototype: browser.contentWindow,
                            wantXrays: false});
      Cu.evalInSandbox("const unsafeWindow = window;", sandbox);
      Cu.evalInSandbox("document.getElementById('output').innerHTML = 'hello2';", sandbox);

      is(browser.contentDocument.getElementById("output").innerHTML, "hello2",
         "EP sandbox code ran successfully");

      removeTab(tab, resolve);
    }, {capture: true, once: true});
  });
}

// Test for bug 1095305. We just want to make sure that loading some
// unprivileged content from an add-on package doesn't crash.
function testAddonContent() {
  let chromeRegistry = Components.classes["@mozilla.org/chrome/chrome-registry;1"]
    .getService(Components.interfaces.nsIChromeRegistry);
  let base = chromeRegistry.convertChromeURL(Services.io.newURI("chrome://addonshim1/content/"));

  let res = Services.io.getProtocolHandler("resource")
    .QueryInterface(Ci.nsIResProtocolHandler);
  res.setSubstitution("addonshim1", base);

  return new Promise(function(resolve, reject) {
    const url = "resource://addonshim1/page.html";
    let tab = gBrowser.addTab(url);
    let browser = tab.linkedBrowser;
    addLoadListener(browser, function handler() {
      res.setSubstitution("addonshim1", null);
      removeTab(tab, resolve);
    });
  });
}


// Test for bug 1102410. We check that multiple nsIAboutModule's can be
// registered in the parent, and that the child can browse to each of
// the registered about: pages.
function testAboutModuleRegistration() {
  let Registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

  let modulesToUnregister = new Map();

  function TestChannel(uri, aLoadInfo, aboutName) {
    this.aboutName = aboutName;
    this.loadInfo = aLoadInfo;
    this.URI = this.originalURI = uri;
  }

  TestChannel.prototype = {
    asyncOpen(listener, context) {
      let stream = this.open();
      let runnable = {
        run: () => {
          try {
            listener.onStartRequest(this, context);
          } catch (e) {}
          try {
            listener.onDataAvailable(this, context, stream, 0, stream.available());
          } catch (e) {}
          try {
            listener.onStopRequest(this, context, Cr.NS_OK);
          } catch (e) {}
        }
      };
      Services.tm.dispatchToMainThread(runnable);
    },

    asyncOpen2(listener) {
      // throws an error if security checks fail
      var outListener = contentSecManager.performSecurityCheck(this, listener);
      return this.asyncOpen(outListener, null);
    },

    open() {
      function getWindow(channel) {
        try {
          if (channel.notificationCallbacks)
            return channel.notificationCallbacks.getInterface(Ci.nsILoadContext).associatedWindow;
        } catch (e) {}

        try {
          if (channel.loadGroup && channel.loadGroup.notificationCallbacks)
            return channel.loadGroup.notificationCallbacks.getInterface(Ci.nsILoadContext).associatedWindow;
        } catch (e) {}

        return null;
      }

      let data = `<html><h1>${this.aboutName}</h1></html>`;
      let wnd = getWindow(this);
      if (!wnd)
        throw Cr.NS_ERROR_UNEXPECTED;

      let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(Ci.nsIStringInputStream);
      stream.setData(data, data.length);
      return stream;
    },

    open2() {
      // throws an error if security checks fail
      contentSecManager.performSecurityCheck(this, null);
      return this.open();
    },

    isPending() {
      return false;
    },
    cancel() {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    },
    suspend() {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    },
    resume() {
      throw Cr.NS_ERROR_NOT_IMPLEMENTED;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIChannel, Ci.nsIRequest])
  };

  /**
   * This function creates a new nsIAboutModule and registers it. Callers
   * should also call unregisterModules after using this function to clean
   * up the nsIAboutModules at the end of this test.
   *
   * @param aboutName
   *        This will be the string after about: used to refer to this module.
   *        For example, if aboutName is foo, you can refer to this module by
   *        browsing to about:foo.
   *
   * @param uuid
   *        A unique identifer string for this module. For example,
   *        "5f3a921b-250f-4ac5-a61c-8f79372e6063"
   */
  let createAndRegisterAboutModule = function(aboutName, uuid) {

    let AboutModule = function() {};

    AboutModule.prototype = {
      classID: Components.ID(uuid),
      classDescription: `Testing About Module for about:${aboutName}`,
      contractID: `@mozilla.org/network/protocol/about;1?what=${aboutName}`,
      QueryInterface: XPCOMUtils.generateQI([Ci.nsIAboutModule]),

      newChannel: (aURI, aLoadInfo) => {
        return new TestChannel(aURI, aLoadInfo, aboutName);
      },

      getURIFlags: (aURI) => {
        return Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT |
               Ci.nsIAboutModule.ALLOW_SCRIPT;
      },
    };

    let factory = {
      createInstance(outer, iid) {
        if (outer) {
          throw Cr.NS_ERROR_NO_AGGREGATION;
        }
        return new AboutModule();
      },
    };

    Registrar.registerFactory(AboutModule.prototype.classID,
                              AboutModule.prototype.classDescription,
                              AboutModule.prototype.contractID,
                              factory);

    modulesToUnregister.set(AboutModule.prototype.classID,
                            factory);
  };

  /**
   * Unregisters any nsIAboutModules registered with
   * createAndRegisterAboutModule.
   */
  let unregisterModules = () => {
    for (let [classID, factory] of modulesToUnregister) {
      Registrar.unregisterFactory(classID, factory);
    }
  };

  /**
   * Takes a browser, and sends it a framescript to attempt to
   * load some about: pages. The frame script will send a test:result
   * message on completion, passing back a data object with:
   *
   * {
   *   pass: true
   * }
   *
   * on success, and:
   *
   * {
   *   pass: false,
   *   errorMsg: message,
   * }
   *
   * on failure.
   *
   * @param browser
   *        The browser to send the framescript to.
   */
  let testAboutModulesWork = (browser) => {
    let testConnection = () => {
      // This section is loaded into a frame script.
      /* global content:false */
      let request = new content.XMLHttpRequest();
      try {
        request.open("GET", "about:test1", false);
        request.send(null);
        if (request.status != 200) {
          throw (`about:test1 response had status ${request.status} - expected 200`);
        }
        if (request.responseText.indexOf("test1") == -1) {
          throw (`about:test1 response had result ${request.responseText}`);
        }

        request = new content.XMLHttpRequest();
        request.open("GET", "about:test2", false);
        request.send(null);

        if (request.status != 200) {
          throw (`about:test2 response had status ${request.status} - expected 200`);
        }
        if (request.responseText.indexOf("test2") == -1) {
          throw (`about:test2 response had result ${request.responseText}`);
        }

        sendAsyncMessage("test:result", {
          pass: true,
        });
      } catch (e) {
        sendAsyncMessage("test:result", {
          pass: false,
          errorMsg: e.toString(),
        });
      }
    };

    return new Promise((resolve, reject) => {
      let mm = browser.messageManager;
      mm.addMessageListener("test:result", function onTestResult(message) {
        mm.removeMessageListener("test:result", onTestResult);
        if (message.data.pass) {
          ok(true, "Connections to about: pages were successful");
        } else {
          ok(false, message.data.errorMsg);
        }
        resolve();
      });
      mm.loadFrameScript("data:,(" + testConnection.toString() + ")();", false);
    });
  }

  // Here's where the actual test is performed.
  return new Promise((resolve, reject) => {
    createAndRegisterAboutModule("test1", "5f3a921b-250f-4ac5-a61c-8f79372e6063");
    createAndRegisterAboutModule("test2", "d7ec0389-1d49-40fa-b55c-a1fc3a6dbf6f");

    // This needs to be a chrome-privileged page that loads in the
    // content process. It needs chrome privs because otherwise the
    // XHRs for about:test[12] will fail with a privilege error
    // despite the presence of URI_SAFE_FOR_UNTRUSTED_CONTENT.
    let newTab = gBrowser.addTab("chrome://addonshim1/content/page.html");
    gBrowser.selectedTab = newTab;
    let browser = newTab.linkedBrowser;

    addLoadListener(browser, function() {
      testAboutModulesWork(browser).then(() => {
        unregisterModules();
        removeTab(newTab, resolve);
      });
    });
  });
}

function testProgressListener() {
  const url = baseURL + "browser_addonShims_testpage.html";

  let sawGlobalLocChange = false;
  let sawTabsLocChange = false;

  let globalListener = {
    onLocationChange(webProgress, request, uri) {
      if (uri.spec == url) {
        sawGlobalLocChange = true;
        ok(request instanceof Ci.nsIHttpChannel, "Global listener channel is an HTTP channel");
      }
    },
  };

  let tabsListener = {
    onLocationChange(browser, webProgress, request, uri) {
      if (uri.spec == url) {
        sawTabsLocChange = true;
        ok(request instanceof Ci.nsIHttpChannel, "Tab listener channel is an HTTP channel");
      }
    },
  };

  gBrowser.addProgressListener(globalListener);
  gBrowser.addTabsProgressListener(tabsListener);
  info("Added progress listeners");

  return new Promise(function(resolve, reject) {
    let tab = gBrowser.addTab(url);
    gBrowser.selectedTab = tab;
    addLoadListener(tab.linkedBrowser, function handler() {
      ok(sawGlobalLocChange, "Saw global onLocationChange");
      ok(sawTabsLocChange, "Saw tabs onLocationChange");

      gBrowser.removeProgressListener(globalListener);
      gBrowser.removeTabsProgressListener(tabsListener);
      removeTab(tab, resolve);
    });
  });
}

function testRootTreeItem() {
  return new Promise(function(resolve, reject) {
    const url = baseURL + "browser_addonShims_testpage.html";
    let tab = gBrowser.addTab(url);
    gBrowser.selectedTab = tab;
    let browser = tab.linkedBrowser;
    addLoadListener(browser, function handler() {
      let win = browser.contentWindow;

      // Add-ons love this crap.
      let root = win.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                    .getInterface(Components.interfaces.nsIWebNavigation)
                    .QueryInterface(Components.interfaces.nsIDocShellTreeItem)
                    .rootTreeItem
                    .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                    .getInterface(Components.interfaces.nsIDOMWindow);
      is(root, gWin, "got correct chrome window");

      removeTab(tab, resolve);
    });
  });
}

function testImportNode() {
  return new Promise(function(resolve, reject) {
    const url = baseURL + "browser_addonShims_testpage.html";
    let tab = gBrowser.addTab(url);
    gBrowser.selectedTab = tab;
    let browser = tab.linkedBrowser;
    addLoadListener(browser, function handler() {
      let node = gWin.document.createElement("div");
      let doc = browser.contentDocument;
      let result;
      try {
        result = doc.importNode(node, false);
      } catch (e) {
        ok(false, "importing threw an exception");
      }
      if (browser.isRemoteBrowser) {
        is(result, node, "got expected import result");
      }

      removeTab(tab, resolve);
    });
  });
}

function runTests(win, funcs) {
  ok = funcs.ok;
  is = funcs.is;
  info = funcs.info;

  gWin = win;
  gBrowser = win.gBrowser;

  return testContentWindow().
    then(testListeners).
    then(testCapturing).
    then(testObserver).
    then(testSandbox).
    then(testAddonContent).
    then(testAboutModuleRegistration).
    then(testProgressListener).
    then(testRootTreeItem).
    then(testImportNode).
    then(Promise.resolve());
}

/*
 bootstrap.js API
*/

function startup(aData, aReason) {
  forEachWindow(win => {
    win.runAddonShimTests = (funcs) => runTests(win, funcs);
  });
}

function shutdown(aData, aReason) {
  forEachWindow(win => {
    delete win.runAddonShimTests;
  });
}

function install(aData, aReason) {
}

function uninstall(aData, aReason) {
}
