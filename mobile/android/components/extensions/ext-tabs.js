/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyServiceGetter(this, "aboutNewTabService",
                                   "@mozilla.org/browser/aboutnewtab-service;1",
                                   "nsIAboutNewTabService");

XPCOMUtils.defineLazyModuleGetter(this, "MatchPattern",
                                  "resource://gre/modules/MatchPattern.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PromiseUtils",
                                  "resource://gre/modules/PromiseUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  SingletonEventManager,
  ignoreEvent,
} = ExtensionUtils;

// This function is pretty tightly tied to Extension.jsm.
// Its job is to fill in the |tab| property of the sender.
function getSender(extension, target, sender) {
  let tabId;
  if ("tabId" in sender) {
    // The message came from a privileged extension page running in a tab. In
    // that case, it should include a tabId property (which is filled in by the
    // page-open listener below).
    tabId = sender.tabId;
    delete sender.tabId;
  } else if (target instanceof Ci.nsIDOMXULElement) {
    tabId = tabTracker.getBrowserData(target).tabId;
  }

  if (tabId) {
    let tab = extension.tabManager.get(tabId, null);
    if (tab) {
      sender.tab = tab.convert();
    }
  }
}

// Used by Extension.jsm
global.tabGetSender = getSender;

/* eslint-disable mozilla/balanced-listeners */
extensions.on("page-shutdown", (type, context) => {
  if (context.viewType == "tab") {
    if (context.extension.id !== context.xulBrowser.contentPrincipal.addonId) {
      // Only close extension tabs.
      // This check prevents about:addons from closing when it contains a
      // WebExtension as an embedded inline options page.
      return;
    }
    let {BrowserApp} = context.xulBrowser.ownerGlobal;
    if (BrowserApp) {
      let tab = BrowserApp.getTabForBrowser(context.xulBrowser);
      if (tab) {
        BrowserApp.closeTab(tab);
      }
    }
  }
});
/* eslint-enable mozilla/balanced-listeners */

function getBrowserWindow(window) {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDocShell)
               .QueryInterface(Ci.nsIDocShellTreeItem).rootTreeItem
               .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
}

let tabListener = {
  tabReadyInitialized: false,
  tabReadyPromises: new WeakMap(),
  initializingTabs: new WeakSet(),

  initTabReady() {
    if (!this.tabReadyInitialized) {
      windowTracker.addListener("progress", this);

      this.tabReadyInitialized = true;
    }
  },

  onLocationChange(browser, webProgress, request, locationURI, flags) {
    if (webProgress.isTopLevel) {
      let {BrowserApp} = browser.ownerGlobal;
      let tab = BrowserApp.getTabForBrowser(browser);

      // Now we are certain that the first page in the tab was loaded.
      this.initializingTabs.delete(tab);

      // browser.innerWindowID is now set, resolve the promises if any.
      let deferred = this.tabReadyPromises.get(tab);
      if (deferred) {
        deferred.resolve(tab);
        this.tabReadyPromises.delete(tab);
      }
    }
  },

  /**
   * Returns a promise that resolves when the tab is ready.
   * Tabs created via the `tabs.create` method are "ready" once the location
   * changes to the requested URL. Other tabs are assumed to be ready once their
   * inner window ID is known.
   *
   * @param {XULElement} tab The <tab> element.
   * @returns {Promise} Resolves with the given tab once ready.
   */
  awaitTabReady(tab) {
    let deferred = this.tabReadyPromises.get(tab);
    if (!deferred) {
      deferred = PromiseUtils.defer();
      if (!this.initializingTabs.has(tab) && (tab.browser.innerWindowID ||
                                              tab.browser.currentURI.spec === "about:blank")) {
        deferred.resolve(tab);
      } else {
        this.initTabReady();
        this.tabReadyPromises.set(tab, deferred);
      }
    }
    return deferred.promise;
  },
};

