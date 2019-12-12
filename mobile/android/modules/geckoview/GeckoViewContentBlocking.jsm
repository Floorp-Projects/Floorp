/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewContentBlocking"];

const { GeckoViewModule } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewModule.jsm"
);

class GeckoViewContentBlocking extends GeckoViewModule {
  onEnable() {
    this.registerListener(["ContentBlocking:RequestLog"]);

    this.messageManager.addMessageListener("ContentBlocking:ExportLog", this);
  }

  onDisable() {
    this.unregisterListener(["ContentBlocking:RequestLog"]);

    this.messageManager.removeMessageListener(
      "ContentBlocking:ExportLog",
      this
    );
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "ContentBlocking:RequestLog": {
        if (!this._requestLogCallbacks) {
          this._requestLogCallbacks = new Map();
          this._requestLogId = 0;
        }
        this._requestLogCallbacks.set(this._requestLogId, aCallback);
        this.messageManager.sendAsyncMessage("ContentBlocking:RequestLog", {
          id: this._requestLogId,
        });
        this._requestLogId++;
        break;
      }
    }
  }

  // Message manager event handler.
  receiveMessage(aMsg) {
    debug`receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "ContentBlocking:ExportLog": {
        if (
          !this._requestLogCallbacks ||
          !this._requestLogCallbacks.has(aMsg.data.id)
        ) {
          warn`Failed to export content blocking log.`;
          return;
        }

        const callback = this._requestLogCallbacks.get(aMsg.data.id);

        if (!aMsg.data.log) {
          warn`Failed to export content blocking log.`;
          callback.onError(aMsg.data.error);
          // Clean up the callback even on a failed response.
          this._requestLogCallbacks.delete(aMsg.data.id);
          return;
        }

        const res = Object.keys(aMsg.data.log).map(key => {
          const blockData = aMsg.data.log[key].map(data => {
            return {
              category: data[0],
              blocked: data[1],
              count: data[2],
            };
          });
          return {
            origin: key,
            blockData: blockData,
          };
        });
        callback.onSuccess({ log: res });

        this._requestLogCallbacks.delete(aMsg.data.id);
        break;
      }
    }
  }
}

const { debug, warn } = GeckoViewContentBlocking.initLogging(
  "GeckoViewContentBlocking"
); // eslint-disable-line no-unused-vars
