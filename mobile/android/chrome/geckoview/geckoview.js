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
  GeckoViewActorManager: "resource://gre/modules/GeckoViewActorManager.jsm",
  GeckoViewSettings: "resource://gre/modules/GeckoViewSettings.jsm",
  GeckoViewUtils: "resource://gre/modules/GeckoViewUtils.jsm",
  HistogramStopwatch: "resource://gre/modules/GeckoViewTelemetry.jsm",
  SafeBrowsing: "resource://gre/modules/SafeBrowsing.jsm",
  RemoteSecuritySettings:
    "resource://gre/modules/psm/RemoteSecuritySettings.jsm",
});

XPCOMUtils.defineLazyGetter(this, "WindowEventDispatcher", () =>
  EventDispatcher.for(window)
);

// This file assumes `warn` and `debug` are imported into scope
// by the child scripts.
/* global debug, warn */

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

    // By default all layers are discarded when a browser is set to inactive.
    // GeckoView by default sets browsers to inactive every time they're not
    // visible. To avoid flickering when changing tabs, we preserve layers for
    // all loaded tabs.
    aBrowser.preserveLayers(true);

    WindowEventDispatcher.registerListener(this, [
      "GeckoView:UpdateModuleState",
      "GeckoView:UpdateInitData",
      "GeckoView:UpdateSettings",
    ]);

    this.messageManager.addMessageListener(
      "GeckoView:ContentModuleLoaded",
      this
    );

    this._moduleByActorName = new Map();
    this.forEach(module => {
      module.onInit();
      module.loadInitFrameScript();
      for (const actorName of module.actorNames) {
        this._moduleByActorName[actorName] = module;
      }
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

  getActor(aActorName) {
    return this.browser.browsingContext.currentWindowGlobal?.getActor(
      aActorName
    );
  },

  // Ensures that session history has been flushed before changing remoteness
  async prepareToChangeRemoteness() {
    // Session state like history is maintained at the process level so we need
    // to collect it and restore it in the other process when switching.
    // TODO: This should go away when we migrate the history to the main
    // process Bug 1507287.
    const { history } = await this.getActor("GeckoViewContent").collectState();

    // Ignore scroll and form data since we're navigating away from this page
    // anyway
    this.sessionState = { history };
  },

  willChangeBrowserRemoteness() {
    debug`WillChangeBrowserRemoteness`;

    // Now we're switching the remoteness.
    this.disabledModules = [];
    this.forEach(module => {
      if (module.enabled && module.disableOnProcessSwitch) {
        module.enabled = false;
        this.disabledModules.push(module);
      }
    });

    this.forEach(module => {
      module.onDestroyBrowser();
    });
  },

  didChangeBrowserRemoteness() {
    debug`DidChangeBrowserRemoteness`;

    this.forEach(module => {
      if (module.impl) {
        module.impl.onInitBrowser();
      }
    });

    this.messageManager.addMessageListener(
      "GeckoView:ContentModuleLoaded",
      this
    );

    this.forEach(module => {
      // We're attaching a new browser so we have to reload the frame scripts
      module.loadInitFrameScript();
    });

    this.disabledModules.forEach(module => {
      module.enabled = true;
    });
    this.disabledModules = null;
  },

  afterBrowserRemotenessChange(aSwitchId) {
    const { sessionState } = this;
    this.sessionState = null;

    sessionState.switchId = aSwitchId;

    this.getActor("GeckoViewContent").restoreState(sessionState);
    this.browser.focus();

    // Load was handled
    return true;
  },

  _updateSettings(aSettings) {
    Object.assign(this._settings, aSettings);
    this._frozenSettings = Object.freeze(Object.assign({}, this._settings));

    const windowType = aSettings.isPopup
      ? "navigator:popup"
      : "navigator:geckoview";
    window.document.documentElement.setAttribute("windowtype", windowType);

    this.forEach(module => {
      if (module.impl) {
        module.impl.onSettingsUpdate();
      }
    });
  },

  onMessageFromActor(aActorName, aMessage) {
    this._moduleByActorName[aActorName].receiveMessage(aMessage);
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

    // We don't support having more than one main process script, so let's
    // check that we're not accidentally defining two. We could support this if
    // needed by making _impl an array for each phase impl.
    if (onInit?.resource !== undefined && onEnable?.resource !== undefined) {
      throw new Error(
        "Only one main process script is allowed for each module."
      );
    }

    this._impl = null;
    this._contentModuleLoaded = false;
    this._enabled = false;
    // Only enable once we performed initialization.
    this._enabledOnInit = enabled;

    // For init, load resource _before_ initializing browser to support the
    // onInitBrowser() override. However, load content module after initializing
    // browser, because we don't have a message manager before then.
    this._loadResource(onInit);
    this._loadActors(onInit);
    if (this._enabledOnInit) {
      this._loadActors(onEnable);
    }

    this._onInitPhase = onInit;
    this._onEnablePhase = onEnable;

    const actorNames = [];
    if (this._onInitPhase?.actors) {
      actorNames.push(Object.keys(this._onInitPhase.actors));
    }
    if (this._onEnablePhase?.actors) {
      actorNames.push(Object.keys(this._onEnablePhase.actors));
    }
    this._actorNames = Object.freeze(actorNames);
  }

  get actorNames() {
    return this._actorNames;
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
    this._contentModuleLoaded = false;
  }

  _loadActors(aPhase) {
    if (!aPhase || !aPhase.actors) {
      return;
    }

    GeckoViewActorManager.addJSWindowActors(aPhase.actors);
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

  get disableOnProcessSwitch() {
    // Only disable while process switching if it has a frameScript
    return (
      !!this._onInitPhase?.frameScript || !!this._onEnablePhase?.frameScript
    );
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
      this._loadActors(this._onEnablePhase);
      if (this._impl) {
        this._impl.onEnable();
        this._impl.onSettingsUpdate();
      }
    }

    this._updateContentModuleState();
  }

  receiveMessage(aMessage) {
    if (!this._impl) {
      throw new Error(`No impl for message: ${aMessage.name}.`);
    }

    this._impl.receiveMessage(aMessage);
  }

  onContentModuleLoaded() {
    this._updateContentModuleState();

    if (this._impl) {
      this._impl.onContentModuleLoaded();
    }
  }

  _updateContentModuleState() {
    this._manager.messageManager.sendAsyncMessage(
      "GeckoView:UpdateModuleState",
      {
        module: this._name,
        enabled: this.enabled,
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
  browser.setAttribute("maychangeremoteness", "true");
  browser.setAttribute("remote", "true");
  browser.setAttribute("remoteType", E10SUtils.DEFAULT_REMOTE_TYPE);

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
      name: "ExtensionContent",
      onInit: {
        frameScript: "chrome://geckoview/content/extension-content.js",
      },
    },
    {
      name: "GeckoViewContent",
      onInit: {
        resource: "resource://gre/modules/GeckoViewContent.jsm",
        actors: {
          GeckoViewContent: {
            parent: {
              moduleURI: "resource:///actors/GeckoViewContentParent.jsm",
            },
            child: {
              moduleURI: "resource:///actors/GeckoViewContentChild.jsm",
              events: {
                mozcaretstatechanged: { capture: true, mozSystemGroup: true },
                pageshow: { mozSystemGroup: true },
              },
            },
            allFrames: true,
          },
        },
      },
      onEnable: {
        actors: {
          ContentDelegate: {
            parent: {
              moduleURI: "resource:///actors/ContentDelegateParent.jsm",
            },
            child: {
              moduleURI: "resource:///actors/ContentDelegateChild.jsm",
              events: {
                DOMContentLoaded: {},
                DOMMetaViewportFitChanged: {},
                "MozDOMFullscreen:Entered": {},
                "MozDOMFullscreen:Exit": {},
                "MozDOMFullscreen:Exited": {},
                "MozDOMFullscreen:Request": {},
                MozFirstContentfulPaint: {},
                MozPaintStatusReset: {},
                contextmenu: { capture: true },
              },
            },
            allFrames: true,
          },
        },
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
        actors: {
          ProgressDelegate: {
            parent: {
              moduleURI: "resource:///actors/ProgressDelegateParent.jsm",
            },
            child: {
              moduleURI: "resource:///actors/ProgressDelegateChild.jsm",
              events: {
                MozAfterPaint: { capture: false, mozSystemGroup: true },
                DOMContentLoaded: { capture: false, mozSystemGroup: true },
                pageshow: { capture: false, mozSystemGroup: true },
              },
            },
          },
        },
      },
    },
    {
      name: "GeckoViewScroll",
      onEnable: {
        actors: {
          ScrollDelegate: {
            child: {
              moduleURI: "resource:///actors/ScrollDelegateChild.jsm",
              events: {
                mozvisualscroll: { mozSystemGroup: true },
              },
            },
          },
        },
      },
    },
    {
      name: "GeckoViewSelectionAction",
      onEnable: {
        actors: {
          SelectionActionDelegate: {
            child: {
              moduleURI: "resource:///actors/SelectionActionDelegateChild.jsm",
              events: {
                mozcaretstatechanged: { mozSystemGroup: true },
                pagehide: { capture: true, mozSystemGroup: true },
                deactivate: { mozSystemGroup: true },
              },
            },
            allFrames: true,
          },
        },
      },
    },
    {
      name: "GeckoViewSettings",
      onInit: {
        resource: "resource://gre/modules/GeckoViewSettings.jsm",
        actors: {
          GeckoViewSettings: {
            child: {
              moduleURI: "resource:///actors/GeckoViewSettingsChild.jsm",
            },
          },
        },
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
      onInit: {
        resource: "resource://gre/modules/GeckoViewContentBlocking.jsm",
      },
    },
    {
      name: "SessionStateAggregator",
      onInit: {
        frameScript: "chrome://geckoview/content/SessionStateAggregator.js",
      },
    },
    {
      name: "GeckoViewAutofill",
      onInit: {
        frameScript: "chrome://geckoview/content/GeckoViewAutofillChild.js",
      },
    },
    {
      name: "GeckoViewMediaControl",
      onEnable: {
        resource: "resource://gre/modules/GeckoViewMediaControl.jsm",
        frameScript: "chrome://geckoview/content/GeckoViewMediaControlChild.js",
      },
    },
    {
      name: "GeckoViewAutocomplete",
      onInit: {
        actors: {
          FormAutofill: {
            parent: {
              moduleURI: "resource://autofill/FormAutofillParent.jsm",
            },
            child: {
              moduleURI: "resource://autofill/FormAutofillChild.jsm",
              events: {
                focusin: {},
                DOMFormBeforeSubmit: {},
              },
            },
            allFrames: true,
          },
        },
      },
    },
  ]);

  if (!Services.appinfo.sessionHistoryInParent) {
    browser.prepareToChangeRemoteness = () =>
      ModuleManager.prepareToChangeRemoteness();
    browser.afterChangeRemoteness = switchId =>
      ModuleManager.afterBrowserRemotenessChange(switchId);
  }

  browser.addEventListener("WillChangeBrowserRemoteness", event =>
    ModuleManager.willChangeBrowserRemoteness()
  );

  browser.addEventListener("DidChangeBrowserRemoteness", event =>
    ModuleManager.didChangeBrowserRemoteness()
  );

  // Allows actors to access ModuleManager.
  window.moduleManager = ModuleManager;

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

    // Let the extension code know it can start loading things that were delayed
    // while GeckoView started up.
    InitLater(() => {
      Services.obs.notifyObservers(window, "extensions-late-startup");
    });

    InitLater(() => {
      RemoteSecuritySettings.init();
    });

    InitLater(() => {
      // Initialize safe browsing module. This is required for content
      // blocking features and manages blocklist downloads and updates.
      SafeBrowsing.init();
    });

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

    InitLater(() => {
      // This lets Marionette and the Remote Agent (used for our CDP and the
      // upcoming WebDriver BiDi implementation) start listening (when enabled).
      // Both GeckoView and these two remote protocols do most of their
      // initialization in "profile-after-change", and there is no order enforced
      // between them.  Therefore we defer asking both components to startup
      // until after all "profile-after-change" handlers (including this one)
      // have completed.
      Services.obs.notifyObservers(null, "marionette-startup-requested");
      Services.obs.notifyObservers(null, "remote-startup-requested");
    });
  });

  // Move focus to the content window at the end of startup,
  // so things like text selection can work properly.
  browser.focus();
}
