const {classes: Cc, utils: Cu, interfaces: Ci} = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const CHROME_URI = "chrome://tabswitch/content/test.html";

class TabSwitchAboutModule {
  constructor() {
    this.QueryInterface = XPCOMUtils.generateQI([Ci.nsIAboutModule]);
  }

  newChannel(aURI, aLoadInfo) {
    let uri = Services.io.newURI(CHROME_URI, null, null);
    let chan = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);
    chan.originalURI = aURI;
    return chan;
  }

  getURIFlags(aURI) {
    return Ci.nsIAboutModule.ALLOW_SCRIPT |
           Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD;
  }
}

let factory = XPCOMUtils._getFactory(TabSwitchAboutModule);
let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
let UUIDGenerator = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

registrar.registerFactory(UUIDGenerator.generateUUID(), "",
                          "@mozilla.org/network/protocol/about;1?what=tabswitch",
                          factory);
