// Utilities for running tests in an e10s environment.

// There are some tricks/shortcuts that test code takes that we don't see
// in the real browser code.  These include setting content.location.href
// (which doesn't work in test code with e10s enabled as the document object
// is yet to be created), waiting for certain events the main browser doesn't
// care about and so doesn't normally get special support, eg, the "pageshow"
// or "load" events).
// So we make some hacks to pretend these work in the test suite.

// Ideally all these hacks could be removed, but this can only happen when
// the tests are refactored to not use these tricks.  But this would be a huge
// amount of work and is unlikely to happen anytime soon...

const CONTENT_URL = "chrome://mochikit/content/mochitest-e10s-utils-content.js";

// This is an object that is used as the "location" on a remote document or
// window.  It will be overwritten as the real document and window are made
// available.
let locationStub = function(browser) {
  this.browser = browser;
};
locationStub.prototype = {
  get href() {
    return this.browser.webNavigation.currentURI.spec;
  },
  set href(val) {
    this.browser.loadURI(val);
  },
  assign: function(url) {
    this.href = url;
  }
};

// This object is used in place of contentWindow while we wait for it to be
// overwritten as the real window becomes available.
let TemporaryWindowStub = function(browser) {
  this._locationStub = new locationStub(browser);
};

TemporaryWindowStub.prototype = {
  // save poor developers from getting confused about why the window isn't
  // working like a window should..
  toString: function() {
    return "[Window Stub for e10s tests]";
  },
  get location() {
    return this._locationStub;
  },
  set location(val) {
    this._locationStub.href = val;
  },
  get document() {
    // so tests can say: document.location....
    return this;
  }
};

// An observer called when a new remote browser element is created.  We replace
// the _contentWindow in new browsers with our TemporaryWindowStub object.
function observeNewFrameloader(subject, topic, data) {
  let browser = subject.QueryInterface(Ci.nsIFrameLoader).ownerElement;
  browser._contentWindow = new TemporaryWindowStub(browser);
}

function e10s_init() {
  // Use the global message manager to inject a content script into all browsers.
  let globalMM = Cc["@mozilla.org/globalmessagemanager;1"]
                   .getService(Ci.nsIMessageListenerManager);
  globalMM.loadFrameScript(CONTENT_URL, true);
  globalMM.addMessageListener("Test:Event", function(message) {
    let event = document.createEvent('HTMLEvents');
    event.initEvent(message.data.name, true, true, {});
    message.target.dispatchEvent(event);
  });

  // We add an observer so we can notice new <browser> elements created
  Services.obs.addObserver(observeNewFrameloader, "remote-browser-shown", false);

  // Listen for an 'oop-browser-crashed' event and log it so people analysing
  // test logs have a clue about what is going on.
  window.addEventListener("oop-browser-crashed", (event) => {
    let uri = event.target.currentURI;
    Cu.reportError("remote browser crashed while on " +
                   (uri ? uri.spec : "<unknown>") + "\n");
  }, true);
}
