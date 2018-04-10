ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const WEBEXTENSION_ID = "tabswitch-talos@mozilla.org";
const ABOUT_PAGE_NAME = "tabswitch";
const Registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
const UUID = "0f459ab4-b4ba-4741-ac89-ee47dea07adb";
const ABOUT_PATH_PATH = "content/test.html";

XPCOMUtils.defineLazyGetter(
  this, "processScript",
  () => Cc["@mozilla.org/webextensions/extension-process-script;1"]
          .getService().wrappedJSObject);

const TPSProcessScript = {
  init() {
    let extensionChild = processScript.getExtensionChild(WEBEXTENSION_ID);
    let aboutPageURI = extensionChild.baseURI.resolve(ABOUT_PATH_PATH);

    class TabSwitchAboutModule {
      constructor() {
        this.QueryInterface = XPCOMUtils.generateQI([Ci.nsIAboutModule]);
      }
      newChannel(aURI, aLoadInfo) {
        let uri = Services.io.newURI(aboutPageURI);
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
    this._factory = factory;

    Registrar.registerFactory(
      Components.ID(UUID), "",
      `@mozilla.org/network/protocol/about;1?what=${ABOUT_PAGE_NAME}`,
      factory);

    this._hasSetup = true;
  },

  teardown() {
    if (!this._hasSetup) {
      return;
    }

    Registrar.unregisterFactory(Components.ID(UUID), this._factory);
    this._hasSetup = false;
    this._factory = null;
  },

  receiveMessage(msg) {
    if (msg.name == "TPS:Teardown") {
      this.teardown();
    }
  }
};

TPSProcessScript.init();
