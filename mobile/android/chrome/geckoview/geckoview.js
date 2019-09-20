/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { DelayedInit } = ChromeUtils.import(
  "resource://gre/modules/DelayedInit.jsm"
);
var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  HistogramStopwatch: "resource://gre/modules/GeckoViewTelemetry.jsm",
});

XPCOMUtils.defineLazyGetter(this, "WindowEventDispatcher", () =>
  EventDispatcher.for(window)
);

/**
 * ModuleManager creates and manages GeckoView modules. Each GeckoView module
 * normally consists of a JSM module file with an optional content module file.
 * The module file contains a class that extends GeckoViewModule, and the
 * content module file contains a class that extends GeckoViewChildModule. A
 * module usually pairs with a particular GeckoSessionHandler or delegate on the
 * Java side, and automatically receives module lifetime events such as
 * initialization, change in enabled state, and change in settings.
 */
var ModuleManager = {
  get _initData() {
    return window.arguments[0].QueryInterface(Ci.nsIAndroidView).initData;
  },

  init(aBrowser, aModules) {
    const MODULES_INIT_PROBE = new HistogramStopwatch(
      "GV_STARTUP_MODULES_MS",
      aBrowser
    );

    MODULES_INIT_PROBE.start();

    const initData = this._initData;
    this._browser = aBrowser;
    this._settings = initData.settings;
    this._frozenSettings = Object.freeze(Object.assign({}, this._settings));

    const self = this;
    this._modules = new Map(
      (function*() {
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
      })()
    );

    window.document.documentElement.appendChild(aBrowser);

    WindowEventDispatcher.registerListener(this, [
      "GeckoView:UpdateModuleState",
      "GeckoView:UpdateInitData",
      "GeckoView:UpdateSettings",
    ]);

    this.messageManager.addMessageListener(
      "GeckoView:ContentModuleLoaded",
      this
    );

    this.forEach(module => {
      module.onInit();
      module.loadInitFrameScript();
    });

    window.addEventListener("unload", () => {
      this.forEach(module => {
        module.enabled = false;
        module.onDestroy();
      });

      this._modules.clear();
    });

    MODULES_INIT_PROBE.finish();
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

  updateRemoteTypeForURI(aURI) {
    const currentType = this.browser.remoteType || E10SUtils.NOT_REMOTE;
    const remoteType = E10SUtils.getRemoteTypeForURI(
      aURI,
      this.settings.useMultiprocess,
      /* useRemoteSubframes */ false,
      currentType,
      this.browser.currentURI
    );

    debug`updateRemoteType: uri=${aURI} currentType=${currentType}
                             remoteType=${remoteType}`;

    if (currentType === remoteType) {
      // We're already using a child process of the correct type.
      return false;
    }

    if (remoteType !== E10SUtils.NOT_REMOTE && !this.settings.useMultiprocess) {
      warn`Tried to create a remote browser in non-multiprocess mode`;
      return false;
    }

    // Now we're switching the remoteness (value of "remote" attr).

    let disabledModules = [];
    this.forEach(module => {
      if (module.enabled) {
        module.enabled = false;
        disabledModules.push(module);
      }
    });

    this.forEach(module => {
      module.onDestroyBrowser();
    });

    const parent = this.browser.parentNode;
    this.browser.remove();
    if (remoteType) {
      this.browser.setAttribute("remote", "true");
      this.browser.setAttribute("remoteType", remoteType);
    } else {
      this.browser.setAttribute("remote", "false");
      this.browser.removeAttribute("remoteType");
    }

    this.forEach(module => {
      if (module.impl) {
        module.impl.onInitBrowser();
      }
    });

    parent.appendChild(this.browser);

    this.messageManager.addMessageListener(
      "GeckoView:ContentModuleLoaded",
      this
    );

    this.forEach(module => {
      // We're attaching a new browser so we have to reload the frame scripts
      module.loadInitFrameScript();
    });

    disabledModules.forEach(module => {
      module.enabled = true;
    });

    this.browser.focus();
    return true;
  },

  _updateSettings(aSettings) {
    Object.assign(this._settings, aSettings);
    this._frozenSettings = Object.freeze(Object.assign({}, this._settings));

    this.forEach(module => {
      if (module.impl) {
        module.impl.onSettingsUpdate();
      }
    });

    this._browser.messageManager.sendAsyncMessage(
      "GeckoView:UpdateSettings",
      aSettings
    );
  },

  onEvent(aEvent, aData, aCallback) {
    debug`onEvent ${aEvent} ${aData}`;
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

        // Notify child of the transfer.
        this._browser.messageManager.sendAsyncMessage(aEvent);
        break;
      }

      case "GeckoView:UpdateSettings": {
        this._updateSettings(aData);
        break;
      }
    }
  },

  receiveMessage(aMsg) {
    debug`receiveMessage ${aMsg.name} ${aMsg.data}`;
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
  constructor({ manager, name, enabled, onInit, onEnable }) {
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
    this._loadResource(onInit);

    this._onInitPhase = onInit;
    this._onEnablePhase = onEnable;
  }

  onInit() {
    if (this._impl) {
      this._impl.onInit();
      this._impl.onSettingsUpdate();
    }

    this.enabled = this._enabledOnInit;
  }

  /**
   * Loads the onInit frame script
   */
  loadInitFrameScript() {
    this._loadFrameScript(this._onInitPhase);
  }

  onDestroy() {
    if (this._impl) {
      this._impl.onDestroy();
    }
  }

  /**
   * Called before the browser is removed
   */
  onDestroyBrowser() {
    if (this._impl) {
      this._impl.onDestroyBrowser();
    }
    this._contentModuleLoaded = false;
  }

  /**
   * Load resource according to a phase object that contains possible keys,
   *
   * "resource": specify the JSM resource to load for this module.
   * "frameScript": specify a content JS frame script to load for this module.
   */
  _loadResource(aPhase) {
    if (!aPhase || !aPhase.resource || this._impl) {
      return;
    }

    const exports = ChromeUtils.import(aPhase.resource);
    this._impl = new exports[this._name](this);
  }

  /**
   * Load frameScript according to a phase object that contains possible keys,
   *
   * "frameScript": specify a content JS frame script to load for this module.
   */
  _loadFrameScript(aPhase) {
    if (!aPhase || !aPhase.frameScript || this._contentModuleLoaded) {
      return;
    }

    if (this._impl) {
      this._impl.onLoadContentModule();
    }
    this._manager.messageManager.loadFrameScript(aPhase.frameScript, true);
    this._contentModuleLoaded = true;
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
      this._loadResource(this._onEnablePhase);
      this._loadFrameScript(this._onEnablePhase);
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
    this._manager.messageManager.sendAsyncMessage(
      "GeckoView:UpdateModuleState",
      {
        module: this._name,
        enabled: this.enabled,
        settings: aIncludeSettings ? this._manager.settings : null,
      }
    );
  }
}

