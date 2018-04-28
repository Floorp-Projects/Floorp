"use strict";

var Cm = Components.manager;

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const { generateUUID } = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

function AboutPage(aboutHost, chromeURL, uriFlags) {
  this.chromeURL = chromeURL;
  this.aboutHost = aboutHost;
  this.classID = Components.ID(generateUUID().number);
  this.description = "BrowserTestUtils: " + aboutHost;
  this.uriFlags = uriFlags;
}

AboutPage.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIAboutModule]),
  getURIFlags(aURI) { // eslint-disable-line no-unused-vars
    return this.uriFlags;
  },

  newChannel(aURI, aLoadInfo) {
    let newURI = Services.io.newURI(this.chromeURL);
    let channel = Services.io.newChannelFromURIWithLoadInfo(newURI,
                                                            aLoadInfo);
    channel.originalURI = aURI;

    if (this.uriFlags & Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT) {
      channel.owner = null;
    }
    return channel;
  },

  createInstance(outer, iid) {
    if (outer !== null) {
      throw Cr.NS_ERROR_NO_AGGREGATION;
    }
    return this.QueryInterface(iid);
  },

  register() {
    Cm.QueryInterface(Ci.nsIComponentRegistrar).registerFactory(
      this.classID, this.description,
      "@mozilla.org/network/protocol/about;1?what=" + this.aboutHost, this);
  },

  unregister() {
    Cm.QueryInterface(Ci.nsIComponentRegistrar).unregisterFactory(
      this.classID, this);
  }
};

const gRegisteredPages = new Map();

addMessageListener("browser-test-utils:about-registration:register", msg => {
  let {aboutModule, pageURI, flags} = msg.data;
  if (gRegisteredPages.has(aboutModule)) {
    gRegisteredPages.get(aboutModule).unregister();
  }
  let moduleObj = new AboutPage(aboutModule, pageURI, flags);
  moduleObj.register();
  gRegisteredPages.set(aboutModule, moduleObj);
  sendAsyncMessage("browser-test-utils:about-registration:registered", aboutModule);
});

addMessageListener("browser-test-utils:about-registration:unregister", msg => {
  let aboutModule = msg.data;
  let moduleObj = gRegisteredPages.get(aboutModule);
  if (moduleObj) {
    moduleObj.unregister();
    gRegisteredPages.delete(aboutModule);
  }
  sendAsyncMessage("browser-test-utils:about-registration:unregistered", aboutModule);
});
