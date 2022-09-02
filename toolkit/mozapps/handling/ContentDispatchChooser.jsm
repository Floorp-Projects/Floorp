/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Constants

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);

const DIALOG_URL_APP_CHOOSER =
  "chrome://mozapps/content/handling/appChooser.xhtml";
const DIALOG_URL_PERMISSION =
  "chrome://mozapps/content/handling/permissionDialog.xhtml";

var EXPORTED_SYMBOLS = ["nsContentDispatchChooser"];

const gPrefs = {};
XPCOMUtils.defineLazyPreferenceGetter(
  gPrefs,
  "promptForExternal",
  "network.protocol-handler.prompt-from-external",
  true
);

const PROTOCOL_HANDLER_OPEN_PERM_KEY = "open-protocol-handler";
const PERMISSION_KEY_DELIMITER = "^";

class nsContentDispatchChooser {
  /**
   * Prompt the user to open an external application.
   * If the triggering principal doesn't have permission to open apps for the
   * protocol of aURI, we show a permission prompt first.
   * If the caller has permission and a preferred handler is set, we skip the
   * dialogs and directly open the handler.
   * @param {nsIHandlerInfo} aHandler - Info about protocol and handlers.
   * @param {nsIURI} aURI - URI to be handled.
   * @param {nsIPrincipal} [aPrincipal] - Principal which triggered the load.
   * @param {BrowsingContext} [aBrowsingContext] - Context of the load.
   * @param {bool} [aTriggeredExternally] - Whether the load came from outside
   * this application.
   */
  async handleURI(
    aHandler,
    aURI,
    aPrincipal,
    aBrowsingContext,
    aTriggeredExternally = false
  ) {
    let callerHasPermission = this._hasProtocolHandlerPermission(
      aHandler.type,
      aPrincipal
    );

    // Force showing the dialog for links passed from outside the application.
    // This avoids infinite loops, see bug 1678255, bug 1667468, etc.
    if (
      aTriggeredExternally &&
      gPrefs.promptForExternal &&
      // ... unless we intend to open the link with a website or extension:
      !(
        aHandler.preferredAction == Ci.nsIHandlerInfo.useHelperApp &&
        aHandler.preferredApplicationHandler instanceof Ci.nsIWebHandlerApp
      )
    ) {
      aHandler.alwaysAskBeforeHandling = true;
    }

    // Skip the dialog if a preferred application is set and the caller has
    // permission.
    if (
      callerHasPermission &&
      !aHandler.alwaysAskBeforeHandling &&
      (aHandler.preferredAction == Ci.nsIHandlerInfo.useHelperApp ||
        aHandler.preferredAction == Ci.nsIHandlerInfo.useSystemDefault)
    ) {
      try {
        aHandler.launchWithURI(aURI, aBrowsingContext);
      } catch (error) {
        // We are not supposed to ask, but when file not found the user most likely
        // uninstalled the application which handles the uri so we will continue
        // by application chooser dialog.
        if (error.result == Cr.NS_ERROR_FILE_NOT_FOUND) {
          aHandler.alwaysAskBeforeHandling = true;
        } else {
          throw error;
        }
      }
    }

    let shouldOpenHandler = false;

    try {
      shouldOpenHandler = await this._prompt(
        aHandler,
        aPrincipal,
        callerHasPermission,
        aBrowsingContext,
        aURI
      );
    } catch (error) {
      Cu.reportError(error.message);
    }

    if (!shouldOpenHandler) {
      return;
    }

    // Site was granted permission and user chose to open application.
    // Launch the external handler.
    aHandler.launchWithURI(aURI, aBrowsingContext);
  }

  /**
   * Get the name of the application set to handle the the protocol.
   * @param {nsIHandlerInfo} aHandler - Info about protocol and handlers.
   * @returns {string|null} - Human readable handler name or null if the user
   * is expected to set a handler.
   */
  _getHandlerName(aHandler) {
    if (aHandler.alwaysAskBeforeHandling) {
      return null;
    }
    if (
      aHandler.preferredAction == Ci.nsIHandlerInfo.useSystemDefault &&
      aHandler.hasDefaultHandler
    ) {
      return aHandler.defaultDescription;
    }
    return aHandler.preferredApplicationHandler?.name;
  }

