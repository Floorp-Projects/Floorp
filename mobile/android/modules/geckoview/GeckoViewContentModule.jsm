/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewContentModule"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

GeckoViewUtils.initLogging("GeckoView.Module.[C]", this);

class GeckoViewContentModule {
  static initLogging(aModuleName) {
    this._moduleName = aModuleName;
    const tag = aModuleName.replace("GeckoView", "GeckoView.") + ".[C]";
    return GeckoViewUtils.initLogging(tag, {});
  }

  static create(aGlobal, aModuleName) {
    return new this(aModuleName || this._moduleName, aGlobal);
  }

  constructor(aModuleName, aGlobal) {
    this.moduleName = aModuleName;
    this.messageManager = aGlobal;

    if (!aGlobal._gvEventDispatcher) {
      aGlobal._gvEventDispatcher =
          GeckoViewUtils.getDispatcherForWindow(aGlobal.content);
      aGlobal.addEventListener("unload", event => {
        if (event.target === this.messageManager) {
          aGlobal._gvEventDispatcher.finalize();
        }
      }, {
        mozSystemGroup: true,
      });
    }
    this.eventDispatcher = aGlobal._gvEventDispatcher;

    this.messageManager.addMessageListener(
      "GeckoView:UpdateSettings",
      aMsg => {
        this.settings = aMsg.data;
        this.onSettingsUpdate();
      }
    );
    this.messageManager.addMessageListener(
      "GeckoView:Register",
      aMsg => {
        if (aMsg.data.module == this.moduleName) {
          this.settings = aMsg.data.settings;
          this.onEnable();
          this.messageManager.sendAsyncMessage(
            "GeckoView:ContentRegistered", { module: this.moduleName });
        }
      }
    );
    this.messageManager.addMessageListener(
      "GeckoView:Unregister",
      aMsg => {
        if (aMsg.data.module == this.moduleName) {
          this.onDisable();
        }
      }
    );

    this.onInit();
  }

  // Override to initialize module.
  onInit() {}

  // Override to detect settings change. Access settings via this.settings.
  onSettingsUpdate() {}

  // Override to enable module after setting a Java delegate.
  onEnable() {}

  // Override to disable module after clearing the Java delegate.
  onDisable() {}
}