function createBrowser() {
  const browser = (window.browser = document.createXULElement("browser"));
  // Identify this `<browser>` element uniquely to Marionette, devtools, etc.
  browser.permanentKey = {};

  browser.setAttribute("nodefaultsrc", "true");
  browser.setAttribute("type", "content");
  browser.setAttribute("primary", "true");
  browser.setAttribute("flex", "1");

  const settings = window.arguments[0].QueryInterface(Ci.nsIAndroidView)
    .initData.settings;
  if (settings.useMultiprocess) {
    if (
      Services.prefs.getBoolPref(
        "dom.w3c_pointer_events.multiprocess.android.enabled"
      )
    ) {
      Services.prefs.setBoolPref("dom.w3c_pointer_events.enabled", true);
    }
    browser.setAttribute("remote", "true");
    browser.setAttribute("remoteType", E10SUtils.DEFAULT_REMOTE_TYPE);
  }

  return browser;
}

function InitLater(fn, object, name) {
  return DelayedInit.schedule(fn, object, name, 15000 /* 15s max wait */);
}

function startup() {
  GeckoViewUtils.initLogging("XUL", window);

  const browser = createBrowser();
  ModuleManager.init(browser, [
    {
      name: "GeckoViewAccessibility",
      onInit: {
        resource: "resource://gre/modules/GeckoViewAccessibility.jsm",
      },
    },
    {
      name: "GeckoViewContent",
      onInit: {
        resource: "resource://gre/modules/GeckoViewContent.jsm",
        frameScript: "chrome://geckoview/content/GeckoViewContentChild.js",
      },
    },
    {
      name: "GeckoViewMedia",
      onEnable: {
        resource: "resource://gre/modules/GeckoViewMedia.jsm",
        frameScript: "chrome://geckoview/content/GeckoViewMediaChild.js",
      },
    },
    {
      name: "GeckoViewNavigation",
      onInit: {
        resource: "resource://gre/modules/GeckoViewNavigation.jsm",
        frameScript: "chrome://geckoview/content/GeckoViewNavigationChild.js",
      },
    },
    {
      name: "GeckoViewProcessHangMonitor",
      onInit: {
        resource: "resource://gre/modules/GeckoViewProcessHangMonitor.jsm",
      },
    },
    {
      name: "GeckoViewProgress",
      onEnable: {
        resource: "resource://gre/modules/GeckoViewProgress.jsm",
        frameScript: "chrome://geckoview/content/GeckoViewProgressChild.js",
      },
    },
    {
      name: "GeckoViewScroll",
      onEnable: {
        frameScript: "chrome://geckoview/content/GeckoViewScrollChild.js",
      },
    },
    {
      name: "GeckoViewSelectionAction",
      onEnable: {
        frameScript:
          "chrome://geckoview/content/GeckoViewSelectionActionChild.js",
      },
    },
    {
      name: "GeckoViewSettings",
      onInit: {
        resource: "resource://gre/modules/GeckoViewSettings.jsm",
        frameScript: "chrome://geckoview/content/GeckoViewSettingsChild.js",
      },
    },
    {
      name: "GeckoViewTab",
      onInit: {
        resource: "resource://gre/modules/GeckoViewTab.jsm",
      },
    },
    {
      name: "GeckoViewContentBlocking",
      onEnable: {
        resource: "resource://gre/modules/GeckoViewContentBlocking.jsm",
        frameScript:
          "chrome://geckoview/content/GeckoViewContentBlockingChild.js",
      },
    },
    {
      name: "SessionStateAggregator",
      onInit: {
        frameScript: "chrome://geckoview/content/SessionStateAggregator.js",
      },
    },
  ]);

  Services.tm.dispatchToMainThread(() => {
    // This should always be the first thing we do here - any additional delayed
    // initialisation tasks should be added between "browser-delayed-startup-finished"
    // and "browser-idle-startup-tasks-finished".

    // Bug 1496684: Various bits of platform stuff depend on this notification
    // to learn when a browser window has finished its initial (chrome)
    // initialisation, especially with regards to the very first window that is
    // created. Therefore, GeckoView "windows" need to send this, too.
    InitLater(() =>
      Services.obs.notifyObservers(window, "browser-delayed-startup-finished")
    );

    // This should always go last, since the idle tasks (except for the ones with
    // timeouts) should execute in order. Note that this observer notification is
    // not guaranteed to fire, since the window could close before we get here.

    // This notification in particular signals the ScriptPreloader that we have
    // finished startup, so it can now stop recording script usage and start
    // updating the startup cache for faster script loading.
    InitLater(() =>
      Services.obs.notifyObservers(
        window,
        "browser-idle-startup-tasks-finished"
      )
    );
  });

  // Move focus to the content window at the end of startup,
  // so things like text selection can work properly.
  browser.focus();
}
