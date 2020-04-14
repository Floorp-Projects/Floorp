/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "ExtensionActionHelper",
  "GeckoViewConnection",
  "GeckoViewWebExtension",
  "mobileWindowTracker",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { EventEmitter } = ChromeUtils.import(
  "resource://gre/modules/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const PRIVATE_BROWSING_PERMISSION = {
  permissions: ["internal:privateBrowsingAllowed"],
  origins: [],
};

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.jsm",
  GeckoViewTabBridge: "resource://gre/modules/GeckoViewTab.jsm",
  Management: "resource://gre/modules/Extension.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "mimeService",
  "@mozilla.org/mime;1",
  "nsIMIMEService"
);

const { debug, warn } = GeckoViewUtils.initLogging("Console"); // eslint-disable-line no-unused-vars

/** Provides common logic between page and browser actions */
class ExtensionActionHelper {
  constructor({
    tabTracker,
    windowTracker,
    tabContext,
    properties,
    extension,
  }) {
    this.tabTracker = tabTracker;
    this.windowTracker = windowTracker;
    this.tabContext = tabContext;
    this.properties = properties;
    this.extension = extension;
  }

  getTab(aTabId) {
    if (aTabId !== null) {
      return this.tabTracker.getTab(aTabId);
    }
    return null;
  }

  getWindow(aWindowId) {
    if (aWindowId !== null) {
      return this.windowTracker.getWindow(aWindowId);
    }
    return null;
  }

  extractProperties(aAction) {
    const merged = {};
    for (const p of this.properties) {
      merged[p] = aAction[p];
    }
    return merged;
  }

  eventDispatcherFor(aTabId) {
    if (!aTabId) {
      return EventDispatcher.instance;
    }

    const windowId = GeckoViewTabBridge.tabIdToWindowId(aTabId);
    const window = this.windowTracker.getWindow(windowId);
    return window.WindowEventDispatcher;
  }

  sendRequestForResult(aTabId, aData) {
    return this.eventDispatcherFor(aTabId).sendRequestForResult({
      ...aData,
      aTabId,
      extensionId: this.extension.id,
    });
  }

  sendRequest(aTabId, aData) {
    return this.eventDispatcherFor(aTabId).sendRequest({
      ...aData,
      aTabId,
      extensionId: this.extension.id,
    });
  }
}

class EmbedderPort {
  constructor(portId, messenger) {
    this.id = portId;
    this.messenger = messenger;
    EventDispatcher.instance.registerListener(this, [
      "GeckoView:WebExtension:PortMessageFromApp",
      "GeckoView:WebExtension:PortDisconnect",
    ]);
  }
  close() {
    EventDispatcher.instance.unregisterListener(this, [
      "GeckoView:WebExtension:PortMessageFromApp",
      "GeckoView:WebExtension:PortDisconnect",
    ]);
  }
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    if (this.id !== aData.portId) {
      return;
    }

    switch (aEvent) {
      case "GeckoView:WebExtension:PortMessageFromApp": {
        const holder = new StructuredCloneHolder(aData.message);
        this.messenger.sendPortMessage(this.id, holder);
        break;
      }

      case "GeckoView:WebExtension:PortDisconnect": {
        this.messenger.sendPortDisconnect(this.id);
        this.close();
        break;
      }
    }
  }
}

class GeckoViewConnection {
  constructor(sender, nativeApp) {
    this.sender = sender;
    this.nativeApp = nativeApp;
    const scope = GeckoViewWebExtension.extensionScopes.get(sender.extensionId);
    this.allowContentMessaging = scope.allowContentMessaging;
    if (!this.allowContentMessaging && !sender.verified) {
      throw new Error(`Unexpected messaging sender: ${JSON.stringify(sender)}`);
    }
  }

  get dispatcher() {
    const target = this.sender.actor.browsingContext.top.embedderElement;

    if (this.sender.envType === "addon_child") {
      // If this is a WebExtension Page we will have a GeckoSession associated
      // to it and thus a dispatcher.
      const dispatcher = GeckoViewUtils.getDispatcherForWindow(
        target.ownerGlobal
      );
      if (dispatcher) {
        return dispatcher;
      }

      // No dispatcher means this message is coming from a background script,
      // use the global event handler
      return EventDispatcher.instance;
    } else if (
      this.sender.envType === "content_child" &&
      this.allowContentMessaging
    ) {
      // If this message came from a content script, send the message to
      // the corresponding tab messenger so that GeckoSession can pick it
      // up.
      return GeckoViewUtils.getDispatcherForWindow(target.ownerGlobal);
    }

    throw new Error(`Uknown sender envType: ${this.sender.envType}`);
  }

