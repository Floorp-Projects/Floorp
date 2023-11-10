/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";
import { EventEmitter } from "resource://gre/modules/EventEmitter.sys.mjs";

const PRIVATE_BROWSING_PERMISSION = {
  permissions: ["internal:privateBrowsingAllowed"],
  origins: [],
};

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  AddonRepository: "resource://gre/modules/addons/AddonRepository.sys.mjs",
  AddonSettings: "resource://gre/modules/addons/AddonSettings.sys.mjs",
  EventDispatcher: "resource://gre/modules/Messaging.sys.mjs",
  Extension: "resource://gre/modules/Extension.sys.mjs",
  ExtensionData: "resource://gre/modules/Extension.sys.mjs",
  ExtensionPermissions: "resource://gre/modules/ExtensionPermissions.sys.mjs",
  ExtensionProcessCrashObserver: "resource://gre/modules/Extension.sys.mjs",
  GeckoViewTabBridge: "resource://gre/modules/GeckoViewTab.sys.mjs",
  Management: "resource://gre/modules/Extension.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "mimeService",
  "@mozilla.org/mime;1",
  "nsIMIMEService"
);

const { debug, warn } = GeckoViewUtils.initLogging("Console");

export var DownloadTracker = new (class extends EventEmitter {
  constructor() {
    super();

    // maps numeric IDs to DownloadItem objects
    this._downloads = new Map();
  }

  onEvent(event, data, callback) {
    switch (event) {
      case "GeckoView:WebExtension:DownloadChanged": {
        const downloadItem = this.getDownloadItemById(data.downloadItemId);

        if (!downloadItem) {
          callback.onError("Error: Trying to update unknown download");
          return;
        }

        const delta = downloadItem.update(data);
        if (delta) {
          this.emit("download-changed", {
            delta,
            downloadItem,
          });
        }
      }
    }
  }

  addDownloadItem(item) {
    this._downloads.set(item.id, item);
  }

  /**
   * Finds and returns a DownloadItem with a certain numeric ID
   *
   * @param {number} id
   * @returns {DownloadItem} download item
   */
  getDownloadItemById(id) {
    return this._downloads.get(id);
  }
})();

/** Provides common logic between page and browser actions */
export class ExtensionActionHelper {
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
      return lazy.EventDispatcher.instance;
    }

    const windowId = lazy.GeckoViewTabBridge.tabIdToWindowId(aTabId);
    const window = this.windowTracker.getWindow(windowId);
    return window.WindowEventDispatcher;
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
    this.dispatcher = lazy.EventDispatcher.byName(`port:${portId}`);
    this.dispatcher.registerListener(this, [
      "GeckoView:WebExtension:PortMessageFromApp",
      "GeckoView:WebExtension:PortDisconnect",
    ]);
  }
  close() {
    this.dispatcher.unregisterListener(this, [
      "GeckoView:WebExtension:PortMessageFromApp",
      "GeckoView:WebExtension:PortDisconnect",
    ]);
  }
  onPortDisconnect() {
    this.dispatcher.sendRequest({
      type: "GeckoView:WebExtension:Disconnect",
      sender: this.sender,
    });
    this.close();
  }
  onPortMessage(holder) {
    this.dispatcher.sendRequest({
      type: "GeckoView:WebExtension:PortMessage",
      data: holder.deserialize({}),
    });
  }
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:WebExtension:PortMessageFromApp": {
        const holder = new StructuredCloneHolder(
          "GeckoView:WebExtension:PortMessageFromApp",
          null,
          aData.message
        );
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

export class GeckoViewConnection {
  constructor(sender, target, nativeApp, allowContentMessaging) {
    this.sender = sender;
    this.target = target;
    this.nativeApp = nativeApp;
    this.allowContentMessaging = allowContentMessaging;

    if (!allowContentMessaging && sender.envType !== "addon_child") {
      throw new Error(`Unexpected messaging sender: ${JSON.stringify(sender)}`);
    }
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
      return lazy.EventDispatcher.instance;
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
      extensionId: this.sender.id,
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

    this._sendMessage({
      type: "GeckoView:WebExtension:Connect",
      data: {},
      portId: port.id,
    });

    return port;
  }
}

