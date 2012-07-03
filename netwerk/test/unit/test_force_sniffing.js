// This file tests the flag LOAD_TREAT_APPLICATION_OCTET_STREAM_AS_UNKNOWN.

do_load_httpd_js();

const octetStreamType = "application/octet-stream";
const sniffedType = "application/x-sniffed";

const snifferCID = Components.ID("{954f3fdd-d717-4c02-9464-7c2da617d21d}");
const snifferContract = "@mozilla.org/network/unittest/contentsniffer;1";
const categoryName = "content-sniffing-services";

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
    // We should have sniffed the type of the file.
    var chan = request.QueryInterface(Components.interfaces.nsIChannel);
    do_check_eq(chan.contentType, sniffedType);
  },

  onDataAvailable: function test_ODA() {
    throw Components.results.NS_ERROR_UNEXPECTED;
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    do_test_finished();
  }
};

function handler(metadata, response) {
  response.setHeader("Content-Type", octetStreamType);
}

function makeChan(url) {
  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel(url, null, null);
  // Force sniffing if we have "application/octet-stream" as Content-Type
  chan.loadFlags |= Components.interfaces
                              .nsIChannel
                              .LOAD_TREAT_APPLICATION_OCTET_STREAM_AS_UNKNOWN;

  return chan;
}

var url = "http://localhost:4444/test";

var httpserv = null;

function run_test() {
  httpserv = new nsHttpServer();
  httpserv.registerPathHandler("/test", handler);
  httpserv.start(4444);

  // Register our fake sniffer that always returns the content-type we want.
  Components.manager.nsIComponentRegistrar.registerFactory(snifferCID,
                       "Unit test content sniffer", snifferContract, sniffer);

  var catMan = Components.classes["@mozilla.org/categorymanager;1"]
                         .getService(Components.interfaces.nsICategoryManager);
  catMan.nsICategoryManager.addCategoryEntry(categoryName, snifferContract,
                                             "unit test", false, true);

  var chan = makeChan(url);
  chan.asyncOpen(listener, null);

  do_test_pending();
}

