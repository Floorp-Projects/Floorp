// This file tests nsIContentSniffer, introduced in bug 324985

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/NetUtil.jsm");

const unknownType = "application/x-unknown-content-type";
const sniffedType = "application/x-sniffed";

const snifferCID = Components.ID("{4c93d2db-8a56-48d7-b261-9cf2a8d998eb}");
const snifferContract = "@mozilla.org/network/unittest/contentsniffer;1";
const categoryName = "net-content-sniffers";

var sniffing_enabled = true;

/**
 * This object is both a factory and an nsIContentSniffer implementation (so, it
 * is de-facto a service)
 */
var sniffer = {
  QueryInterface: function sniffer_qi(iid) {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsIFactory) ||
        iid.equals(Components.interfaces.nsIContentSniffer))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  createInstance: function sniffer_ci(outer, iid) {
    if (outer)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  lockFactory: function sniffer_lockf(lock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  getMIMETypeFromContent: function (request, data, length) {
    return sniffedType;
  }
};

var listener = {
  onStartRequest: function test_onStartR(request, ctx) {
    try {
      var chan = request.QueryInterface(Components.interfaces.nsIChannel);
      if (chan.contentType == unknownType)
        do_throw("Type should not be unknown!");
      if (sniffing_enabled && this._iteration > 2 &&
          chan.contentType != sniffedType) {
        do_throw("Expecting <" + sniffedType +"> but got <" +
                 chan.contentType + "> for " + chan.URI.spec);
      } else if (!sniffing_enabled && chan.contentType == sniffedType) {
        do_throw("Sniffing not enabled but sniffer called for " + chan.URI.spec);
      }
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    throw Components.results.NS_ERROR_ABORT;
  },

  onDataAvailable: function test_ODA() {
    throw Components.results.NS_ERROR_UNEXPECTED;
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    run_test_iteration(this._iteration);
    do_test_finished();
  },

  _iteration: 1
};

function makeChan(url) {
  var chan = NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true});
  if (sniffing_enabled)
    chan.loadFlags |= Components.interfaces.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS;

  return chan;
}

var httpserv = null;
var urls = null;

function run_test() {
  httpserv = new HttpServer();
  httpserv.start(-1);

  urls = [
  // NOTE: First URL here runs without our content sniffer
  "data:" + unknownType + ", Some text",
  "data:" + unknownType + ", Text", // Make sure sniffing works even if we
                                    // used the unknown content sniffer too
  "data:text/plain, Some more text",
    "http://localhost:" + httpserv.identity.primaryPort
];

  Components.manager.nsIComponentRegistrar.registerFactory(snifferCID,
    "Unit test content sniffer", snifferContract, sniffer);

  run_test_iteration(1);
}

function run_test_iteration(index) {
  if (index > urls.length) {
    if (sniffing_enabled) {
        sniffing_enabled = false;
        index = listener._iteration = 1;
    } else {
        do_test_pending();
        httpserv.stop(do_test_finished);
        return; // we're done
    }
  }

  if (sniffing_enabled && index == 2) {
    // Register our sniffer only here
    // This also makes sure that dynamic registration is working
    var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);
    catMan.nsICategoryManager.addCategoryEntry(categoryName, "unit test",
                                               snifferContract, false, true);
  }

  var chan = makeChan(urls[index - 1]);

  listener._iteration++;
  chan.asyncOpen2(listener);

  do_test_pending();
}

