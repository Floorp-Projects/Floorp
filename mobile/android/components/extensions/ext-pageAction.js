/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-android.js */

XPCOMUtils.defineLazyModuleGetters(this, {
  GeckoViewWebExtension: "resource://gre/modules/GeckoViewWebExtension.jsm",
  ExtensionActionHelper: "resource://gre/modules/GeckoViewWebExtension.jsm",
});

const { PageActionBase } = ChromeUtils.import(
  "resource://gre/modules/ExtensionActions.jsm"
);

const PAGE_ACTION_PROPERTIES = [
  "title",
  "icon",
  "popup",
  "badgeText",
  "enabled",
  "patternMatching",
];

class PageAction extends PageActionBase {
  constructor(extension, clickDelegate) {
    const tabContext = new TabContext(tabId => this.getContextData(null));
    super(tabContext, extension);
    this.clickDelegate = clickDelegate;
    this.helper = new ExtensionActionHelper({
      extension,
      tabTracker,
      windowTracker,
      tabContext,
      properties: PAGE_ACTION_PROPERTIES,
    });
  }

  updateOnChange(tab) {
    const tabId = tab ? tab.id : null;
    // The embedder only gets the override, not the full object
    const action = tab
      ? this.getContextData(tab)
      : this.helper.extractProperties(this.globals);
    this.helper.sendRequest(tabId, {
      action,
      type: "GeckoView:PageAction:Update",
    });
  }

  openPopup() {
    const tab = tabTracker.activeTab;
    const popupUri = this.triggerClickOrPopup(tab);
    const actionObject = this.getContextData(tab);
    const action = this.helper.extractProperties(actionObject);
    this.helper.sendRequest(tab.id, {
      action,
      type: "GeckoView:PageAction:OpenPopup",
      popupUri,
    });
  }

  triggerClickOrPopup(tab = tabTracker.activeTab) {
    return super.triggerClickOrPopup(tab);
  }

  getTab(tabId) {
    return this.helper.getTab(tabId);
  }

  dispatchClick() {
    this.clickDelegate.onClick();
  }
}

this.pageAction = class extends ExtensionAPI {
  async onManifestEntry(entryName) {
    const { extension } = this;
    const action = new PageAction(extension, this);
    await action.loadIconData();
    this.action = action;

    GeckoViewWebExtension.pageActions.set(extension, action);

    // Notify the embedder of this action
    action.updateOnChange(null);
  }

  onClick() {
    this.emit("click", tabTracker.activeTab);
  }

  onShutdown() {
    const { extension, action } = this;
    action.onShutdown();
    GeckoViewWebExtension.pageActions.delete(extension);
  }

  getAPI(context) {
    const { extension } = context;
    const { tabManager } = extension;
    const { action } = this;

    return {
      pageAction: {
        ...action.api(context),

        onClicked: new EventManager({
          context,
          name: "pageAction.onClicked",
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

        openPopup() {
          action.openPopup();
        },
      },
    };
  }
};