extensions.registerSchemaAPI("tabs", "addon_parent", context => {
  let {extension} = context;

  let {tabManager} = extension;

  function getTabOrActive(tabId) {
    if (tabId !== null) {
      return tabTracker.getTab(tabId);
    }
    return tabTracker.activeTab;
  }

  async function promiseTabWhenReady(tabId) {
    let tab;
    if (tabId !== null) {
      tab = tabManager.get(tabId);
    } else {
      tab = tabManager.getWrapper(tabTracker.activeTab);
    }

    await tabListener.awaitTabReady(tab.tab);

    return tab;
  }

  let self = {
    tabs: {
      onActivated: new GlobalEventManager(context, "tabs.onActivated", "Tab:Selected", (fire, data) => {
        let tab = tabManager.get(data.id);

        fire.async({tabId: tab.id, windowId: tab.windowId});
      }).api(),

      onCreated: new SingletonEventManager(context, "tabs.onCreated", fire => {
        let listener = (eventName, event) => {
          fire.async(tabManager.convert(event.tab));
        };

        tabTracker.on("tab-created", listener);
        return () => {
          tabTracker.off("tab-created", listener);
        };
      }).api(),

      /**
       * Since multiple tabs currently can't be highlighted, onHighlighted
       * essentially acts an alias for self.tabs.onActivated but returns
       * the tabId in an array to match the API.
       * @see  https://developer.mozilla.org/en-US/Add-ons/WebExtensions/API/Tabs/onHighlighted
      */
      onHighlighted: new GlobalEventManager(context, "tabs.onHighlighted", "Tab:Selected", (fire, data) => {
        let tab = tabManager.get(data.id);

        fire.async({tabIds: [tab.id], windowId: tab.windowId});
      }).api(),

      onAttached: new SingletonEventManager(context, "tabs.onAttached", fire => {
        return () => {};
      }).api(),

      onDetached: new SingletonEventManager(context, "tabs.onDetached", fire => {
        return () => {};
      }).api(),

      onRemoved: new SingletonEventManager(context, "tabs.onRemoved", fire => {
        let listener = (eventName, event) => {
          fire.async(event.tabId, {windowId: event.windowId, isWindowClosing: event.isWindowClosing});
        };

        tabTracker.on("tab-removed", listener);
        return () => {
          tabTracker.off("tab-removed", listener);
        };
      }).api(),

      onReplaced: ignoreEvent(context, "tabs.onReplaced"),

      onMoved: new SingletonEventManager(context, "tabs.onMoved", fire => {
        return () => {};
      }).api(),

      onUpdated: new SingletonEventManager(context, "tabs.onUpdated", fire => {
        const restricted = ["url", "favIconUrl", "title"];

        function sanitize(extension, changeInfo) {
          let result = {};
          let nonempty = false;
          for (let prop in changeInfo) {
            if (extension.hasPermission("tabs") || !restricted.includes(prop)) {
              nonempty = true;
              result[prop] = changeInfo[prop];
            }
          }
          return [nonempty, result];
        }

        let fireForTab = (tab, changed) => {
          let [needed, changeInfo] = sanitize(extension, changed);
          if (needed) {
            fire.async(tab.id, changeInfo, tab.convert());
          }
        };

        let listener = event => {
          let needed = [];
          let tab;
          switch (event.type) {
            case "DOMTitleChanged": {
              let {BrowserApp} = getBrowserWindow(event.target.ownerGlobal);

              tab = BrowserApp.getTabForWindow(event.target.ownerGlobal);
              needed.push("title");
              break;
            }

            case "DOMAudioPlaybackStarted":
            case "DOMAudioPlaybackStopped": {
              let {BrowserApp} = event.target.ownerGlobal;
              tab = BrowserApp.getTabForBrowser(event.originalTarget);
              needed.push("audible");
              break;
            }
          }

          if (!tab) {
            return;
          }

          tab = tabManager.getWrapper(tab);
          let changeInfo = {};
          for (let prop of needed) {
            changeInfo[prop] = tab[prop];
          }

          fireForTab(tab, changeInfo);
        };

        let statusListener = ({browser, status, url}) => {
          let {BrowserApp} = browser.ownerGlobal;
          let tab = BrowserApp.getTabForBrowser(browser);
          if (tab) {
            let changed = {status};
            if (url) {
              changed.url = url;
            }

            fireForTab(tabManager.wrapTab(tab), changed);
          }
        };

        windowTracker.addListener("status", statusListener);
        windowTracker.addListener("DOMTitleChanged", listener);
        return () => {
          windowTracker.removeListener("status", statusListener);
          windowTracker.removeListener("DOMTitleChanged", listener);
        };
      }).api(),

      async create(createProperties) {
        let window = createProperties.windowId !== null ?
          windowTracker.getWindow(createProperties.windowId, context) :
          windowTracker.topWindow;

        let {BrowserApp} = window;
        let url;

        if (createProperties.url !== null) {
          url = context.uri.resolve(createProperties.url);

          if (!context.checkLoadURL(url, {dontReportErrors: true})) {
            return Promise.reject({message: `Illegal URL: ${url}`});
          }
        }

        let options = {};

        let active = true;
        if (createProperties.active !== null) {
          active = createProperties.active;
        }
        options.selected = active;

        if (createProperties.index !== null) {
          options.tabIndex = createProperties.index;
        }

        // Make sure things like about:blank and data: URIs never inherit,
        // and instead always get a NullPrincipal.
        options.disallowInheritPrincipal = true;

        tabListener.initTabReady();
        let tab = BrowserApp.addTab(url, options);

        if (createProperties.url) {
          tabListener.initializingTabs.add(tab);
        }

        return tabManager.convert(tab);
      },

      async remove(tabs) {
        if (!Array.isArray(tabs)) {
          tabs = [tabs];
        }

        for (let tabId of tabs) {
          let tab = tabTracker.getTab(tabId);
          tab.browser.ownerGlobal.BrowserApp.closeTab(tab);
        }
      },

      async update(tabId, updateProperties) {
        let tab = getTabOrActive(tabId);

        let {BrowserApp} = tab.browser.ownerGlobal;

        if (updateProperties.url !== null) {
          let url = context.uri.resolve(updateProperties.url);

          if (!context.checkLoadURL(url, {dontReportErrors: true})) {
            return Promise.reject({message: `Illegal URL: ${url}`});
          }

          tab.browser.loadURI(url);
        }

        if (updateProperties.active !== null) {
          if (updateProperties.active) {
            BrowserApp.selectTab(tab);
          } else {
            // Not sure what to do here? Which tab should we select?
          }
        }
        // FIXME: highlighted/selected, muted, pinned, openerTabId

        return tabManager.convert(tab);
      },

      async reload(tabId, reloadProperties) {
        let tab = getTabOrActive(tabId);

        let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
        if (reloadProperties && reloadProperties.bypassCache) {
          flags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
        }
        tab.browser.reloadWithFlags(flags);
      },

      async get(tabId) {
        return tabManager.get(tabId).convert();
      },

      async getCurrent() {
        if (context.tabId) {
          return tabManager.get(context.tabId).convert();
        }
      },

      async query(queryInfo) {
        if (queryInfo.url !== null) {
          if (!extension.hasPermission("tabs")) {
            return Promise.reject({message: 'The "tabs" permission is required to use the query API with the "url" parameter'});
          }

          queryInfo = Object.assign({}, queryInfo);
          queryInfo.url = new MatchPattern(queryInfo.url);
        }

        return Array.from(tabManager.query(queryInfo, context),
                          tab => tab.convert());
      },

      captureVisibleTab(windowId, options) {
        if (!extension.hasPermission("<all_urls>")) {
          return Promise.reject({message: "The <all_urls> permission is required to use the captureVisibleTab API"});
        }

        let window = windowId == null ?
          windowTracker.topWindow :
          windowTracker.getWindow(windowId, context);

        let tab = window.BrowserApp.selectedTab;
        return tabListener.awaitTabReady(tab).then(() => {
          let {browser} = tab;
          let recipient = {
            innerWindowID: browser.innerWindowID,
          };

          if (!options) {
            options = {};
          }
          if (options.format == null) {
            options.format = "png";
          }
          if (options.quality == null) {
            options.quality = 92;
          }

          let message = {
            options,
            width: browser.clientWidth,
            height: browser.clientHeight,
          };

          return context.sendMessage(browser.messageManager, "Extension:Capture",
                                     message, {recipient});
        });
      },

      async executeScript(tabId, details) {
        let tab = await promiseTabWhenReady(tabId);

        return tab.executeScript(context, details);
      },

      async insertCSS(tabId, details) {
        let tab = await promiseTabWhenReady(tabId);

        return tab.insertCSS(context, details);
      },

      async removeCSS(tabId, details) {
        let tab = await promiseTabWhenReady(tabId);

        return tab.removeCSS(context, details);
      },
    },
  };
  return self;
});