async function filterPromptPermissions(aPermissions) {
  if (!aPermissions) {
    return [];
  }
  const promptPermissions = [];
  for (const permission of aPermissions) {
    if (!(await lazy.Extension.shouldPromptFor(permission))) {
      continue;
    }
    promptPermissions.push(permission);
  }
  return promptPermissions;
}

// Keep in sync with WebExtension.java
const FLAG_NONE = 0;
const FLAG_ALLOW_CONTENT_MESSAGING = 1 << 0;

function exportFlags(aPolicy) {
  let flags = FLAG_NONE;
  if (!aPolicy) {
    return flags;
  }
  const { extension } = aPolicy;
  if (extension.hasPermission("nativeMessagingFromContent")) {
    flags |= FLAG_ALLOW_CONTENT_MESSAGING;
  }
  return flags;
}

async function exportExtension(aAddon, aPermissions, aSourceURI) {
  // First, let's make sure the policy is ready if present
  let policy = WebExtensionPolicy.getByID(aAddon.id);
  if (policy?.readyPromise) {
    policy = await policy.readyPromise;
  }
  const {
    amoListingURL,
    averageRating,
    blocklistState,
    creator,
    description,
    embedderDisabled,
    fullDescription,
    homepageURL,
    icons,
    id,
    isActive,
    isBuiltin,
    isCorrectlySigned,
    isRecommended,
    name,
    optionsType,
    optionsURL,
    reviewCount,
    reviewURL,
    signedState,
    sourceURI,
    temporarilyInstalled,
    userDisabled,
    version,
  } = aAddon;
  let creatorName = null;
  let creatorURL = null;
  if (creator) {
    const { name, url } = creator;
    creatorName = name;
    creatorURL = url;
  }
  const openOptionsPageInTab =
    optionsType === lazy.AddonManager.OPTIONS_TYPE_TAB;
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
  // Add-ons without an `isCorrectlySigned` property are correctly signed as
  // they aren't the correct type for signing.
  if (lazy.AddonSettings.REQUIRE_SIGNING && isCorrectlySigned === false) {
    disabledFlags.push("signatureDisabled");
  }
  if (lazy.AddonManager.checkCompatibility && !aAddon.isCompatible) {
    disabledFlags.push("appVersionDisabled");
  }
  const baseURL = policy ? policy.getURL() : "";
  const privateBrowsingAllowed = policy ? policy.privateBrowsingAllowed : false;
  const promptPermissions = aPermissions
    ? await filterPromptPermissions(aPermissions.permissions)
    : [];

  let updateDate;
  try {
    updateDate = aAddon.updateDate?.toISOString();
  } catch {
    // `installDate` is used as a fallback for `updateDate` but only when the
    // add-on is installed. Before that, `installDate` might be undefined,
    // which would cause `updateDate` (and `installDate`) to be an "invalid
    // date".
    updateDate = null;
  }

  return {
    webExtensionId: id,
    locationURI: aSourceURI != null ? aSourceURI.spec : "",
    isBuiltIn: isBuiltin,
    webExtensionFlags: exportFlags(policy),
    metaData: {
      amoListingURL,
      averageRating,
      baseURL,
      blocklistState,
      creatorName,
      creatorURL,
      description,
      disabledFlags,
      downloadUrl: sourceURI?.displaySpec,
      enabled: isActive,
      fullDescription,
      homepageURL,
      icons,
      isRecommended,
      name,
      openOptionsPageInTab,
      optionsPageURL: optionsURL,
      origins: aPermissions ? aPermissions.origins : [],
      privateBrowsingAllowed,
      promptPermissions,
      reviewCount,
      reviewURL,
      signedState,
      temporary: temporarilyInstalled,
      updateDate,
      version,
    },
  };
}

