/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests the dialog open by the Options button for addons that provide a
// custom chrome-like protocol for optionsURL.

let CustomChromeProtocol = {
  scheme: "khrome",
  defaultPort: -1,
  protocolFlags: Ci.nsIProtocolHandler.URI_DANGEROUS_TO_LOAD |
                 Ci.nsIProtocolHandler.URI_IS_LOCAL_RESOURCE |
                 Ci.nsIProtocolHandler.URI_NORELATIVE |
                 Ci.nsIProtocolHandler.URI_NOAUTH,

  newURI: function CCP_newURI(aSpec, aOriginCharset, aBaseUri) {
    let uri = Cc["@mozilla.org/network/simple-uri;1"].
              createInstance(Ci.nsIURI);
    uri.spec = aSpec;
    return uri;
  },

  newChannel2: function CCP_newChannel2(aURI, aLoadInfo) {
    let url = Services.io.newURI("chrome:" + aURI.path, null, null);
    let ch = Services.io.newChannelFromURIWithLoadInfo(url, aLoadInfo);
    ch.originalURI = aURI;
    return ch;
  },

  newChannel: function CCP_newChannel(aURI) {
    return this.newChannel2(aURI, null);
  },

  allowPort: function CCP_allowPort(aPort, aScheme) {
    return false;
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIProtocolHandler
  ]),

  classID: Components.ID("{399cb2d1-05dd-4363-896f-63b78e008cf8}"),

  factory: {
    registrar: Components.manager.QueryInterface(Ci.nsIComponentRegistrar),

    register: function CCP_register() {
      this.registrar.registerFactory(
        CustomChromeProtocol.classID,
        "CustomChromeProtocol",
        "@mozilla.org/network/protocol;1?name=khrome",
        this
      );
    },

    unregister: function CCP_register() {
      this.registrar.unregisterFactory(CustomChromeProtocol.classID, this);
    },

    // nsIFactory
    createInstance: function BNPH_createInstance(aOuter, aIID) {
      if (aOuter) {
        throw Components.Exception("Class does not allow aggregation",
                                   Components.results.NS_ERROR_NO_AGGREGATION);
      }
      return CustomChromeProtocol.QueryInterface(aIID);
    },

    lockFactory: function BNPH_lockFactory(aLock) {
      throw Components.Exception("Function lockFactory is not implemented",
                                 Components.results.NS_ERROR_NOT_IMPLEMENTED);
    },

    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIFactory
    ])
  }
}

function test() {
  waitForExplicitFinish();
  requestLongerTimeout(2);

  info("Registering custom chrome-like protocol.");
  CustomChromeProtocol.factory.register();
  registerCleanupFunction(function () CustomChromeProtocol.factory.unregister());

  const ADDONS_LIST = [
    { id: "test1@tests.mozilla.org",
      name: "Test add-on 1",
      optionsURL: CHROMEROOT + "addon_prefs.xul" },
    { id: "test2@tests.mozilla.org",
      name: "Test add-on 2",
      optionsURL: (CHROMEROOT + "addon_prefs.xul").replace("chrome:", "khrome:") },
  ];

  var gProvider = new MockProvider();
  gProvider.createAddons(ADDONS_LIST);

  open_manager("addons://list/extension", function(aManager) {
    let addonList = aManager.document.getElementById("addon-list");
    let currentAddon;
    let instantApply = Services.prefs.getBoolPref("browser.preferences.instantApply");

    function getAddonByName(aName) {
      for (let addonItem of addonList.childNodes) {
        if (addonItem.hasAttribute("name") &&
            addonItem.getAttribute("name") == aName)
          return addonItem;
      }
      return null;
    }

    function observer(aSubject, aTopic, aData) {
      switch (aTopic) {
        case "domwindowclosed":
          // Give the preference window a chance to finish closing before
          // closing the add-ons manager.
          waitForFocus(function () {
            test_next_addon();
          });
          break;
        case "domwindowopened":
          let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
          waitForFocus(function () {
            // If the openDialog privileges are wrong a new browser window
            // will open, let the test proceed (and fail) rather than timeout.
            if (win.location != currentAddon.optionsURL &&
                win.location != "chrome://browser/content/browser.xul")
              return;

            is(win.location, currentAddon.optionsURL,
               "The correct addon pref window should have opened");

            let chromeFlags = win.QueryInterface(Ci.nsIInterfaceRequestor).
                                  getInterface(Ci.nsIWebNavigation).
                                  QueryInterface(Ci.nsIDocShellTreeItem).treeOwner.
                                  QueryInterface(Ci.nsIInterfaceRequestor).
                                  getInterface(Ci.nsIXULWindow).chromeFlags;
            ok(chromeFlags & Ci.nsIWebBrowserChrome.CHROME_OPENAS_CHROME &&
               (instantApply || chromeFlags & Ci.nsIWebBrowserChrome.CHROME_OPENAS_DIALOG),
               "Window was open as a chrome dialog.");

            win.close();
          }, win);
          break;
      }
    }

    function test_next_addon() {
      currentAddon = ADDONS_LIST.shift();
      if (!currentAddon) {
        Services.ww.unregisterNotification(observer);
        close_manager(aManager, finish);
        return;
      }

      info("Testing " + currentAddon.name);
      let addonItem = getAddonByName(currentAddon.name, addonList);
      let optionsBtn =
        aManager.document.getAnonymousElementByAttribute(addonItem, "anonid",
                                                         "preferences-btn");
      is(optionsBtn.hidden, false, "Prefs button should be visible.")

      addonList.ensureElementIsVisible(addonItem);
      EventUtils.synthesizeMouseAtCenter(optionsBtn, { }, aManager);
    }

    Services.ww.registerNotification(observer);
    test_next_addon();
  });

}
