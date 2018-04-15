/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewModule"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

GeckoViewUtils.initLogging("GeckoView.Module", this);

class GeckoViewModule {
  constructor(aModuleName, aWindow, aBrowser, aEventDispatcher) {
    this.isRegistered = false;
    this.window = aWindow;
    this.browser = aBrowser;
    this.eventDispatcher = aEventDispatcher;
    this.moduleName = aModuleName;
    this._isContentLoaded = false;
    this._eventProxy = new EventProxy(this, this.eventDispatcher);

    this.eventDispatcher.registerListener(
      (aEvent, aData, aCallback) => {
        this.messageManager.sendAsyncMessage("GeckoView:UpdateSettings",
                                             this.settings);
        this.onSettingsUpdate();
      }, "GeckoView:UpdateSettings"
    );

    this.eventDispatcher.registerListener(
      (aEvent, aData, aCallback) => {
        if (aData.module == this.moduleName) {
          this._register();
          aData.settings = this.settings;
          this.messageManager.sendAsyncMessage("GeckoView:Register", aData);
        }
      }, "GeckoView:Register"
    );

    this.eventDispatcher.registerListener(
      (aEvent, aData, aCallback) => {
        if (aData.module == this.moduleName) {
          this.messageManager.sendAsyncMessage("GeckoView:Unregister", aData);
          this._unregister();
        }
      }, "GeckoView:Unregister"
    );

    this.onInitBrowser();
  }

  // Override to initialize the browser before it is bound to the window.
  onInitBrowser() {}

  // Override to initialize module.
  onInit() {}

  // Override to detect settings change. Access settings via this.settings.
  onSettingsUpdate() {}

  // Override to enable module after setting a Java delegate.
  onEnable() {}

  // Override to disable module after clearing the Java delegate.
  onDisable() {}

  _register() {
    if (this.isRegistered) {
      return;
    }
    this.onEnable();
    this.isRegistered = true;
  }

  _unregister() {
    if (!this.isRegistered) {
      return;
    }
    this.onDisable();
    this.isRegistered = false;
  }

  registerContent(aUri) {
    if (this._isContentLoaded) {
      return;
    }
    this._isContentLoaded = true;
    this._eventProxy.enableQueuing(true);

    let self = this;
    this.messageManager.addMessageListener("GeckoView:ContentRegistered",
      function listener(aMsg) {
        if (aMsg.data.module !== self.moduleName) {
          return;
        }
        self.messageManager.removeMessageListener("GeckoView:ContentRegistered",
                                                  listener);
        self._eventProxy.enableQueuing(false);
        self._eventProxy.dispatchQueuedEvents();
    });
    this.messageManager.loadFrameScript(aUri, true);
  }

  registerListener(aEventList) {
    this._eventProxy.registerListener(aEventList);
  }

  unregisterListener() {
    this._eventProxy.unregisterListener();
  }

  get settings() {
    let view = this.window.arguments[0].QueryInterface(Ci.nsIAndroidView);
    return Object.freeze(view.settings);
  }

  get messageManager() {
    return this.browser.messageManager;
  }
}

class EventProxy {
  constructor(aListener, aEventDispatcher) {
    this.listener = aListener;
    this.eventDispatcher = aEventDispatcher;
    this._eventQueue = [];
    this._registeredEvents = [];
    this._enableQueuing = false;
  }

  registerListener(aEventList) {
    debug `registerListener ${aEventList}`;
    this.eventDispatcher.registerListener(this, aEventList);
    this._registeredEvents = this._registeredEvents.concat(aEventList);
  }

  unregisterListener() {
    debug `unregisterListener`;
    if (this._registeredEvents.length === 0) {
      return;
    }
    this.eventDispatcher.unregisterListener(this, this._registeredEvents);
    this._registeredEvents = [];
  }

  onEvent(aEvent, aData, aCallback) {
    if (this._enableQueuing) {
      debug `queue ${aEvent}, data=${aData}`;
      this._eventQueue.unshift(arguments);
    } else {
      this._dispatch(...arguments);
    }
  }

  enableQueuing(aEnable) {
    debug `enableQueuing ${aEnable}`;
    this._enableQueuing = aEnable;
  }

  _dispatch(aEvent, aData, aCallback) {
    debug `dispatch ${aEvent}, data=${aData}`;
    if (this.listener.onEvent) {
      this.listener.onEvent(...arguments);
    } else {
      this.listener(...arguments);
    }
  }

  dispatchQueuedEvents() {
    debug `dispatchQueued`;
    while (this._eventQueue.length) {
      const args = this._eventQueue.pop();
      this._dispatch(...args);
    }
  }
}