  /**
   * Show permission or/and app chooser prompt.
   * @param {nsIHandlerInfo} aHandler - Info about protocol and handlers.
   * @param {nsIPrincipal} aPrincipal - Principal which triggered the load.
   * @param {boolean} aHasPermission - Whether the caller has permission to
   * open the protocol.
   * @param {BrowsingContext} [aBrowsingContext] - Context associated with the
   * protocol navigation.
   */
  async _prompt(aHandler, aPrincipal, aHasPermission, aBrowsingContext, aURI) {
    let shouldOpenHandler = false;
    let resetHandlerChoice = false;
    let updateHandlerData = false;

    const isStandardProtocol = E10SUtils.STANDARD_SAFE_PROTOCOLS.includes(
      aURI.scheme
    );
    const {
      hasDefaultHandler,
      preferredApplicationHandler,
      alwaysAskBeforeHandling,
    } = aHandler;

    // This will skip the app chooser dialog flow unless the user explicitly opts to choose
    // another app in the permission dialog.
    if (
      !isStandardProtocol &&
      hasDefaultHandler &&
      preferredApplicationHandler == null &&
      alwaysAskBeforeHandling
    ) {
      aHandler.alwaysAskBeforeHandling = false;
      updateHandlerData = true;
    }

    // If caller does not have permission, prompt the user.
    if (!aHasPermission) {
      let canPersistPermission = this._isSupportedPrincipal(aPrincipal);

      let outArgs = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
        Ci.nsIWritablePropertyBag
      );
      // Whether the permission request was granted
      outArgs.setProperty("granted", false);
      // If the user wants to select a new application for the protocol.
      // This will cause us to show the chooser dialog, even if an app is set.
      outArgs.setProperty("resetHandlerChoice", null);
      // If the we should store the permission and not prompt again for it.
      outArgs.setProperty("remember", null);

      await this._openDialog(
        DIALOG_URL_PERMISSION,
        {
          handler: aHandler,
          principal: aPrincipal,
          browsingContext: aBrowsingContext,
          outArgs,
          canPersistPermission,
          preferredHandlerName: this._getHandlerName(aHandler),
        },
        aBrowsingContext
      );
      if (!outArgs.getProperty("granted")) {
        // User denied request
        return false;
      }

      // Check if user wants to set a new application to handle the protocol.
      resetHandlerChoice = outArgs.getProperty("resetHandlerChoice");

      // If the user wants to select a new app we don't persist the permission.
      if (!resetHandlerChoice && aPrincipal) {
        let remember = outArgs.getProperty("remember");
        this._updatePermission(aPrincipal, aHandler.type, remember);
      }

      shouldOpenHandler = true;
    }

    // Prompt if the user needs to make a handler choice for the protocol.
    if (aHandler.alwaysAskBeforeHandling || resetHandlerChoice) {
      // User has not set a preferred application to handle this protocol scheme.
      // Open the application chooser dialog
      let outArgs = Cc["@mozilla.org/hash-property-bag;1"].createInstance(
        Ci.nsIWritablePropertyBag
      );
      outArgs.setProperty("openHandler", false);
      outArgs.setProperty("preferredAction", aHandler.preferredAction);
      outArgs.setProperty(
        "preferredApplicationHandler",
        aHandler.preferredApplicationHandler
      );
      outArgs.setProperty(
        "alwaysAskBeforeHandling",
        aHandler.alwaysAskBeforeHandling
      );
      let usePrivateBrowsing = aBrowsingContext?.usePrivateBrowsing;
      await this._openDialog(
        DIALOG_URL_APP_CHOOSER,
        {
          handler: aHandler,
          outArgs,
          usePrivateBrowsing,
          enableButtonDelay: aHasPermission,
        },
        aBrowsingContext
      );

      shouldOpenHandler = outArgs.getProperty("openHandler");

      // If the user accepted the dialog, apply their selection.
      if (shouldOpenHandler) {
        for (let prop of [
          "preferredAction",
          "preferredApplicationHandler",
          "alwaysAskBeforeHandling",
        ]) {
          aHandler[prop] = outArgs.getProperty(prop);
        }
        updateHandlerData = true;
      }
    }

    if (updateHandlerData) {
      // Store handler data
      Cc["@mozilla.org/uriloader/handler-service;1"]
        .getService(Ci.nsIHandlerService)
        .store(aHandler);
    }

