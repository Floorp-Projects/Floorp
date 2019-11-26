/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [
  "ExtensionActionHelper",
  "GeckoViewConnection",
  "GeckoViewWebExtension",
];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  Extension: "resource://gre/modules/Extension.jsm",
  ExtensionChild: "resource://gre/modules/ExtensionChild.jsm",
  GeckoViewTabBridge: "resource://gre/modules/GeckoViewTab.jsm",
});

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

class EmbedderPort extends ExtensionChild.Port {
  constructor(...args) {
    super(...args);
    EventDispatcher.instance.registerListener(this, [
      "GeckoView:WebExtension:PortMessageFromApp",
      "GeckoView:WebExtension:PortDisconnect",
    ]);
  }
  handleDisconnection() {
    super.handleDisconnection();
    EventDispatcher.instance.unregisterListener(this, [
      "GeckoView:WebExtension:PortMessageFromApp",
      "GeckoView:WebExtension:PortDisconnect",
    ]);
  }
  close() {
    // Notify listeners that this port is being closed because the context is
    // gone.
    this.disconnectByOtherEnd();
  }
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    if (this.id !== aData.portId) {
      return;
    }

    switch (aEvent) {
      case "GeckoView:WebExtension:PortMessageFromApp": {
        this.postMessage(aData.message);
        break;
      }

      case "GeckoView:WebExtension:PortDisconnect": {
        this.disconnect();
        break;
      }
    }
  }
}

class GeckoViewConnection {
  constructor(context, sender, target, nativeApp) {
    this.context = context;
    this.sender = sender;
    this.target = target;
    this.nativeApp = nativeApp;
    this.allowContentMessaging = GeckoViewWebExtension.extensionScopes.get(
      sender.extensionId
    ).allowContentMessaging;
  }

  _getMessageManager(aTarget) {
    if (aTarget.frameLoader) {
      return aTarget.frameLoader.messageManager;
    }
    return aTarget;
  }

  get dispatcher() {
    if (this.sender.envType === "addon_child") {
      // If this is a WebExtension Page we will have a GeckoSession associated
      // to it and thus a dispatcher.
      const dispatcher = GeckoViewUtils.getDispatcherForWindow(
        this.target.ownerGlobal
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
      return GeckoViewUtils.getDispatcherForWindow(this.target.ownerGlobal);
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

  onConnect(portId) {
    const port = new EmbedderPort(
      this.context,
      this.target.messageManager,
      [Services.mm],
      "",
      portId,
      this.sender,
      this.sender
    );
    port.registerOnMessage(holder =>
      this._sendMessage({
        type: "GeckoView:WebExtension:PortMessage",
        portId: port.id,
        data: holder.deserialize({}),
      })
    );

    port.registerOnDisconnect(msg =>
      EventDispatcher.instance.sendRequest({
        type: "GeckoView:WebExtension:Disconnect",
        sender: this.sender,
        portId: port.id,
      })
    );

    return this._sendMessage({
      type: "GeckoView:WebExtension:Connect",
      data: {},
      portId: port.id,
    });
  }
}

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

  extensionById(aId) {
    const scope = this.extensionScopes.get(aId);
    if (!scope) {
      return null;
    }

    return scope.extension;
  },

  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:BrowserAction:Click": {
        const extension = this.extensionById(aData.extensionId);
        if (!extension) {
          return;
        }

        const browserAction = this.browserActions.get(extension);
        if (!browserAction) {
          return;
        }

        browserAction.click();
        break;
      }
      case "GeckoView:PageAction:Click": {
        const extension = this.extensionById(aData.extensionId);
        if (!extension) {
          return;
        }

        const pageAction = this.pageActions.get(extension);
        if (!pageAction) {
          return;
        }

        pageAction.click();
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

      case "GeckoView:WebExtension:Install": {
        // TODO
        aCallback.onError(`Not implemented`);
        break;
      }

      case "GeckoView:WebExtension:InstallBuiltIn": {
        // TODO
        aCallback.onError(`Not implemented`);
        break;
      }

      case "GeckoView:WebExtension:Uninstall": {
        // TODO
        aCallback.onError(`Not implemented`);
        break;
      }

      case "GeckoView:WebExtension:Enable": {
        // TODO
        aCallback.onError(`Not implemented`);
        break;
      }

      case "GeckoView:WebExtension:Disable": {
        // TODO
        aCallback.onError(`Not implemented`);
        break;
      }

      case "GeckoView:WebExtension:List": {
        // TODO
        aCallback.onError(`Not implemented`);
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
