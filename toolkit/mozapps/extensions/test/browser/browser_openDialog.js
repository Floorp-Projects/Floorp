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

  newChannel: function CCP_newChannel(aURI) {
    let url = "chrome:" + aURI.path;
    let ch = NetUtil.newChannel(url);
    ch.originalURI = aURI;
    return ch;
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
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      }
      return CustomChromeProtocol.QueryInterface(aIID);
    },

    lockFactory: function BNPH_lockFactory(aLock) {
      throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
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
    let lastAddonIndex = -1;

    function getAddonByName(aName) {
      for (let i = 0; i < addonList.childNodes.length; i++) {
        let addonItem = addonList.childNodes[i];
        if (addonItem.hasAttribute("name") &&
            addonItem.getAttribute("name") == aName)
          return addonItem;
      }
      return null;
    }

    let winClosedCount = 0;
    Services.ww.registerNotification(function (aSubject, aTopic, aData) {
      switch (aTopic) {
        case "domwindowclosed":
          if (++winClosedCount == 2) {
            Services.ww.unregisterNotification(arguments.callee);
            // Give the preference window a chance to finish closing before
            // closing the add-ons manager.
            waitForFocus(function () {
              close_manager(aManager, finish);
            });
          }
          break;
        case "domwindowopened":
          let win = aSubject.QueryInterface(Ci.nsIDOMEventTarget);
          waitForFocus(function () {
            // If the openDialog privileges are wrong a new browser window
            // will open, let the test proceed (and fail) rather than timeout.
            if (win.location != ADDONS_LIST[lastAddonIndex].optionsURL &&
                win.location != "chrome://browser/content/browser.xul")
              return;

            is(win.location, ADDONS_LIST[lastAddonIndex].optionsURL,
               "The correct addon pref window should have opened");

            let chromeFlags = win.QueryInterface(Ci.nsIInterfaceRequestor).
                                  getInterface(Ci.nsIWebNavigation).
                                  QueryInterface(Ci.nsIDocShellTreeItem).treeOwner.
                                  QueryInterface(Ci.nsIInterfaceRequestor).
                                  getInterface(Ci.nsIXULWindow).chromeFlags;
            ok(chromeFlags & Ci.nsIWebBrowserChrome.CHROME_OPENAS_CHROME &&
               chromeFlags & Ci.nsIWebBrowserChrome.CHROME_OPENAS_DIALOG,
               "Window was open as a chrome dialog.");

            win.close();
          }, win);
          break;
      }
    });

    ADDONS_LIST.forEach(function (aAddon, aIndex) {
      lastAddonIndex = aIndex;
      info("Testing addon " + aIndex);
      let addonItem = getAddonByName(aAddon.name, addonList);
      let optionsBtn =
        aManager.document.getAnonymousElementByAttribute(addonItem, "anonid",
                                                         "preferences-btn");
      is(optionsBtn.hidden, false, "Prefs button should be visible.")

      addonList.ensureElementIsVisible(addonItem);
      EventUtils.synthesizeMouseAtCenter(optionsBtn, { }, aManager);
    });
  });

}
