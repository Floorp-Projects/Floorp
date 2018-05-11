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
 * normally consists of a JSM module file with an optional content module file.
 * The module file contains a class that extends GeckoViewModule, and the
 * content module file contains a class that extends GeckoViewContentModule. A
 * module usually pairs with a particular GeckoSessionHandler or delegate on the
 * Java side, and automatically receives module lifetime events such as
 * initialization, change in enabled state, and change in settings.
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

    window.document.documentElement.appendChild(aBrowser);

    WindowEventDispatcher.registerListener(this, [
      "GeckoView:UpdateModuleState",
      "GeckoView:UpdateInitData",
      "GeckoView:UpdateSettings",
    ]);

    this.messageManager.addMessageListener("GeckoView:ContentModuleLoaded",
                                           this);

    this.forEach(module => {
      module.onInit();
    });
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
      if (module.enabled && module.impl) {
        module.impl.onSettingsUpdate();
      }
    });

    this._browser.messageManager.sendAsyncMessage("GeckoView:UpdateSettings",
                                                  aSettings);
  },

  onEvent(aEvent, aData, aCallback) {
    debug `onEvent ${aEvent} ${aData}`;
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

  receiveMessage(aMsg) {
    debug `receiveMessage ${aMsg.name} ${aMsg.data}`;
    switch (aMsg.name) {
      case "GeckoView:ContentModuleLoaded": {
        const module = this._modules.get(aMsg.data.module);
        if (module) {
          module.onContentModuleLoaded();
        }
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
  /**
   * Create a ModuleInfo instance. See _loadPhase for phase object description.
   *
   * @param manager the ModuleManager instance.
   * @param name Name of the module.
   * @param enabled Enabled state of the module at startup.
   * @param onInit Phase object for the init phase, when the window is created.
   * @param onEnable Phase object for the enable phase, when the module is first
   *                 enabled by setting a delegate in Java.
   */
  constructor({manager, name, enabled, onInit, onEnable}) {
    this._manager = manager;
    this._name = name;

    this._impl = null;
    this._contentModuleLoaded = false;
    this._enabled = false;
    // Only enable once we performed initialization.
    this._enabledOnInit = enabled;

    // For init, load resource _before_ initializing browser to support the
    // onInitBrowser() override. However, load content module after initializing
    // browser, because we don't have a message manager before then.
    this._loadPhase({
      resource: onInit && onInit.resource,
    });
    this._onInitPhase = {
      frameScript: onInit && onInit.frameScript,
    };
    this._onEnablePhase = onEnable;
  }

  onInit() {
    if (this._impl) {
      this._impl.onInit();
    }
    this._loadPhase(this._onInitPhase);
    this._onInitPhase = null;

    this.enabled = this._enabledOnInit;
  }

  /**
   * Load resources according to a phase object that contains possible keys,
   *
   * "resource": specify the JSM resource to load for this module.
   * "frameScript": specify a content JS frame script to load for this module.
   */
  _loadPhase(aPhase) {
    if (!aPhase) {
      return;
    }

    if (aPhase.resource && !this._impl) {
      const scope = {};
      const global = ChromeUtils.import(aPhase.resource, scope);
      const tag = this._name.replace("GeckoView", "GeckoView.");
      GeckoViewUtils.initLogging(tag, global);
      this._impl = new scope[this._name](this);
    }

    if (aPhase.frameScript && !this._contentModuleLoaded) {
      if (this._impl) {
        this._impl.onLoadContentModule();
      }
      this._manager.messageManager.loadFrameScript(aPhase.frameScript, true);
      this._contentModuleLoaded = true;
    }
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

    if (!aEnabled && this._impl) {
      this._impl.onDisable();
    }

    this._enabled = aEnabled;

    if (aEnabled) {
      this._loadPhase(this._onEnablePhase);
      this._onEnablePhase = null;
      if (this._impl) {
        this._impl.onEnable();
        this._impl.onSettingsUpdate();
      }
    }

    this._updateContentModuleState(/* includeSettings */ aEnabled);
  }

  onContentModuleLoaded() {
    this._updateContentModuleState(/* includeSettings */ true);

    if (this._impl) {
      this._impl.onContentModuleLoaded();
    }
  }

  _updateContentModuleState(aIncludeSettings) {
    this._manager.messageManager.sendAsyncMessage("GeckoView:UpdateModuleState", {
      module: this._name,
      enabled: this.enabled,
      settings: aIncludeSettings ? this._manager.settings : null,
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
    onInit: {
      resource: "resource://gre/modules/GeckoViewAccessibility.jsm",
    },
  }, {
    name: "GeckoViewContent",
    onInit: {
      resource: "resource://gre/modules/GeckoViewContent.jsm",
      frameScript: "chrome://geckoview/content/GeckoViewContent.js",
    },
  }, {
    name: "GeckoViewNavigation",
    onInit: {
      resource: "resource://gre/modules/GeckoViewNavigation.jsm",
    },
    onEnable: {
      frameScript: "chrome://geckoview/content/GeckoViewNavigationContent.js",
    },
  }, {
    name: "GeckoViewProgress",
    onEnable: {
      resource: "resource://gre/modules/GeckoViewProgress.jsm",
    },
  }, {
    name: "GeckoViewScroll",
    onEnable: {
      frameScript: "chrome://geckoview/content/GeckoViewScrollContent.js",
    },
  }, {
    name: "GeckoViewSelectionAction",
    onEnable: {
      frameScript: "chrome://geckoview/content/GeckoViewSelectionActionContent.js",
    },
  }, {
    name: "GeckoViewSettings",
    onInit: {
      resource: "resource://gre/modules/GeckoViewSettings.jsm",
      frameScript: "chrome://geckoview/content/GeckoViewContentSettings.js",
    },
  }, {
    name: "GeckoViewTab",
    onInit: {
      resource: "resource://gre/modules/GeckoViewTab.jsm",
    },
  }, {
    name: "GeckoViewTrackingProtection",
    onEnable: {
      resource: "resource://gre/modules/GeckoViewTrackingProtection.jsm",
    },
  }]);

  // Move focus to the content window at the end of startup,
  // so things like text selection can work properly.
  browser.focus();
}