class ExtensionInstallListener {
  constructor(aResolve, aInstall, aInstallId) {
    this.install = aInstall;
    this.installId = aInstallId;
    this.resolve = result => {
      aResolve(result);
      lazy.EventDispatcher.instance.unregisterListener(this, [
        "GeckoView:WebExtension:CancelInstall",
      ]);
    };
    lazy.EventDispatcher.instance.registerListener(this, [
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
    debug`onDownloadCancelled state=${aInstall.state}`;
    // Do not resolve we were told to CancelInstall,
    // to prevent racing with that handler.
    if (!this.cancelling) {
      const { error: installError, state } = aInstall;
      this.resolve({ installError, state });
    }
  }

  onDownloadFailed(aInstall) {
    debug`onDownloadFailed state=${aInstall.state}`;
    const { error: installError, state } = aInstall;
    this.resolve({ installError, state });
  }

  onDownloadEnded() {
    // Nothing to do
  }

  onInstallCancelled(aInstall, aCancelledByUser) {
    debug`onInstallCancelled state=${aInstall.state} cancelledByUser=${aCancelledByUser}`;
    // Do not resolve we were told to CancelInstall,
    // to prevent racing with that handler.
    if (!this.cancelling) {
      const { error: installError, state } = aInstall;
      // An install can be cancelled by the user OR something else, e.g. when
      // the blocklist prevents the install of a blocked add-on.
      this.resolve({ installError, state, cancelledByUser: aCancelledByUser });
    }
  }

  onInstallFailed(aInstall) {
    debug`onInstallFailed state=${aInstall.state}`;
    const { error: installError, state } = aInstall;
    this.resolve({ installError, state });
  }

  onInstallPostponed(aInstall) {
    debug`onInstallPostponed state=${aInstall.state}`;
    const { error: installError, state } = aInstall;
    this.resolve({ installError, state });
  }

  async onInstallEnded(aInstall, aAddon) {
    debug`onInstallEnded addonId=${aAddon.id}`;
    const extension = await exportExtension(
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
    Services.obs.addObserver(this, "webextension-optional-permission-prompt");
  }

  async permissionPrompt(aInstall, aAddon, aInfo) {
    const { sourceURI } = aInstall;
    const { permissions } = aInfo;
    const extension = await exportExtension(aAddon, permissions, sourceURI);
    const response = await lazy.EventDispatcher.instance.sendRequestForResult({
      type: "GeckoView:WebExtension:InstallPrompt",
      extension,
    });

    if (response.allow) {
      aInfo.resolve();
    } else {
      aInfo.reject();
    }
  }

  async optionalPermissionPrompt(aExtensionId, aPermissions, resolve) {
    const response = await lazy.EventDispatcher.instance.sendRequestForResult({
      type: "GeckoView:WebExtension:OptionalPrompt",
      extensionId: aExtensionId,
      permissions: aPermissions,
    });
    resolve(response.allow);
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
      case "webextension-optional-permission-prompt": {
        const { id, permissions, resolve } = aSubject.wrappedJSObject;
        this.optionalPermissionPrompt(id, permissions, resolve);
        break;
      }
    }
  }
}

class AddonInstallObserver {
  constructor() {
    Services.obs.addObserver(this, "addon-install-failed");
  }

  async onInstallationFailed(aAddon, aAddonName, aError) {
    // aAddon could be null if we have a network error where we can't download the xpi file.
    // aAddon could also be a valid object without an ID when the xpi file is corrupt.
    let extension = null;
    if (aAddon?.id) {
      extension = await exportExtension(
        aAddon,
        aAddon.userPermissions,
        /* aSourceURI */ null
      );
    }

    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnInstallationFailed",
      extension,
      addonName: aAddonName,
      error: aError,
    });
  }

  observe(aSubject, aTopic, aData) {
    debug`observe ${aTopic}`;
    switch (aTopic) {
      case "addon-install-failed": {
        aSubject.wrappedJSObject.installs.forEach(install => {
          const { addon, error, name: addonName } = install;
          this.onInstallationFailed(addon, addonName, error);
        });
        break;
      }
    }
  }
}

new ExtensionPromptObserver();
new AddonInstallObserver();

class AddonManagerListener {
  constructor() {
    lazy.AddonManager.addAddonListener(this);
    // Some extension properties are not going to be available right away after the extension
    // have been installed (e.g. in particular metaData.optionsPageURL), the GeckoView event
    // dispatched from onExtensionReady listener will be providing updated extension metadata to
    // the GeckoView side when it is actually going to be available.
    this.onExtensionReady = this.onExtensionReady.bind(this);
    lazy.Management.on("ready", this.onExtensionReady);
  }

