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

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  GeckoViewTabBridge: "resource://gre/modules/GeckoViewTab.jsm",
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
    optionsBrowserStyle,
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
  const openOptionsPageInTab =
    optionsBrowserStyle === AddonManager.OPTIONS_TYPE_TAB;
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
  return {
    webExtensionId: id,
    locationURI: aSourceURI != null ? aSourceURI.spec : "",
    isBuiltIn: isBuiltin,
    metaData: {
      permissions: aPermissions ? aPermissions.permissions : [],
      origins: aPermissions ? aPermissions.origins : [],
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
    },
  };
}

class ExtensionInstallListener {
  constructor(aResolve) {
    this.resolve = aResolve;
  }

  onDownloadCancelled(aInstall) {
    const { error: installError } = aInstall;
    this.resolve({ installError });
  }

  onDownloadFailed(aInstall) {
    const { error: installError } = aInstall;
    this.resolve({ installError });
  }

  onDownloadEnded() {
    // Nothing to do
  }

  onInstallCancelled(aInstall) {
    const { error: installError } = aInstall;
    this.resolve({ installError });
  }

  onInstallFailed(aInstall) {
    const { error: installError } = aInstall;
    this.resolve({ installError });
  }

  onInstallEnded(aInstall, aAddon) {
    const extension = exportExtension(
      aAddon,
      aAddon.userPermissions,
      aInstall.sourceURI
    );
    this.resolve({ extension });
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
  }

  get topWindow() {
    if (this._topWindow) {
      return this._topWindow.get();
    }
    return null;
  }

  setTabActive(aWindow, aActive) {
    const tab = aWindow.BrowserApp.selectedTab;
    tab.active = aActive;

    if (aActive) {
      this._topWindow = Cu.getWeakReference(aWindow);
      this.emit("tab-activated", {
        windowId: aWindow.windowUtils.outerWindowID,
        tabId: tab.id,
      });
    }
  }
}

var mobileWindowTracker = new MobileWindowTracker();

var GeckoViewWebExtension = {
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
    let scope = this.extensionScopes.get(aId);
    if (!scope) {
      // Check if this is an installed extension we haven't seen yet
      const addon = await AddonManager.getAddonByID(aId);
      if (!addon) {
        debug`Could not find extension with id=${aId}`;
        return null;
      }
      scope = {
        allowContentMessaging: false,
        extension: addon,
      };
    }

    return scope.extension;
  },

  async installWebExtension(aUri) {
    const install = await AddonManager.getInstallForURL(aUri.spec);
    const promise = new Promise(resolve => {
      install.addListener(new ExtensionInstallListener(resolve));
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
            `Extension does not point to a resource URI or a file URL. extension=${
              aData.locationUri
            }`
          );
          return;
        }

        if (uri.fileName != "" && uri.fileExtension != "xpi") {
          aCallback.onError(
            `Extension does not point to a folder or an .xpi file. Hint: the path needs to end with a "/" to be considered a folder. extension=${
              aData.locationUri
            }`
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
            `An error occurred while registering the WebExtension ${
              aData.locationUri
            }: ${error}.`
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

      case "GeckoView:WebExtension:Install": {
        const uri = Services.io.newURI(aData.locationUri);
        if (uri == null) {
          aCallback.onError(`Could not parse uri: ${uri}`);
          return;
        }

        try {
          const result = await this.installWebExtension(uri);
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
        // TODO
        aCallback.onError(`Not implemented`);
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