  _sendMessage({ type, portId, data }) {
    const message = {
      type,
      sender: this.sender,
      data,
      portId,
      nativeApp: this.nativeApp,
    };

    return this.dispatcher.sendRequestForResult(message);
  }

  sendMessage(data) {
    return this._sendMessage({
      type: "GeckoView:WebExtension:Message",
      data: data.deserialize({}),
    });
  }

  onConnect(portId, messenger) {
    const port = new EmbedderPort(portId, messenger);

    port.onPortMessage = holder =>
      this._sendMessage({
        type: "GeckoView:WebExtension:PortMessage",
        portId: port.id,
        data: holder.deserialize({}),
      });

    port.onPortDisconnect = () => {
      EventDispatcher.instance.sendRequest({
        type: "GeckoView:WebExtension:Disconnect",
        sender: this.sender,
        portId: port.id,
      });
      port.close();
    };

    this._sendMessage({
      type: "GeckoView:WebExtension:Connect",
      data: {},
      portId: port.id,
    });

    return port;
  }
}

function exportExtension(aAddon, aPermissions, aSourceURI) {
  const {
    creator,
    description,
    homepageURL,
    signedState,
    name,
    icons,
    version,
    optionsURL,
    optionsType,
    isRecommended,
    blocklistState,
    userDisabled,
    embedderDisabled,
    isActive,
    isBuiltin,
    id,
  } = aAddon;
  let creatorName = null;
  let creatorURL = null;
  if (creator) {
    const { name, url } = creator;
    creatorName = name;
    creatorURL = url;
  }
  const openOptionsPageInTab = optionsType === AddonManager.OPTIONS_TYPE_TAB;
  const disabledFlags = [];
  if (userDisabled) {
    disabledFlags.push("userDisabled");
  }
  if (blocklistState !== Ci.nsIBlocklistService.STATE_NOT_BLOCKED) {
    disabledFlags.push("blocklistDisabled");
  }
  if (embedderDisabled) {
    disabledFlags.push("appDisabled");
  }
  let baseURL = "";
  let privateBrowsingAllowed = false;
  const policy = WebExtensionPolicy.getByID(id);
  if (policy) {
    baseURL = policy.getURL();
    privateBrowsingAllowed = policy.privateBrowsingAllowed;
  }
  const promptPermissions = aPermissions
    ? aPermissions.permissions.filter(Extension.shouldPromptFor)
    : [];
  return {
    webExtensionId: id,
    locationURI: aSourceURI != null ? aSourceURI.spec : "",
    isBuiltIn: isBuiltin,
    metaData: {
      origins: aPermissions ? aPermissions.origins : [],
      promptPermissions,
      description,
      enabled: isActive,
      disabledFlags,
      version,
      creatorName,
      creatorURL,
      homepageURL,
      name,
      optionsPageURL: optionsURL,
      openOptionsPageInTab,
      isRecommended,
      blocklistState,
      signedState,
      icons,
      baseURL,
      privateBrowsingAllowed,
    },
  };
}

class ExtensionInstallListener {
  constructor(aResolve, aInstall, aInstallId) {
    this.install = aInstall;
    this.installId = aInstallId;
    this.resolve = result => {
      aResolve(result);
      EventDispatcher.instance.unregisterListener(this, [
        "GeckoView:WebExtension:CancelInstall",
      ]);
    };
    EventDispatcher.instance.registerListener(this, [
      "GeckoView:WebExtension:CancelInstall",
    ]);
  }

