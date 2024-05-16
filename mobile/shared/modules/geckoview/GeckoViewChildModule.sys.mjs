/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

const { debug, warn } = GeckoViewUtils.initLogging("Module[C]");

export class GeckoViewChildModule {
  static initLogging(aModuleName) {
    this._moduleName = aModuleName;
    const tag = aModuleName.replace("GeckoView", "") + "[C]";
    return GeckoViewUtils.initLogging(tag);
  }

  static create(aGlobal, aModuleName) {
    return new this(aModuleName || this._moduleName, aGlobal);
  }

  constructor(aModuleName, aGlobal) {
    this.moduleName = aModuleName;
    this.messageManager = aGlobal;
    this.enabled = false;

    if (!aGlobal._gvEventDispatcher) {
      aGlobal._gvEventDispatcher = GeckoViewUtils.getDispatcherForWindow(
        aGlobal.content
      );
      aGlobal.addEventListener(
        "unload",
        event => {
          if (event.target === this.messageManager) {
            aGlobal._gvEventDispatcher.finalize();
          }
        },
        {
          mozSystemGroup: true,
        }
      );
    }
    this.eventDispatcher = aGlobal._gvEventDispatcher;

    this.messageManager.addMessageListener(
      "GeckoView:UpdateModuleState",
      aMsg => {
        if (aMsg.data.module !== this.moduleName) {
          return;
        }

        const { enabled } = aMsg.data;

        if (enabled !== this.enabled) {
          if (!enabled) {
            this.onDisable();
          }

          this.enabled = enabled;

          if (enabled) {
            this.onEnable();
          }
        }
      }
    );

    this.onInit();

    this.messageManager.sendAsyncMessage("GeckoView:ContentModuleLoaded", {
      module: this.moduleName,
    });
  }

  // Override to initialize module.
  onInit() {}

  // Override to enable module after setting a Java delegate.
  onEnable() {}

  // Override to disable module after clearing the Java delegate.
  onDisable() {}
}