    return shouldOpenHandler;
  }

  /**
   * Test if a given principal has the open-protocol-handler permission for a
   * specific protocol.
   * @param {string} scheme - Scheme of the protocol.
   * @param {nsIPrincipal} aPrincipal - Principal to test for permission.
   * @returns {boolean} - true if permission is set, false otherwise.
   */
  _hasProtocolHandlerPermission(scheme, aPrincipal) {
    // Permission disabled by pref
    if (!nsContentDispatchChooser.isPermissionEnabled) {
      return true;
    }

    // If a handler is set to open externally by default we skip the dialog.
    if (
      Services.prefs.getBoolPref(
        "network.protocol-handler.external." + scheme,
        false
      )
    ) {
      return true;
    }

    if (!aPrincipal) {
      return false;
    }

    if (aPrincipal.isAddonOrExpandedAddonPrincipal) {
      return true;
    }

    let key = this._getSkipProtoDialogPermissionKey(scheme);
    return (
      Services.perms.testPermissionFromPrincipal(aPrincipal, key) ===
      Services.perms.ALLOW_ACTION
    );
  }

  /**
   * Get open-protocol-handler permission key for a protocol.
   * @param {string} aProtocolScheme - Scheme of the protocol.
   * @returns {string} - Permission key.
   */
  _getSkipProtoDialogPermissionKey(aProtocolScheme) {
    return (
      PROTOCOL_HANDLER_OPEN_PERM_KEY +
      PERMISSION_KEY_DELIMITER +
      aProtocolScheme
    );
  }

  /**
   * Opens a dialog as a SubDialog on tab level.
   * If we don't have a BrowsingContext we will fallback to a standalone window.
   * @param {string} aDialogURL - URL of the dialog to open.
   * @param {Object} aDialogArgs - Arguments passed to the dialog.
   * @param {BrowsingContext} [aBrowsingContext] - BrowsingContext associated
   * with the tab the dialog is associated with.
   */
  async _openDialog(aDialogURL, aDialogArgs, aBrowsingContext) {
    // Make the app chooser dialog resizable
    let resizable = `resizable=${
      aDialogURL == DIALOG_URL_APP_CHOOSER ? "yes" : "no"
    }`;

    if (aBrowsingContext) {
      let window = aBrowsingContext.topChromeWindow;
      if (!window) {
        throw new Error(
          "Can't show external protocol dialog. BrowsingContext has no chrome window associated."
        );
      }

      let { topFrameElement } = aBrowsingContext;
      if (topFrameElement?.tagName != "browser") {
        throw new Error(
          "Can't show external protocol dialog. BrowsingContext has no browser associated."
        );
      }

      let tabDialogBox = window.gBrowser.getTabDialogBox(topFrameElement);
      return tabDialogBox.open(
        aDialogURL,
        {
          features: resizable,
          allowDuplicateDialogs: false,
          keepOpenSameOriginNav: true,
        },
        aDialogArgs
      ).closedPromise;
    }

    // If we don't have a BrowsingContext, we need to show a standalone window.
    let win = Services.ww.openWindow(
      null,
      aDialogURL,
      null,
      `chrome,dialog=yes,centerscreen,${resizable}`,
      aDialogArgs
    );

    // Wait until window is closed.
    return new Promise(resolve => {
      win.addEventListener("unload", function onUnload(event) {
        if (event.target.location != aDialogURL) {
          return;
        }
        win.removeEventListener("unload", onUnload);
        resolve();
      });
    });
  }

  /**
   * Update the open-protocol-handler permission for the site which triggered
   * the dialog. Sites with this permission may skip this dialog.
   * @param {nsIPrincipal} aPrincipal - subject to update the permission for.
   * @param {string} aScheme - Scheme of protocol to allow.
   * @param {boolean} aAllow - Whether to set / unset the permission.
   */
  _updatePermission(aPrincipal, aScheme, aAllow) {
    // If enabled, store open-protocol-handler permission for content principals.
    if (
      !nsContentDispatchChooser.isPermissionEnabled ||
      aPrincipal.isSystemPrincipal ||
      !this._isSupportedPrincipal(aPrincipal)
    ) {
      return;
    }

    let permKey = this._getSkipProtoDialogPermissionKey(aScheme);
    if (aAllow) {
      Services.perms.addFromPrincipal(
        aPrincipal,
        permKey,
        Services.perms.ALLOW_ACTION,
        Services.perms.EXPIRE_NEVER
      );
    } else {
      Services.perms.removeFromPrincipal(aPrincipal, permKey);
    }
  }

  /**
   * Determine if we can use a principal to store permissions.
   * @param {nsIPrincipal} aPrincipal - Principal to test.
   * @returns {boolean} - true if we can store permissions, false otherwise.
   */
  _isSupportedPrincipal(aPrincipal) {
    return (
      aPrincipal &&
      ["http", "https", "moz-extension", "file"].some(scheme =>
        aPrincipal.schemeIs(scheme)
      )
    );
  }
}

nsContentDispatchChooser.prototype.classID = Components.ID(
  "e35d5067-95bc-4029-8432-e8f1e431148d"
);
nsContentDispatchChooser.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIContentDispatchChooser",
]);

XPCOMUtils.defineLazyPreferenceGetter(
  nsContentDispatchChooser,
  "isPermissionEnabled",
  "security.external_protocol_requires_permission",
  true
);