  async onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:WebExtension:CancelInstall": {
        const { installId } = aData;
        if (this.installId !== installId) {
          return;
        }
        this.cancelling = true;
        let cancelled = false;
        try {
          this.install.cancel();
          cancelled = true;
        } catch (_) {
          // install may have already failed or been cancelled
        }
        aCallback.onSuccess({ cancelled });
        break;
      }
    }
  }

  onDownloadCancelled(aInstall) {
    // Do not resolve we were told to CancelInstall,
    // to prevent racing with that handler.
    if (!this.cancelling) {
      const { error: installError, state } = aInstall;
      this.resolve({ installError, state });
    }
  }

  onDownloadFailed(aInstall) {
    const { error: installError, state } = aInstall;
    this.resolve({ installError, state });
  }

  onDownloadEnded() {
    // Nothing to do
  }

  onInstallCancelled(aInstall) {
    // Do not resolve we were told to CancelInstall,
    // to prevent racing with that handler.
    if (!this.cancelling) {
      const { error: installError, state } = aInstall;
      this.resolve({ installError, state });
    }
  }

  onInstallFailed(aInstall) {
    const { error: installError, state } = aInstall;
    this.resolve({ installError, state });
  }

  onInstallPostponed(aInstall) {
    const { error: installError, state } = aInstall;
    this.resolve({ installError, state });
  }

  onInstallEnded(aInstall, aAddon) {
    const addonId = aAddon.id;
    const { sourceURI } = aInstall;
    const onReady = (name, { id }) => {
      if (id != addonId) {
        return;
      }
      Management.off("ready", onReady);
      const extension = exportExtension(
        aAddon,
        aAddon.userPermissions,
        sourceURI
      );
      this.resolve({ extension });
    };
    Management.on("ready", onReady);
  }
}

class ExtensionPromptObserver {
  constructor() {
    Services.obs.addObserver(this, "webextension-permission-prompt");
  }

  async permissionPrompt(aInstall, aAddon, aInfo) {
    const { sourceURI } = aInstall;
    const { permissions } = aInfo;
    const extension = exportExtension(aAddon, permissions, sourceURI);
    const response = await EventDispatcher.instance.sendRequestForResult({
      type: "GeckoView:WebExtension:InstallPrompt",
      extension,
    });

    if (response.allow) {
      aInfo.resolve();
    } else {
      aInfo.reject();
    }
  }

  observe(aSubject, aTopic, aData) {
    debug`observe ${aTopic}`;

    switch (aTopic) {
      case "webextension-permission-prompt": {
        const { info } = aSubject.wrappedJSObject;
        const { addon, install } = info;
        this.permissionPrompt(install, addon, info);
        break;
      }
    }
  }
}

new ExtensionPromptObserver();

class MobileWindowTracker extends EventEmitter {
  constructor() {
    super();
    this._topWindow = null;
    this._topNonPBWindow = null;
  }

  get topWindow() {
    if (this._topWindow) {
      return this._topWindow.get();
    }
    return null;
  }

  get topNonPBWindow() {
    if (this._topNonPBWindow) {
      return this._topNonPBWindow.get();
    }
    return null;
  }

  setTabActive(aWindow, aActive) {
    const { browser, tab, windowUtils } = aWindow;
    tab.active = aActive;

    if (aActive) {
      this._topWindow = Cu.getWeakReference(aWindow);
      const isPrivate = PrivateBrowsingUtils.isBrowserPrivate(browser);
      if (!isPrivate) {
        this._topNonPBWindow = this._topWindow;
      }
      this.emit("tab-activated", {
        windowId: windowUtils.outerWindowID,
        tabId: tab.id,
        isPrivate,
      });
    }
  }
}

var mobileWindowTracker = new MobileWindowTracker();

async function updatePromptHandler(aInfo) {
  const oldPerms = aInfo.existingAddon.userPermissions;
  if (!oldPerms) {
    // Updating from a legacy add-on, let it proceed
    return;
  }

  const newPerms = aInfo.addon.userPermissions;

  const difference = Extension.comparePermissions(oldPerms, newPerms);

  // We only care about permissions that we can prompt the user for
  const newPermissions = difference.permissions.filter(
    Extension.shouldPromptFor
  );
  const { origins: newOrigins } = difference;

  // If there are no new permissions, just proceed
  if (!newOrigins.length && !newPermissions.length) {
    return;
  }

  const currentlyInstalled = exportExtension(aInfo.existingAddon, oldPerms);
  const updatedExtension = exportExtension(aInfo.addon, newPerms);
  const response = await EventDispatcher.instance.sendRequestForResult({
    type: "GeckoView:WebExtension:UpdatePrompt",
    currentlyInstalled,
    updatedExtension,
    newPermissions,
    newOrigins,
  });

  if (!response.allow) {
    throw new Error("Extension update rejected.");
  }
}

