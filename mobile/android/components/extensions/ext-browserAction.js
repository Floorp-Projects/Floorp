/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-android.js */

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewWebExtension: "resource://gre/modules/GeckoViewWebExtension.jsm",
  ExtensionActionHelper: "resource://gre/modules/GeckoViewWebExtension.jsm",
});

const { BrowserActionBase } = ChromeUtils.import(
  "resource://gre/modules/ExtensionActions.jsm"
);

const BROWSER_ACTION_PROPERTIES = [
  "title",
  "icon",
  "popup",
  "badgeText",
  "badgeBackgroundColor",
  "badgeTextColor",
  "enabled",
  "patternMatching",
];

class BrowserAction extends BrowserActionBase {
  constructor(extension, clickDelegate) {
    const tabContext = new TabContext(tabId => this.getContextData(null));
    super(tabContext, extension);
    this.clickDelegate = clickDelegate;
    this.helper = new ExtensionActionHelper({
      extension,
      tabTracker,
      windowTracker,
      tabContext,
      properties: BROWSER_ACTION_PROPERTIES,
    });
  }

  updateOnChange(tab) {
    const tabId = tab ? tab.id : null;
    const action = tab
      ? this.getContextData(tab)
      : this.helper.extractProperties(this.globals);
    this.helper.sendRequest(tabId, {
      action,
      type: "GeckoView:BrowserAction:Update",
    });
  }

  openPopup() {
    const tab = tabTracker.activeTab;
    const popupUri = this.triggerClickOrPopup(tab);
    const actionObject = this.getContextData(tab);
    const action = this.helper.extractProperties(actionObject);
    this.helper.sendRequest(tab.id, {
      action,
      type: "GeckoView:BrowserAction:OpenPopup",
      popupUri,
    });
  }

  triggerClickOrPopup(tab = tabTracker.activeTab) {
    return super.triggerClickOrPopup(tab);
  }

  getTab(tabId) {
    return this.helper.getTab(tabId);
  }

  getWindow(windowId) {
    return this.helper.getWindow(windowId);
  }

  dispatchClick() {
    this.clickDelegate.onClick();
  }
}

this.browserAction = class extends ExtensionAPI {
  async onManifestEntry(entryName) {
    const { extension } = this;
    this.action = new BrowserAction(extension, this);
    await this.action.loadIconData();

    GeckoViewWebExtension.browserActions.set(extension, this.action);

    // Notify the embedder of this action
    this.action.updateOnChange(null);
  }

  onShutdown() {
    const { extension } = this;
    this.action.onShutdown();
    GeckoViewWebExtension.browserActions.delete(extension);
  }

  onClick() {
    this.emit("click", tabTracker.activeTab);
  }

  getAPI(context) {
    const { extension } = context;
    const { tabManager } = extension;
    const { action } = this;
    const namespace =
      extension.manifestVersion < 3 ? "browserAction" : "action";

    return {
      [namespace]: {
        ...action.api(context),

        onClicked: new EventManager({
          context,
          name: `${namespace}.onClicked`,
          register: fire => {
            const listener = (event, tab) => {
              fire.async(tabManager.convert(tab));
            };
            this.on("click", listener);
            return () => {
              this.off("click", listener);
            };
          },
        }).api(),

        openPopup: function() {
          action.openPopup();
        },
      },
    };
  }
};

global.browserActionFor = this.browserAction.for;
