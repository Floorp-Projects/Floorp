// This file tests channel event sinks (bug 315598 et al)

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserv.identity.primaryPort;
});

const sinkCID = Components.ID("{14aa4b81-e266-45cb-88f8-89595dece114}");
const sinkContract = "@mozilla.org/network/unittest/channeleventsink;1";

const categoryName = "net-channel-event-sinks";

/**
 * This object is both a factory and an nsIChannelEventSink implementation (so, it
 * is de-facto a service). It's also an interface requestor that gives out
 * itself when asked for nsIChannelEventSink.
 */
var eventsink = {
  QueryInterface: function eventsink_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIFactory) ||
        iid.equals(Ci.nsIChannelEventSink))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },
  createInstance: function eventsink_ci(outer, iid) {
    if (outer)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  lockFactory: function eventsink_lockf(lock) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  asyncOnChannelRedirect: function eventsink_onredir(oldChan, newChan, flags, callback) {
    // veto
    this.called = true;
    throw Cr.NS_BINDING_ABORTED;
  },

  getInterface: function eventsink_gi(iid) {
    if (iid.equals(Ci.nsIChannelEventSink))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  called: false
};

var listener = {
  expectSinkCall: true,

  onStartRequest: function test_onStartR(request) {
    try {
      // Commenting out this check pending resolution of bug 255119
      //if (Components.isSuccessCode(request.status))
      //  do_throw("Channel should have a failure code!");

      // The current URI must be the original URI, as all redirects have been
      // cancelled
      if (!(request instanceof Ci.nsIChannel) ||
          !request.URI.equals(request.originalURI))
        do_throw("Wrong URI: Is <" + request.URI.spec + ">, should be <" +
                 request.originalURI.spec + ">");

      if (request instanceof Ci.nsIHttpChannel) {
        // As we expect a blocked redirect, verify that we have a 3xx status
        Assert.equal(Math.floor(request.responseStatus / 100), 3);
        Assert.equal(request.requestSucceeded, false);
      }

      Assert.equal(eventsink.called, this.expectSinkCall);
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    throw Cr.NS_ERROR_ABORT;
  },

  onDataAvailable: function test_ODA() {
    do_throw("Should not get any data!");
  },

  onStopRequest: function test_onStopR(request, status) {
    if (this._iteration <= 2) {
      run_test_continued();
    } else {
      do_test_pending();
      httpserv.stop(do_test_finished);
    }
    do_test_finished();
  },

  _iteration: 1
};

function makeChan(url) {
  return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true});
}

var httpserv = null;

function run_test() {
  httpserv = new HttpServer();
  httpserv.registerPathHandler("/redirect", redirect);
  httpserv.registerPathHandler("/redirectfile", redirectfile);
  httpserv.start(-1);

  Components.manager.nsIComponentRegistrar.registerFactory(sinkCID,
    "Unit test Event sink", sinkContract, eventsink);

  // Step 1: Set the callbacks on the listener itself
  var chan = makeChan(URL + "/redirect");
  chan.notificationCallbacks = eventsink;

  chan.asyncOpen(listener);

  do_test_pending();
}

function run_test_continued() {
  eventsink.called = false;

  var catMan = Cc["@mozilla.org/categorymanager;1"]
                 .getService(Ci.nsICategoryManager);

  var chan;
  if (listener._iteration == 1) {
    // Step 2: Category entry
    catMan.nsICategoryManager.addCategoryEntry(categoryName, "unit test",
                                               sinkContract, false, true);
    chan = makeChan(URL + "/redirect")
  } else {
    // Step 3: Global contract id
    catMan.nsICategoryManager.deleteCategoryEntry(categoryName, "unit test",
                                                  false);
    listener.expectSinkCall = false;
    chan = makeChan(URL + "/redirectfile");
  }

  listener._iteration++;
  chan.asyncOpen(listener);

  do_test_pending();
}

// PATHS

// /redirect
function redirect(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 301, "Moved Permanently");
  response.setHeader("Location",
                     "http://localhost:" + metadata.port + "/",
                     false);

  var body = "Moved\n";
  response.bodyOutputStream.write(body, body.length);
}

// /redirectfile
function redirectfile(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 301, "Moved Permanently");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Location", "file:///etc/", false);

  var body = "Attempted to move to a file URI, but failed.\n";
  response.bodyOutputStream.write(body, body.length);
}
