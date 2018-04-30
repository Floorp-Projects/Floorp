/*
 * Tests for bug 1351443: NewChannel2 should only fallback to NewChannel if
 * NewChannel2() is unimplemented.
 */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

var contentSecManager = Cc["@mozilla.org/contentsecuritymanager;1"]
                          .getService(Ci.nsIContentSecurityManager);

function ProtocolHandler() {
  this.uri = Cc["@mozilla.org/network/simple-uri-mutator;1"]
               .createInstance(Ci.nsIURIMutator)
               .setSpec(this.scheme + ":dummy")
               .finalize();
  this.uri.QueryInterface(Ci.nsIMutable).mutable = false;
}

ProtocolHandler.prototype = {
  /** nsIProtocolHandler */
  get scheme() {
    return "x-1351443";
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
  newChannel2: function(aURI, aLoadInfo) {
    this.loadInfo = aLoadInfo;
    return this;
  },
  newChannel2_not_implemented: function(aURI, aLoadInfo) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  newChannel2_failure: function(aURI, aLoadInfo) {
    throw Cr.NS_ERROR_FAILURE;
  },
  newChannel: function(aURI) {
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
    var file = do_get_file("test_bug894586.js", false);
    Assert.ok(file.exists());
    var url = Services.io.newFileURI(file);
    return NetUtil.newChannel({uri: url, loadUsingSystemPrincipal: true}).open2();
  },
  open2: function() {
    // throws an error if security checks fail
    contentSecManager.performSecurityCheck(this, null);
    return this.open();
  },
  asyncOpen: function(aListener, aContext) {
    throw Components.Exception("Not implemented",
                               Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
  asyncOpen2: function(aListener, aContext) {
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
  classID: Components.ID("{accbaf4a-2fd9-47e7-8aad-8c19fc5265b5}")
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

  // The default implementation of NewChannel2 should work.
  var channel = NetUtil.newChannel({
    uri: handler.uri,
    loadUsingSystemPrincipal: true
  });
  ok(channel, "channel exists");
  channel = null;

  // If the method throws NS_ERROR_NOT_IMPLEMENTED, it should fall back on newChannel()
  handler.newChannel2 = handler.newChannel2_not_implemented;
  channel = NetUtil.newChannel({
    uri: handler.uri,
    loadUsingSystemPrincipal: true
  });
  ok(channel, "channel exists");
  channel = null;

  // If the method is not defined (the error code will be
  // NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED) it should fall back on newChannel()
  handler.newChannel2 = undefined;
  channel = NetUtil.newChannel({
    uri: handler.uri,
    loadUsingSystemPrincipal: true
  });
  ok(channel, "channel exists");
  channel = null;

  // If the method simply throws an error code, it should not fall back on newChannel()
  // so we make sure it throws.
  handler.newChannel2 = handler.newChannel2_failure;
  Assert.throws(() => {
    channel = NetUtil.newChannel({
      uri: handler.uri,
      loadUsingSystemPrincipal: true
    });
  }, /Failure/, "If the channel returns an error code, it should throw");
  ok(!channel, "channel should not exist");
}
