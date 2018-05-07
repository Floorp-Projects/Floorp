/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewModule"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

GeckoViewUtils.initLogging("GeckoView.Module", this);

class GeckoViewModule {
  constructor(aModuleInfo) {
    this._info = aModuleInfo;

    this._isContentLoaded = false;
    this._eventProxy = new EventProxy(this, this.eventDispatcher);

    this.onInitBrowser();
  }

  get name() {
    return this._info.name;
  }

  get enabled() {
    return this._info.enabled;
  }

  get window() {
    return this._info.manager.window;
  }

  get browser() {
    return this._info.manager.browser;
  }

  get messageManager() {
    return this._info.manager.messageManager;
  }

  get eventDispatcher() {
    return this._info.manager.eventDispatcher;
  }

  get settings() {
    return this._info.manager.settings;
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

  registerContent(aUri) {
    if (this._isContentLoaded) {
      return;
    }
    this._isContentLoaded = true;
    this._eventProxy.enableQueuing(true);

    let self = this;
    this.messageManager.addMessageListener("GeckoView:ContentRegistered",
      function listener(aMsg) {
        if (aMsg.data.module !== self.name) {
          return;
        }
        self.messageManager.removeMessageListener("GeckoView:ContentRegistered",
                                                  listener);
        self.messageManager.sendAsyncMessage("GeckoView:UpdateSettings",
                                             self.settings);
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
