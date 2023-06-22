/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  GeckoViewWebExtension: "resource://gre/modules/GeckoViewWebExtension.sys.mjs",
  ExtensionActionHelper: "resource://gre/modules/GeckoViewWebExtension.sys.mjs",
});

const { PageActionBase } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionActions.sys.mjs"
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

this.pageAction = class extends ExtensionAPIPersistent {
  static for(extension) {
    return GeckoViewWebExtension.pageActions.get(extension);
  }

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

  PERSISTENT_EVENTS = {
    onClicked({ fire }) {
      const { extension } = this;
      const { tabManager } = extension;

      const listener = async (_event, tab) => {
        if (fire.wakeup) {
          await fire.wakeup();
        }
        // TODO: we should double-check if the tab is already being closed by the time
        // the background script got started and we converted the primed listener.
        fire.async(tabManager.convert(tab));
      };

      this.on("click", listener);
      return {
        unregister: () => {
          this.off("click", listener);
        },
        convert(newFire, _extContext) {
          fire = newFire;
        },
      };
    },
  };

  getAPI(context) {
    const { action } = this;

    return {
      pageAction: {
        ...action.api(context),

        onClicked: new EventManager({
          context,
          module: "pageAction",
          event: "onClicked",
          inputHandling: true,
          extensionApi: this,
        }).api(),

        openPopup() {
          action.openPopup();
        },
      },
    };
  }
};

global.pageActionFor = this.pageAction.for;
