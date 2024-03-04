/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewUtils } from "resource://gre/modules/GeckoViewUtils.sys.mjs";

const { debug, warn } = GeckoViewUtils.initLogging("Module");

export class GeckoViewModule {
  static initLogging(aModuleName) {
    const tag = aModuleName.replace("GeckoView", "");
    return GeckoViewUtils.initLogging(tag);
  }

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
    return this.moduleManager.window;
  }

  getActor(aActorName) {
    return this.moduleManager.getActor(aActorName);
  }

  get browser() {
    return this.moduleManager.browser;
  }

  get messageManager() {
    return this.moduleManager.messageManager;
  }

  get eventDispatcher() {
    return this.moduleManager.eventDispatcher;
  }

  get settings() {
    return this.moduleManager.settings;
  }

  get moduleManager() {
    return this._info.manager;
  }

  // Override to initialize the browser before it is bound to the window.
  onInitBrowser() {}

  // Override to cleanup when the browser is destroyed.
  onDestroyBrowser() {}

  // Override to initialize module.
  onInit() {}

  // Override to cleanup when the window is closed
  onDestroy() {}

  // Override to detect settings change. Access settings via this.settings.
  onSettingsUpdate() {}

  // Override to enable module after setting a Java delegate.
  onEnable() {}

  // Override to disable module after clearing the Java delegate.
  onDisable() {}

  // Override to perform actions when content module has started loading;
  // by default, pause events so events that depend on content modules can work.
  onLoadContentModule() {
    this._eventProxy.enableQueuing(true);
  }

  // Override to perform actions when content module has finished loading;
  // by default, un-pause events and flush queued events.
  onContentModuleLoaded() {
    this._eventProxy.enableQueuing(false);
    this._eventProxy.dispatchQueuedEvents();
  }

  registerListener(aEventList) {
    this._eventProxy.registerListener(aEventList);
  }

  unregisterListener(aEventList) {
    this._eventProxy.unregisterListener(aEventList);
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
    debug`registerListener ${aEventList}`;
    this.eventDispatcher.registerListener(this, aEventList);
    this._registeredEvents = this._registeredEvents.concat(aEventList);
  }

  unregisterListener(aEventList) {
    debug`unregisterListener`;
    if (this._registeredEvents.length === 0) {
      return;
    }

    if (!aEventList) {
      this.eventDispatcher.unregisterListener(this, this._registeredEvents);
      this._registeredEvents = [];
    } else {
      this.eventDispatcher.unregisterListener(this, aEventList);
      this._registeredEvents = this._registeredEvents.filter(
        e => !aEventList.includes(e)
      );
    }
  }

  onEvent(aEvent, aData) {
    if (this._enableQueuing) {
      debug`queue ${aEvent}, data=${aData}`;
      this._eventQueue.unshift(arguments);
    } else {
      this._dispatch(...arguments);
    }
  }

  enableQueuing(aEnable) {
    debug`enableQueuing ${aEnable}`;
    this._enableQueuing = aEnable;
  }

  _dispatch(aEvent, aData) {
    debug`dispatch ${aEvent}, data=${aData}`;
    if (this.listener.onEvent) {
      this.listener.onEvent(...arguments);
    } else {
      this.listener(...arguments);
    }
  }

  dispatchQueuedEvents() {
    debug`dispatchQueued`;
    while (this._eventQueue.length) {
      const args = this._eventQueue.pop();
      this._dispatch(...args);
    }
  }
}
