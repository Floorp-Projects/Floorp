/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(this, "WindowEventDispatcher",
  () => EventDispatcher.for(window));

/**
 * ModuleManager creates and manages GeckoView modules. Each GeckoView module
 * normally consists of a JSM file with an optional frame script file. The JSM
 * file contains a class that extends GeckoViewModule. A module usually pairs
 * with a particular GeckoSessionHandler or delegate on the Java side, and
 * automatically receives module lifetime events such as initialization, change
 * in enabled state, and change in settings.
 */
var ModuleManager = {
  get _initData() {
    return window.arguments[0].QueryInterface(Ci.nsIAndroidView).initData;
  },

  init(aBrowser, aModules) {
    const initData = this._initData;
    this._browser = aBrowser;
    this._settings = initData.settings;
    this._frozenSettings = Object.freeze(Object.assign({}, this._settings));

    const self = this;
    this._modules = new Map((function* () {
      for (const module of aModules) {
        yield [
          module.name,
          new ModuleInfo({
            enabled: !!initData.modules[module.name],
            manager: self,
            ...module,
          }),
        ];
      }
    })());

    WindowEventDispatcher.registerListener(this, [
      "GeckoView:UpdateModuleState",
      "GeckoView:UpdateInitData",
      "GeckoView:UpdateSettings",
    ]);
  },

  get window() {
    return window;
  },

  get browser() {
    return this._browser;
  },

  get messageManager() {
    return this._browser.messageManager;
  },

  get eventDispatcher() {
    return WindowEventDispatcher;
  },

  get settings() {
    return this._frozenSettings;
  },

  forEach(aCallback) {
    this._modules.forEach(aCallback, this);
  },

  _updateSettings(aSettings) {
    Object.assign(this._settings, aSettings);
    this._frozenSettings = Object.freeze(Object.assign({}, this._settings));

    this.forEach(module => {
      if (module.enabled) {
        module.impl.onSettingsUpdate();
      }
    });

    this._browser.messageManager.sendAsyncMessage("GeckoView:UpdateSettings",
                                                  aSettings);
  },

  onEvent(aEvent, aData, aCallback) {
    switch (aEvent) {
      case "GeckoView:UpdateModuleState": {
        const module = this._modules.get(aData.module);
        if (module) {
          module.enabled = aData.enabled;
        }
        break;
      }

      case "GeckoView:UpdateInitData": {
        // Replace all settings during a transfer.
        const initData = this._initData;
        this._updateSettings(initData.settings);

        // Update module enabled states.
        for (const name in initData.modules) {
          const module = this._modules.get(name);
          if (module) {
            module.enabled = initData.modules[name];
          }
        }
        break;
      }

      case "GeckoView:UpdateSettings": {
        this._updateSettings(aData);
        break;
      }
    }
  },
};

/**
 * ModuleInfo is the structure used by ModuleManager to represent individual
 * modules. It is responsible for loading the module JSM file if necessary,
 * and it acts as the intermediary between ModuleManager and the module
 * object that extends GeckoViewModule.
 */
class ModuleInfo {
  constructor({enabled, manager, name, resource}) {
    this._manager = manager;
    this._name = name;

    const scope = {};
    const global = ChromeUtils.import(resource, scope);
    const tag = name.replace("GeckoView", "GeckoView.");
    GeckoViewUtils.initLogging(tag, global);

    this._impl = new scope[name](this);
    this._enabled = false;
    // Only enable once we performed initialization.
    this._enabledOnInit = enabled;
  }

  onInit() {
    this._impl.onInit();
    this.enabled = this._enabledOnInit;
  }

  get manager() {
    return this._manager;
  }

  get name() {
    return this._name;
  }

  get impl() {
    return this._impl;
  }

  get enabled() {
    return this._enabled;
  }

  set enabled(aEnabled) {
    if (aEnabled === this._enabled) {
      return;
    }

    if (!aEnabled) {
      this._impl.onDisable();
    }

    this._enabled = aEnabled;

    if (aEnabled) {
      this._impl.onEnable();
      this._impl.onSettingsUpdate();
    }

    this._manager.messageManager.sendAsyncMessage("GeckoView:UpdateModuleState", {
      module: this._name,
      enabled: aEnabled,
      settings: aEnabled ? this._manager.settings : null,
    });
  }
}

function createBrowser() {
  const browser = window.browser = document.createElement("browser");
  browser.setAttribute("type", "content");
  browser.setAttribute("primary", "true");
  browser.setAttribute("flex", "1");
  return browser;
}

function startup() {
  GeckoViewUtils.initLogging("GeckoView.XUL", window);

  const browser = createBrowser();
  ModuleManager.init(browser, [{
    name: "GeckoViewAccessibility",
    resource: "resource://gre/modules/GeckoViewAccessibility.jsm",
  }, {
    name: "GeckoViewContent",
    resource: "resource://gre/modules/GeckoViewContent.jsm",
  }, {
    name: "GeckoViewNavigation",
    resource: "resource://gre/modules/GeckoViewNavigation.jsm",
  }, {
    name: "GeckoViewProgress",
    resource: "resource://gre/modules/GeckoViewProgress.jsm",
  }, {
    name: "GeckoViewScroll",
    resource: "resource://gre/modules/GeckoViewScroll.jsm",
  }, {
    name: "GeckoViewSelectionAction",
    resource: "resource://gre/modules/GeckoViewSelectionAction.jsm",
  }, {
    name: "GeckoViewSettings",
    resource: "resource://gre/modules/GeckoViewSettings.jsm",
  }, {
    name: "GeckoViewTab",
    resource: "resource://gre/modules/GeckoViewTab.jsm",
  }, {
    name: "GeckoViewTrackingProtection",
    resource: "resource://gre/modules/GeckoViewTrackingProtection.jsm",
  }]);

  window.document.documentElement.appendChild(browser);

  ModuleManager.forEach(module => {
    module.onInit();
  });

  // Move focus to the content window at the end of startup,
  // so things like text selection can work properly.
  browser.focus();
}
