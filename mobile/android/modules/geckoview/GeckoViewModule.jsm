/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewModule"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "dump", () =>
    ChromeUtils.import("resource://gre/modules/AndroidLog.jsm",
                       {}).AndroidLog.d.bind(null, "ViewModule"));

// function debug(aMsg) {
//   dump(aMsg);
// }

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

    this.init();
    this.onSettingsUpdate();
  }

  // Override this with module initialization.
  init() {}

  // Called when settings have changed. Access settings via this.settings.
  onSettingsUpdate() {}

  _register() {
    if (this.isRegistered) {
      return;
    }
    this.register();
    this.isRegistered = true;
  }

  register() {}

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
        self._eventProxy.dispatchQueuedEvents();
        self._eventProxy.enableQueuing(false);
    });
    this.messageManager.loadFrameScript(aUri, true);
  }

  registerListener(aEventList) {
    this._eventProxy.registerListener(aEventList);
  }

  _unregister() {
    if (!this.isRegistered) {
      return;
    }
    this._eventProxy.unregisterListener();
    this.unregister();
    this.isRegistered = false;
  }

  unregister() {}

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
    debug("register " + aEventList);
    this.eventDispatcher.registerListener(this, aEventList);
    this._registeredEvents = this._registeredEvents.concat(aEventList);
  }

  unregisterListener() {
    debug("unregister");
    if (this._registeredEvents.length === 0) {
      return;
    }
    this.eventDispatcher.unregisterListener(this, this._registeredEvents);
    this._registeredEvents = [];
  }

  onEvent(aEvent, aData, aCallback) {
    if (this._enableQueuing) {
      debug("queue " + aEvent + ", aData=" + JSON.stringify(aData));
      this._eventQueue.unshift(arguments);
    } else {
      this._dispatch.apply(this, arguments);
    }
  }

  enableQueuing(aEnable) {
    debug("enableQueuing " + aEnable);
    this._enableQueuing = aEnable;
  }

  _dispatch(aEvent, aData, aCallback) {
    debug("dispatch " + aEvent + ", aData=" + JSON.stringify(aData));
    this.listener.onEvent.apply(this.listener, arguments);
  }

  dispatchQueuedEvents() {
    debug("dispatchQueued");
    while (this._eventQueue.length) {
      const e = this._eventQueue.pop();
      this._dispatch.apply(this, e);
    }
  }
}
