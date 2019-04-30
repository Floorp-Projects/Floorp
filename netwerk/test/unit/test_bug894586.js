/*
 * Tests for bug 894586: nsSyncLoadService::PushSyncStreamToListener
 * should not fail for channels of unknown size
 */


var contentSecManager = Cc["@mozilla.org/contentsecuritymanager;1"]
                          .getService(Ci.nsIContentSecurityManager);

function ProtocolHandler() {
  this.uri = Cc["@mozilla.org/network/simple-uri-mutator;1"]
               .createInstance(Ci.nsIURIMutator)
               .setSpec(this.scheme + ":dummy")
               .finalize();
}

ProtocolHandler.prototype = {
  /** nsIProtocolHandler */
  get scheme() {
    return "x-bug894586";
  },
  get defaultPort() {
    return -1;
  },
  get protocolFlags() {
    return Ci.nsIProtocolHandler.URI_NORELATIVE |
           Ci.nsIProtocolHandler.URI_NOAUTH |
           Ci.nsIProtocolHandler.URI_IS_UI_RESOURCE |
           Ci.nsIProtocolHandler.URI_IS_LOCAL_RESOURCE |
           Ci.nsIProtocolHandler.URI_NON_PERSISTABLE |
           Ci.nsIProtocolHandler.URI_SYNC_LOAD_IS_OK;
  },
  newURI: function(aSpec, aOriginCharset, aBaseURI) {
    return this.uri;
  },
  newChannel: function(aURI, aLoadInfo) {
    this.loadInfo = aLoadInfo;
    return this;
  },
  allowPort: function(port, scheme) {
    return port != -1;
  },

  /** nsIChannel */
  get originalURI() {
    return this.uri;
  },
  get URI() {
    return this.uri;
  },
  owner: null,
  notificationCallbacks: null,
  get securityInfo() {
    return null;
  },
  get contentType() {
    return "text/css";
  },
  set contentType(val) {
  },
  contentCharset: "UTF-8",
  get contentLength() {
    return -1;
  },
  set contentLength(val) {
    throw Components.Exception("Setting content length", NS_ERROR_NOT_IMPLEMENTED);
  },
  open: function() {
    // throws an error if security checks fail
    contentSecManager.performSecurityCheck(this, null);

    var file = do_get_file("test_bug894586.js", false);
    Assert.ok(file.exists());
    var url = Services.io.newFileURI(file);
    return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true}).open();
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
  get name() {
    return this.uri.spec;
  },
  isPending: () => false,
  get status() {
    return Cr.NS_OK;
  },
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
  QueryInterface: ChromeUtils.generateQI([Ci.nsIProtocolHandler,
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
    Assert.ok(ss.sheetRegistered(handler.uri, Ci.nsIStyleSheetService.AGENT_SHEET));
  } finally {
    registrar.unregisterFactory(handler.classID, handler);
  }
}

// vim: set et ts=2 :

