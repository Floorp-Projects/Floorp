/* eslint-env mozilla/process-script */

const { ComponentUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ComponentUtils.sys.mjs"
);

const WEBEXTENSION_ID = "tabswitch-talos@mozilla.org";
const ABOUT_PAGE_NAME = "tabswitch";
const Registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
const UUID = "0f459ab4-b4ba-4741-ac89-ee47dea07adb";
const ABOUT_PATH_PATH = "content/test.html";

const { WebExtensionPolicy } = Cu.getGlobalForObject(Services);

const TPSProcessScript = {
  init() {
    let extensionPolicy = WebExtensionPolicy.getByID(WEBEXTENSION_ID);
    let aboutPageURI = extensionPolicy.getURL(ABOUT_PATH_PATH);

    class TabSwitchAboutModule {
      constructor() {
        this.QueryInterface = ChromeUtils.generateQI(["nsIAboutModule"]);
      }
      newChannel(aURI, aLoadInfo) {
        let uri = Services.io.newURI(aboutPageURI);
        let chan = Services.io.newChannelFromURIWithLoadInfo(uri, aLoadInfo);
        chan.originalURI = aURI;
        return chan;
      }
      getURIFlags(aURI) {
        return (
          Ci.nsIAboutModule.ALLOW_SCRIPT |
          Ci.nsIAboutModule.URI_MUST_LOAD_IN_CHILD |
          Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT
        );
      }
    }

    let factory = ComponentUtils.generateSingletonFactory(TabSwitchAboutModule);
    this._factory = factory;

    Registrar.registerFactory(
      Components.ID(UUID),
      "",
      `@mozilla.org/network/protocol/about;1?what=${ABOUT_PAGE_NAME}`,
      factory
    );

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
    if (msg.name == "Tabswitch:Teardown") {
      this.teardown();
    }
  },
};

TPSProcessScript.init();