  async onExtensionReady(name, extInstance) {
    // In xpcshell tests there wil be test extensions that trigger this event while the
    // AddonManager has not been started at all, on the contrary on a regular browser
    // instance the AddonManager is expected to be already fully started for an extension
    // for the extension to be able to reach the "ready" state, and so we just silently
    // early exit here if the AddonManager is not ready.
    if (!lazy.AddonManager.isReady) {
      return;
    }

    debug`onExtensionReady ${extInstance.id}`;

    const addonWrapper = await lazy.AddonManager.getAddonByID(extInstance.id);
    if (!addonWrapper) {
      return;
    }

    const extension = await exportExtension(
      addonWrapper,
      addonWrapper.userPermissions,
      /* aSourceURI */ null
    );
    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnReady",
      extension,
    });
  }

  async onDisabling(aAddon) {
    debug`onDisabling ${aAddon.id}`;

    const extension = await exportExtension(
      aAddon,
      aAddon.userPermissions,
      /* aSourceURI */ null
    );
    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnDisabling",
      extension,
    });
  }

  async onDisabled(aAddon) {
    debug`onDisabled ${aAddon.id}`;

    const extension = await exportExtension(
      aAddon,
      aAddon.userPermissions,
      /* aSourceURI */ null
    );
    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnDisabled",
      extension,
    });
  }

  async onEnabling(aAddon) {
    debug`onEnabling ${aAddon.id}`;

    const extension = await exportExtension(
      aAddon,
      aAddon.userPermissions,
      /* aSourceURI */ null
    );
    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnEnabling",
      extension,
    });
  }

  async onEnabled(aAddon) {
    debug`onEnabled ${aAddon.id}`;

    const extension = await exportExtension(
      aAddon,
      aAddon.userPermissions,
      /* aSourceURI */ null
    );
    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnEnabled",
      extension,
    });
  }

  async onUninstalling(aAddon) {
    debug`onUninstalling ${aAddon.id}`;

    const extension = await exportExtension(
      aAddon,
      aAddon.userPermissions,
      /* aSourceURI */ null
    );
    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnUninstalling",
      extension,
    });
  }

  async onUninstalled(aAddon) {
    debug`onUninstalled ${aAddon.id}`;

    const extension = await exportExtension(
      aAddon,
      aAddon.userPermissions,
      /* aSourceURI */ null
    );
    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnUninstalled",
      extension,
    });
  }

  async onInstalling(aAddon) {
    debug`onInstalling ${aAddon.id}`;

    const extension = await exportExtension(
      aAddon,
      aAddon.userPermissions,
      /* aSourceURI */ null
    );
    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnInstalling",
      extension,
    });
  }

  async onInstalled(aAddon) {
    debug`onInstalled ${aAddon.id}`;

    const extension = await exportExtension(
      aAddon,
      aAddon.userPermissions,
      /* aSourceURI */ null
    );
    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnInstalled",
      extension,
    });
  }
}

new AddonManagerListener();

class ExtensionProcessListener {
  constructor() {
    this.onExtensionProcessCrash = this.onExtensionProcessCrash.bind(this);
    lazy.Management.on("extension-process-crash", this.onExtensionProcessCrash);

    lazy.EventDispatcher.instance.registerListener(this, [
      "GeckoView:WebExtension:EnableProcessSpawning",
      "GeckoView:WebExtension:DisableProcessSpawning",
    ]);
  }

  async onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:WebExtension:EnableProcessSpawning": {
        debug`Extension process crash -> re-enable process spawning`;
        lazy.ExtensionProcessCrashObserver.enableProcessSpawning();
        break;
      }
    }
  }

  async onExtensionProcessCrash(name, { childID, processSpawningDisabled }) {
    debug`Extension process crash -> childID=${childID} processSpawningDisabled=${processSpawningDisabled}`;

    // When an extension process has crashed too many times, Gecko will set
    // `processSpawningDisabled` and no longer allow the extension process
    // spawning. We only want to send a request to the embedder when we are
    // disabling the process spawning.  If process spawning is still enabled
    // then we short circuit and don't notify the embedder.
    if (!processSpawningDisabled) {
      return;
    }

    lazy.EventDispatcher.instance.sendRequest({
      type: "GeckoView:WebExtension:OnDisabledProcessSpawning",
    });
  }
}

