/*
 * Tests for bug 894586: nsSyncLoadService::PushSyncStreamToListener
 * should not fail for channels of unknown size
 */

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function ProtocolHandler() {
  this.uri = Cc["@mozilla.org/network/simple-uri;1"].
               createInstance(Ci.nsIURI);
  this.uri.spec = this.scheme + ":dummy";
  this.uri.QueryInterface(Ci.nsIMutable).mutable = false;
}

ProtocolHandler.prototype = {
  /** nsIProtocolHandler */
  get scheme() "x-bug894586",
  get defaultPort() -1,
  get protocolFlags() Ci.nsIProtocolHandler.URI_NORELATIVE |
                      Ci.nsIProtocolHandler.URI_NOAUTH |
                      Ci.nsIProtocolHandler.URI_IS_UI_RESOURCE |
                      Ci.nsIProtocolHandler.URI_IS_LOCAL_RESOURCE |
                      Ci.nsIProtocolHandler.URI_NON_PERSISTABLE |
                      Ci.nsIProtocolHandler.URI_SYNC_LOAD_IS_OK,
  newURI: function(aSpec, aOriginCharset, aBaseURI) this.uri,
  newChannel: function(aURI) this,
  allowPort: function(port, scheme) port != -1,

  /** nsIChannel */
  get originalURI() this.uri,
  get URI() this.uri,
  owner: null,
  notificationCallbacks: null,
  get securityInfo() null,
  get contentType() "text/css",
  set contentType(val) void(0),
  contentCharset: "UTF-8",
  get contentLength() -1,
  set contentLength(val) {
    throw Components.Exception("Setting content length", NS_ERROR_NOT_IMPLEMENTED);
  },
  open: function() {
    var url = Services.io.newURI("resource://app/chrome.manifest", null, null);
    do_check_true(url.QueryInterface(Ci.nsIFileURL).file.exists());
    return Services.io.newChannelFromURI(url).open();
  },
  asyncOpen: function(aListener, aContext) {
    throw Components.Exception("Not implemented",
                               Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
  contentDisposition: Ci.nsIChannel.DISPOSITION_INLINE,
  get contentDispositionFilename() {
    throw Components.Exception("No file name",
                               Cr.NS_ERROR_NOT_AVAILABLE);
  },
  get contentDispositionHeader() {
    throw Components.Exception("No header",
                               Cr.NS_ERROR_NOT_AVAILABLE);
  },

  /** nsIRequest */
  get name() this.uri.spec,
  isPending: function() false,
  get status() Cr.NS_OK,
  cancel: function(status) {},
  loadGroup: null,
  loadFlags: Ci.nsIRequest.LOAD_NORMAL |
             Ci.nsIRequest.INHIBIT_CACHING |
             Ci.nsIRequest.LOAD_BYPASS_CACHE,

  /** nsIFactory */
  createInstance: function(aOuter, aIID) {
    if (aOuter) {
      throw Components.Exception("createInstance no aggregation",
                                 Cr.NS_ERROR_NO_AGGREGATION);
    }
    return this.QueryInterface(aIID);
  },
  lockFactory: function() {},

  /** nsISupports */
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIProtocolHandler,
                                         Ci.nsIRequest,
                                         Ci.nsIChannel,
                                         Ci.nsIFactory]),
  classID: Components.ID("{16d594bc-d9d8-47ae-a139-ea714dc0c35c}")
};

/**
 * Attempt a sync load; we use the stylesheet service to do this for us,
 * based on the knowledge that it forces a sync load under the hood.
 */
function run_test()
{
  var handler = new ProtocolHandler();
  var registrar = Components.manager.
                             QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(handler.classID, "",
                            "@mozilla.org/network/protocol;1?name=" + handler.scheme,
                            handler);
  try {
    var ss = Cc["@mozilla.org/content/style-sheet-service;1"].
               getService(Ci.nsIStyleSheetService);
    ss.loadAndRegisterSheet(handler.uri, Ci.nsIStyleSheetService.AGENT_SHEET);
    do_check_true(ss.sheetRegistered(handler.uri, Ci.nsIStyleSheetService.AGENT_SHEET));
  } finally {
    registrar.unregisterFactory(handler.classID, handler);
  }
}

// vim: set et ts=2 :

