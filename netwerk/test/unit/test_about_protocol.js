/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

var unsafeAboutModule = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAboutModule]),
  newChannel: function (aURI, aLoadInfo) {
    var uri = Services.io.newURI("about:blank");
    let chan = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);
    chan.owner = Services.scriptSecurityManager.getSystemPrincipal();
    return chan;
  },
  getURIFlags: function (aURI) {
    return Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT;
  }
};

var factory = {
  createInstance: function(aOuter, aIID) {
    if (aOuter)
      throw Cr.NS_ERROR_NO_AGGREGATION;
    return unsafeAboutModule.QueryInterface(aIID);
  },
  lockFactory: function(aLock) {
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },
  QueryInterface: ChromeUtils.generateQI([Ci.nsIFactory])
};

function run_test() {
  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  let classID = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID();
  registrar.registerFactory(classID, "", "@mozilla.org/network/protocol/about;1?what=unsafe", factory);

  let aboutUnsafeChan = NetUtil.newChannel({
    uri: "about:unsafe",
    loadUsingSystemPrincipal: true
  });

  Assert.equal(null, aboutUnsafeChan.owner, "URI_SAFE_FOR_UNTRUSTED_CONTENT channel has no owner");

  registrar.unregisterFactory(classID, factory);
}