new ExtensionProcessListener();

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
    const { browser, tab: nativeTab, docShell } = aWindow;
    nativeTab.active = aActive;

    if (aActive) {
      this._topWindow = Cu.getWeakReference(aWindow);
      const isPrivate = lazy.PrivateBrowsingUtils.isBrowserPrivate(browser);
      if (!isPrivate) {
        this._topNonPBWindow = this._topWindow;
      }
      this.emit("tab-activated", {
        windowId: docShell.outerWindowID,
        tabId: nativeTab.id,
        isPrivate,
        nativeTab,
      });
    }
  }
}

export var mobileWindowTracker = new MobileWindowTracker();

async function updatePromptHandler(aInfo) {
  const oldPerms = aInfo.existingAddon.userPermissions;
  if (!oldPerms) {
    // Updating from a legacy add-on, let it proceed
    return;
  }

  const newPerms = aInfo.addon.userPermissions;

  const difference = lazy.Extension.comparePermissions(oldPerms, newPerms);

  // We only care about permissions that we can prompt the user for
  const newPermissions = await filterPromptPermissions(difference.permissions);
  const { origins: newOrigins } = difference;

  // If there are no new permissions, just proceed
  if (!newOrigins.length && !newPermissions.length) {
    return;
  }

  const currentlyInstalled = await exportExtension(
    aInfo.existingAddon,
    oldPerms
  );
  const updatedExtension = await exportExtension(aInfo.addon, newPerms);
  const response = await lazy.EventDispatcher.instance.sendRequestForResult({
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

export var GeckoViewWebExtension = {
  observe(aSubject, aTopic, aData) {
    debug`observe ${aTopic}`;

    switch (aTopic) {
      case "testing-installed-addon":
      case "testing-uninstalled-addon": {
        // We pretend devtools installed/uninstalled this addon so we don't
        // have to add an API just for internal testing.
        // TODO: assert this is under a test
        lazy.EventDispatcher.instance.sendRequest({
          type: "GeckoView:WebExtension:DebuggerListUpdated",
        });
        break;
      }

      case "devtools-installed-addon": {
        lazy.EventDispatcher.instance.sendRequest({
          type: "GeckoView:WebExtension:DebuggerListUpdated",
        });
        break;
      }
    }
  },

  async extensionById(aId) {
    const addon = await lazy.AddonManager.getAddonByID(aId);
    if (!addon) {
      debug`Could not find extension with id=${aId}`;
      return null;
    }
    return addon;
  },

  async ensureBuiltIn(aUri, aId) {
    await lazy.AddonManager.readyPromise;
    // Although the add-on is privileged in practice due to it being installed
    // as a built-in extension, we pass isPrivileged=false since the exact flag
    // doesn't matter as we are only using ExtensionData to read the version.
    const extensionData = new lazy.ExtensionData(aUri, false);
    const [extensionVersion, extension] = await Promise.all([
      extensionData.getExtensionVersionWithoutValidation(),
      this.extensionById(aId),
    ]);

    if (!extension || extensionVersion != extension.version) {
      return this.installBuiltIn(aUri);
    }

    const exported = await exportExtension(
      extension,
      extension.userPermissions,
      aUri
    );
    return { extension: exported };
  },

  async installBuiltIn(aUri) {
    await lazy.AddonManager.readyPromise;
    const addon = await lazy.AddonManager.installBuiltinAddon(aUri.spec);
    const exported = await exportExtension(addon, addon.userPermissions, aUri);
    return { extension: exported };
  },

  async installWebExtension(aInstallId, aUri) {
    const install = await lazy.AddonManager.getInstallForURL(aUri.spec, {
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
    const mimeType = lazy.mimeService.getTypeFromURI(aUri);
    lazy.AddonManager.installAddonFromWebpage(
      mimeType,
      null,
      systemPrincipal,
      install
    );

    return promise;
  },

  async setPrivateBrowsingAllowed(aId, aAllowed) {
    if (aAllowed) {
      await lazy.ExtensionPermissions.add(aId, PRIVATE_BROWSING_PERMISSION);
    } else {
      await lazy.ExtensionPermissions.remove(aId, PRIVATE_BROWSING_PERMISSION);
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
      return undefined;
    }

    const browserAction = this.browserActions.get(policy.extension);
    if (!browserAction) {
      return undefined;
    }

    return browserAction.triggerClickOrPopup();
  },

  async pageActionClick(aId) {
    const policy = WebExtensionPolicy.getByID(aId);
    if (!policy) {
      return undefined;
    }

    const pageAction = this.pageActions.get(policy.extension);
    if (!pageAction) {
      return undefined;
    }

    return pageAction.triggerClickOrPopup();
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
      aAddon.findUpdates(
        listener,
        lazy.AddonManager.UPDATE_WHEN_USER_REQUESTED
      );
    });
  },

  async updateWebExtension(aId) {
    // Refresh the cached metadata when necessary. This allows us to always
    // export relatively recent metadata to the embedder.
    if (lazy.AddonRepository.isMetadataStale()) {
      // We use a promise to avoid more than one call to `backgroundUpdateCheck()`
      // when `updateWebExtension()` is called for multiple add-ons in parallel.
      if (!this._promiseAddonRepositoryUpdate) {
        this._promiseAddonRepositoryUpdate =
          lazy.AddonRepository.backgroundUpdateCheck().finally(() => {
            this._promiseAddonRepositoryUpdate = null;
          });
      }
      await this._promiseAddonRepositoryUpdate;
    }

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

  validateBuiltInLocation(aLocationUri, aCallback) {
    let uri;
    try {
      uri = Services.io.newURI(aLocationUri);
    } catch (ex) {
      aCallback.onError(`Could not parse uri: ${aLocationUri}. Error: ${ex}`);
      return null;
    }

    if (uri.scheme !== "resource" || uri.host !== "android") {
      aCallback.onError(`Only resource://android/... URIs are allowed.`);
      return null;
    }

    if (uri.fileName !== "") {
      aCallback.onError(
        `This URI does not point to a folder. Note: folders URIs must end with a "/".`
      );
      return null;
    }

    return uri;
  },

  /* eslint-disable complexity */
  async onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;

    switch (aEvent) {
      case "GeckoView:BrowserAction:Click": {
        const popupUrl = await this.browserActionClick(aData.extensionId);
        aCallback.onSuccess(popupUrl);
        break;
      }
      case "GeckoView:PageAction:Click": {
        const popupUrl = await this.pageActionClick(aData.extensionId);
        aCallback.onSuccess(popupUrl);
        break;
      }
      case "GeckoView:WebExtension:MenuClick": {
        aCallback.onError(`Not implemented`);
        break;
      }
      case "GeckoView:WebExtension:MenuShow": {
        aCallback.onError(`Not implemented`);
        break;
      }
      case "GeckoView:WebExtension:MenuHide": {
        aCallback.onError(`Not implemented`);
        break;
      }

      case "GeckoView:ActionDelegate:Attached": {
        this.actionDelegateAttached(aData.extensionId);
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
          extension: await exportExtension(
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
        let uri;
        try {
          uri = Services.io.newURI(locationUri);
        } catch (ex) {
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

      case "GeckoView:WebExtension:EnsureBuiltIn": {
        const { locationUri, webExtensionId } = aData;
        const uri = this.validateBuiltInLocation(locationUri, aCallback);
        if (!uri) {
          return;
        }

        try {
          const result = await this.ensureBuiltIn(uri, webExtensionId);
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
        const uri = this.validateBuiltInLocation(aData.locationUri, aCallback);
        if (!uri) {
          return;
        }

        try {
          const result = await this.installBuiltIn(uri);
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
          await lazy.AddonManager.readyPromise;
          const addons = await lazy.AddonManager.getAddonsByTypes([
            "extension",
          ]);
          const extensions = await Promise.all(
            addons.map(addon =>
              exportExtension(addon, addon.userPermissions, null)
            )
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

// WeakMap[Extension -> BrowserAction]
GeckoViewWebExtension.browserActions = new WeakMap();
// WeakMap[Extension -> PageAction]
GeckoViewWebExtension.pageActions = new WeakMap();
