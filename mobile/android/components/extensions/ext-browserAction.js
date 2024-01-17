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

const { BrowserActionBase } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionActions.sys.mjs"
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

  openPopup(tab, openPopupWithoutUserInteraction = false) {
    const popupUri = openPopupWithoutUserInteraction
      ? this.getPopupUrl(tab)
      : this.triggerClickOrPopup(tab);
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

this.browserAction = class extends ExtensionAPIPersistent {
  static for(extension) {
    return GeckoViewWebExtension.browserActions.get(extension);
  }

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

  PERSISTENT_EVENTS = {
    onClicked({ fire }) {
      const { extension } = this;
      const { tabManager } = extension;
      async function listener(_event, tab) {
        if (fire.wakeup) {
          await fire.wakeup();
        }
        // TODO: we should double-check if the tab is already being closed by the time
        // the background script got started and we converted the primed listener.
        fire.sync(tabManager.convert(tab));
      }
      this.on("click", listener);
      return {
        unregister: () => {
          this.off("click", listener);
        },
        convert(newFire) {
          fire = newFire;
        },
      };
    },
  };

  getAPI(context) {
    const { extension } = context;
    const { action } = this;
    const namespace =
      extension.manifestVersion < 3 ? "browserAction" : "action";

    return {
      [namespace]: {
        ...action.api(context),

        onClicked: new EventManager({
          context,
          // module name is "browserAction" because it the name used in the
          // ext-android.json, independently from the manifest version.
          module: "browserAction",
          event: "onClicked",
          inputHandling: true,
          extensionApi: this,
        }).api(),

        getUserSettings: () => {
          return {
            // isOnToolbar is not supported on Android.
            // We intentionally omit the property, in case
            // extensions would like to feature-detect support
            // for this feature.
          };
        },
        openPopup: options => {
          const isHandlingUserInput =
            context.callContextData?.isHandlingUserInput;

          if (
            !Services.prefs.getBoolPref(
              "extensions.openPopupWithoutUserGesture.enabled"
            ) &&
            !isHandlingUserInput
          ) {
            throw new ExtensionError("openPopup requires a user gesture");
          }

          const currentWindow = windowTracker.getCurrentWindow(context);

          const window =
            typeof options?.windowId === "number"
              ? windowTracker.getWindow(options.windowId, context)
              : currentWindow;

          if (window !== currentWindow) {
            throw new ExtensionError(
              "Only the current window is supported on Android."
            );
          }

          if (this.action.getPopupUrl(window.tab, true)) {
            action.openPopup(window.tab, !isHandlingUserInput);
          }
        },
      },
    };
  }
};

global.browserActionFor = this.browserAction.for;