var GeckoViewWebExtension = {
  observe(aSubject, aTopic, aData) {
    debug`observe ${aTopic}`;

    switch (aTopic) {
      case "testing-installed-addon":
      case "testing-uninstalled-addon": {
        // We pretend devtools installed/uninstalled this addon so we don't
        // have to add an API just for internal testing.
        // TODO: assert this is under a test
        EventDispatcher.instance.sendRequest({
          type: "GeckoView:WebExtension:DebuggerListUpdated",
        });
        break;
      }

      case "devtools-installed-addon": {
        EventDispatcher.instance.sendRequest({
          type: "GeckoView:WebExtension:DebuggerListUpdated",
        });
        break;
      }
    }
  },

  async registerWebExtension(aId, aUri, allowContentMessaging, aCallback) {
    const params = {
      id: aId,
      resourceURI: aUri,
      temporarilyInstalled: true,
      builtIn: true,
    };

    let file;
    if (aUri instanceof Ci.nsIFileURL) {
      file = aUri.file;
    }

    const scope = Extension.getBootstrapScope(aId, file);
    scope.allowContentMessaging = allowContentMessaging;
    // Always allow built-in extensions to run in private browsing
    ExtensionPermissions.add(aId, PRIVATE_BROWSING_PERMISSION);
    this.extensionScopes.set(aId, scope);

    await scope.startup(params, undefined);

    scope.extension.callOnClose({
      close: () => this.extensionScopes.delete(aId),
    });
  },

  async unregisterWebExtension(aId, aCallback) {
    try {
      const scope = this.extensionScopes.get(aId);
      await scope.shutdown();
      this.extensionScopes.delete(aId);
      aCallback.onSuccess();
    } catch (ex) {
      aCallback.onError(`Error unregistering WebExtension ${aId}. ${ex}`);
    }
  },

  async extensionById(aId) {
    const scope = this.extensionScopes.get(aId);
    if (!scope) {
      // Check if this is an installed extension we haven't seen yet
      const addon = await AddonManager.getAddonByID(aId);
      if (!addon) {
        debug`Could not find extension with id=${aId}`;
        return null;
      }
      return addon;
    }

    return scope.extension;
  },

  async installWebExtension(aInstallId, aUri) {
    const install = await AddonManager.getInstallForURL(aUri.spec, {
      telemetryInfo: {
        source: "geckoview-app",
      },
    });
    const promise = new Promise(resolve => {
      install.addListener(
        new ExtensionInstallListener(resolve, install, aInstallId)
      );
    });

    const systemPrincipal = Services.scriptSecurityManager.getSystemPrincipal();
    const mimeType = mimeService.getTypeFromURI(aUri);
    AddonManager.installAddonFromWebpage(
      mimeType,
      null,
      systemPrincipal,
      install
    );

    return promise;
  },

  async setPrivateBrowsingAllowed(aId, aAllowed) {
    if (aAllowed) {
      await ExtensionPermissions.add(aId, PRIVATE_BROWSING_PERMISSION);
    } else {
      await ExtensionPermissions.remove(aId, PRIVATE_BROWSING_PERMISSION);
    }

    // Reload the extension if it is already enabled.  This ensures any change
    // on the private browsing permission is properly handled.
    const addon = await this.extensionById(aId);
    if (addon.isActive) {
      await addon.reload();
    }

    return exportExtension(addon, addon.userPermissions, /* aSourceURI */ null);
  },

  async uninstallWebExtension(aId) {
    const extension = await this.extensionById(aId);
    if (!extension) {
      throw new Error(`Could not find an extension with id='${aId}'.`);
    }

    return extension.uninstall();
  },

  async browserActionClick(aId) {
    const policy = WebExtensionPolicy.getByID(aId);
    if (!policy) {
      return;
    }

    const browserAction = this.browserActions.get(policy.extension);
    if (!browserAction) {
      return;
    }

    browserAction.click();
  },

  async pageActionClick(aId) {
    const policy = WebExtensionPolicy.getByID(aId);
    if (!policy) {
      return;
    }

    const pageAction = this.pageActions.get(policy.extension);
    if (!pageAction) {
      return;
    }

    pageAction.click();
  },

  async actionDelegateAttached(aId) {
    const policy = WebExtensionPolicy.getByID(aId);
    if (!policy) {
      debug`Could not find extension with id=${aId}`;
      return;
    }

    const { extension } = policy;

    const browserAction = this.browserActions.get(extension);
    if (browserAction) {
      // Send information about this action to the delegate
      browserAction.updateOnChange(null);
    }

    const pageAction = this.pageActions.get(extension);
    if (pageAction) {
      pageAction.updateOnChange(null);
    }
  },

  async enableWebExtension(aId, aSource) {
    const extension = await this.extensionById(aId);
    if (aSource === "user") {
      await extension.enable();
    } else if (aSource === "app") {
      await extension.setEmbedderDisabled(false);
    }
    return exportExtension(
      extension,
      extension.userPermissions,
      /* aSourceURI */ null
    );
  },

  async disableWebExtension(aId, aSource) {
    const extension = await this.extensionById(aId);
    if (aSource === "user") {
      await extension.disable();
    } else if (aSource === "app") {
      await extension.setEmbedderDisabled(true);
    }
    return exportExtension(
      extension,
      extension.userPermissions,
      /* aSourceURI */ null
    );
  },

  /**
   * @return A promise resolved with either an AddonInstall object if an update
   * is available or null if no update is found.
   */
  checkForUpdate(aAddon) {
    return new Promise(resolve => {
      const listener = {
        onUpdateAvailable(aAddon, install) {
          install.promptHandler = updatePromptHandler;
          resolve(install);
        },
        onNoUpdateAvailable() {
          resolve(null);
        },
      };
      aAddon.findUpdates(listener, AddonManager.UPDATE_WHEN_USER_REQUESTED);
    });
  },

  async updateWebExtension(aId) {
    const extension = await this.extensionById(aId);

    const install = await this.checkForUpdate(extension);
    if (!install) {
      return null;
    }
    const promise = new Promise(resolve => {
      install.addListener(new ExtensionInstallListener(resolve));
    });
    install.install();
    return promise;
  },

  /* eslint-disable complexity */
  async onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:BrowserAction:Click": {
        this.browserActionClick(aData.extensionId);
        break;
      }
      case "GeckoView:PageAction:Click": {
        this.pageActionClick(aData.extensionId);
        break;
      }
      case "GeckoView:RegisterWebExtension": {
        const uri = Services.io.newURI(aData.locationUri);
        if (
          uri == null ||
          (!(uri instanceof Ci.nsIFileURL) && !(uri instanceof Ci.nsIJARURI))
        ) {
          aCallback.onError(
            `Extension does not point to a resource URI or a file URL. extension=${aData.locationUri}`
          );
          return;
        }

        if (uri.fileName != "" && uri.fileExtension != "xpi") {
          aCallback.onError(
            `Extension does not point to a folder or an .xpi file. Hint: the path needs to end with a "/" to be considered a folder. extension=${aData.locationUri}`
          );
          return;
        }

        if (this.extensionScopes.has(aData.id)) {
          aCallback.onError(
            `An extension with id='${aData.id}' has already been registered.`
          );
          return;
        }

        this.registerWebExtension(
          aData.id,
          uri,
          aData.allowContentMessaging
        ).then(aCallback.onSuccess, error =>
          aCallback.onError(
            `An error occurred while registering the WebExtension ${aData.locationUri}: ${error}.`
          )
        );
        break;
      }

      case "GeckoView:ActionDelegate:Attached": {
        this.actionDelegateAttached(aData.extensionId);
        break;
      }

      case "GeckoView:UnregisterWebExtension": {
        if (!this.extensionScopes.has(aData.id)) {
          aCallback.onError(
            `Could not find an extension with id='${aData.id}'.`
          );
          return;
        }

        this.unregisterWebExtension(aData.id, aCallback);
        break;
      }

      case "GeckoView:WebExtension:Get": {
        const extension = await this.extensionById(aData.extensionId);
        if (!extension) {
          aCallback.onError(
            `Could not find extension with id: ${aData.extensionId}`
          );
          return;
        }

        aCallback.onSuccess({
          extension: exportExtension(
            extension,
            extension.userPermissions,
            /* aSourceURI */ null
          ),
        });
        break;
      }

      case "GeckoView:WebExtension:SetPBAllowed": {
        const { extensionId, allowed } = aData;
        try {
          const extension = await this.setPrivateBrowsingAllowed(
            extensionId,
            allowed
          );
          aCallback.onSuccess({ extension });
        } catch (ex) {
          aCallback.onError(`Unexpected error: ${ex}`);
        }
        break;
      }

      case "GeckoView:WebExtension:Install": {
        const { locationUri, installId } = aData;
        const uri = Services.io.newURI(locationUri);
        if (uri == null) {
          aCallback.onError(`Could not parse uri: ${locationUri}`);
          return;
        }

        try {
          const result = await this.installWebExtension(installId, uri);
          if (result.extension) {
            aCallback.onSuccess(result);
          } else {
            aCallback.onError(result);
          }
        } catch (ex) {
          debug`Install exception error ${ex}`;
          aCallback.onError(`Unexpected error: ${ex}`);
        }

        break;
      }

      case "GeckoView:WebExtension:InstallBuiltIn": {
        // TODO
        aCallback.onError(`Not implemented`);
        break;
      }

      case "GeckoView:WebExtension:Uninstall": {
        try {
          await this.uninstallWebExtension(aData.webExtensionId);
          aCallback.onSuccess();
        } catch (ex) {
          debug`Failed uninstall ${ex}`;
          aCallback.onError(
            `This extension cannot be uninstalled. Error: ${ex}.`
          );
        }
        break;
      }

      case "GeckoView:WebExtension:Enable": {
        try {
          const { source, webExtensionId } = aData;
          if (source !== "user" && source !== "app") {
            throw new Error("Illegal source parameter");
          }
          const extension = await this.enableWebExtension(
            webExtensionId,
            source
          );
          aCallback.onSuccess({ extension });
        } catch (ex) {
          debug`Failed enable ${ex}`;
          aCallback.onError(`Unexpected error: ${ex}`);
        }
        break;
      }

      case "GeckoView:WebExtension:Disable": {
        try {
          const { source, webExtensionId } = aData;
          if (source !== "user" && source !== "app") {
            throw new Error("Illegal source parameter");
          }
          const extension = await this.disableWebExtension(
            webExtensionId,
            source
          );
          aCallback.onSuccess({ extension });
        } catch (ex) {
          debug`Failed disable ${ex}`;
          aCallback.onError(`Unexpected error: ${ex}`);
        }
        break;
      }

      case "GeckoView:WebExtension:List": {
        try {
          const addons = await AddonManager.getAddonsByTypes(["extension"]);
          const extensions = addons.map(addon =>
            exportExtension(addon, addon.userPermissions, null)
          );
          aCallback.onSuccess({ extensions });
        } catch (ex) {
          debug`Failed list ${ex}`;
          aCallback.onError(`Unexpected error: ${ex}`);
        }
        break;
      }

      case "GeckoView:WebExtension:Update": {
        try {
          const { webExtensionId } = aData;
          const result = await this.updateWebExtension(webExtensionId);
          if (result === null || result.extension) {
            aCallback.onSuccess(result);
          } else {
            aCallback.onError(result);
          }
        } catch (ex) {
          debug`Failed update ${ex}`;
          aCallback.onError(`Unexpected error: ${ex}`);
        }
        break;
      }
    }
  },
};

GeckoViewWebExtension.extensionScopes = new Map();
// WeakMap[Extension -> BrowserAction]
GeckoViewWebExtension.browserActions = new WeakMap();
// WeakMap[Extension -> PageAction]
GeckoViewWebExtension.pageActions = new WeakMap();
Services.obs.addObserver(GeckoViewWebExtension, "devtools-installed-addon");
Services.obs.addObserver(GeckoViewWebExtension, "testing-installed-addon");
Services.obs.addObserver(GeckoViewWebExtension, "testing-uninstalled-addon");
