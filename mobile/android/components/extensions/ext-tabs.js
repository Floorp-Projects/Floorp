/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(this, "PromiseUtils",
                               "resource://gre/modules/PromiseUtils.jsm");

const getBrowserWindow = window => {
  return window.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDocShell)
               .QueryInterface(Ci.nsIDocShellTreeItem).rootTreeItem
               .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
};

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
      let nativeTab = BrowserApp.getTabForBrowser(browser);

      // Now we are certain that the first page in the tab was loaded.
      this.initializingTabs.delete(nativeTab);

      // browser.innerWindowID is now set, resolve the promises if any.
      let deferred = this.tabReadyPromises.get(nativeTab);
      if (deferred) {
        deferred.resolve(nativeTab);
        this.tabReadyPromises.delete(nativeTab);
      }
    }
  },

  /**
   * Returns a promise that resolves when the tab is ready.
   * Tabs created via the `tabs.create` method are "ready" once the location
   * changes to the requested URL. Other tabs are assumed to be ready once their
   * inner window ID is known.
   *
   * @param {NativeTab} nativeTab The native tab object.
   * @returns {Promise} Resolves with the given tab once ready.
   */
  awaitTabReady(nativeTab) {
    let deferred = this.tabReadyPromises.get(nativeTab);
    if (!deferred) {
      deferred = PromiseUtils.defer();
      if (!this.initializingTabs.has(nativeTab) &&
          (nativeTab.browser.innerWindowID ||
           nativeTab.browser.currentURI.spec === "about:blank")) {
        deferred.resolve(nativeTab);
      } else {
        this.initTabReady();
        this.tabReadyPromises.set(nativeTab, deferred);
      }
    }
    return deferred.promise;
  },
};

this.tabs = class extends ExtensionAPI {
  getAPI(context) {
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

      await tabListener.awaitTabReady(tab.nativeTab);

      return tab;
    }

    let self = {
      tabs: {
        onActivated: makeGlobalEvent(context, "tabs.onActivated", "Tab:Selected", (fire, data) => {
          let tab = tabManager.get(data.id);

          fire.async({tabId: tab.id, windowId: tab.windowId});
        }),

        onCreated: new EventManager({
          context,
          name: "tabs.onCreated",
          register: fire => {
            let listener = (eventName, event) => {
              fire.async(tabManager.convert(event.nativeTab));
            };

            tabTracker.on("tab-created", listener);
            return () => {
              tabTracker.off("tab-created", listener);
            };
          },
        }).api(),

        /**
         * Since multiple tabs currently can't be highlighted, onHighlighted
         * essentially acts an alias for self.tabs.onActivated but returns
         * the tabId in an array to match the API.
         * @see  https://developer.mozilla.org/en-US/Add-ons/WebExtensions/API/Tabs/onHighlighted
        */
        onHighlighted: makeGlobalEvent(context, "tabs.onHighlighted", "Tab:Selected", (fire, data) => {
          let tab = tabManager.get(data.id);

          fire.async({tabIds: [tab.id], windowId: tab.windowId});
        }),

        onAttached: new EventManager({
          context,
          name: "tabs.onAttached",
          register: fire => { return () => {}; },
        }).api(),

        onDetached: new EventManager({
          context,
          name: "tabs.onDetached",
          register: fire => { return () => {}; },
        }).api(),

        onRemoved: new EventManager({
          context,
          name: "tabs.onRemoved",
          register: fire => {
            let listener = (eventName, event) => {
              fire.async(event.tabId, {windowId: event.windowId, isWindowClosing: event.isWindowClosing});
            };

            tabTracker.on("tab-removed", listener);
            return () => {
              tabTracker.off("tab-removed", listener);
            };
          },
        }).api(),

        onReplaced: new EventManager({
          context,
          name: "tabs.onReplaced",
          register: fire => { return () => {}; },
        }).api(),

        onMoved: new EventManager({
          context,
          name: "tabs.onMoved",
          register: fire => { return () => {}; },
        }).api(),

        onUpdated: new EventManager({
          context,
          name: "tabs.onUpdated",
          register: fire => {
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
              let nativeTab;
              switch (event.type) {
                case "DOMTitleChanged": {
                  let {BrowserApp} = getBrowserWindow(event.target.ownerGlobal);

                  nativeTab = BrowserApp.getTabForWindow(event.target.ownerGlobal);
                  needed.push("title");
                  break;
                }

                case "DOMAudioPlaybackStarted":
                case "DOMAudioPlaybackStopped": {
                  let {BrowserApp} = event.target.ownerGlobal;
                  nativeTab = BrowserApp.getTabForBrowser(event.originalTarget);
                  needed.push("audible");
                  break;
                }
              }

              if (!nativeTab) {
                return;
              }

              let tab = tabManager.getWrapper(nativeTab);
              let changeInfo = {};
              for (let prop of needed) {
                changeInfo[prop] = tab[prop];
              }

              fireForTab(tab, changeInfo);
            };

            let statusListener = ({browser, status, url}) => {
              let {BrowserApp} = browser.ownerGlobal;
              let nativeTab = BrowserApp.getTabForBrowser(browser);
              if (nativeTab) {
                let changed = {status};
                if (url) {
                  changed.url = url;
                }

                fireForTab(tabManager.wrapTab(nativeTab), changed);
              }
            };

            windowTracker.addListener("status", statusListener);
            windowTracker.addListener("DOMTitleChanged", listener);
            return () => {
              windowTracker.removeListener("status", statusListener);
              windowTracker.removeListener("DOMTitleChanged", listener);
            };
          },
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

          options.parentId = BrowserApp.selectedTab.id;

          tabListener.initTabReady();
          let nativeTab = BrowserApp.addTab(url, options);

          if (createProperties.url) {
            tabListener.initializingTabs.add(nativeTab);
          }

          return tabManager.convert(nativeTab);
        },

        async remove(tabs) {
          if (!Array.isArray(tabs)) {
            tabs = [tabs];
          }

          for (let tabId of tabs) {
            let nativeTab = tabTracker.getTab(tabId);
            nativeTab.browser.ownerGlobal.BrowserApp.closeTab(nativeTab);
          }
        },

        async update(tabId, updateProperties) {
          let nativeTab = getTabOrActive(tabId);

          let {BrowserApp} = nativeTab.browser.ownerGlobal;

          if (updateProperties.url !== null) {
            let url = context.uri.resolve(updateProperties.url);

            if (!context.checkLoadURL(url, {dontReportErrors: true})) {
              return Promise.reject({message: `Illegal URL: ${url}`});
            }

            nativeTab.browser.loadURI(url);
          }

          if (updateProperties.active !== null) {
            if (updateProperties.active) {
              BrowserApp.selectTab(nativeTab);
            } else {
              // Not sure what to do here? Which tab should we select?
            }
          }
          // FIXME: highlighted/selected, muted, pinned, openerTabId

          return tabManager.convert(nativeTab);
        },

        async reload(tabId, reloadProperties) {
          let nativeTab = getTabOrActive(tabId);

          let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;
          if (reloadProperties && reloadProperties.bypassCache) {
            flags |= Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE;
          }
          nativeTab.browser.reloadWithFlags(flags);
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
          if (!extension.hasPermission("tabs")) {
            if (queryInfo.url !== null || queryInfo.title !== null) {
              return Promise.reject({message: 'The "tabs" permission is required to use the query API with the "url" or "title" parameters'});
            }
          }

          queryInfo = Object.assign({}, queryInfo);

          if (queryInfo.url !== null) {
            queryInfo.url = new MatchPatternSet([].concat(queryInfo.url));
          }
          if (queryInfo.title !== null) {
            queryInfo.title = new MatchGlob(queryInfo.title);
          }

          return Array.from(tabManager.query(queryInfo, context),
                            tab => tab.convert());
        },

        async captureVisibleTab(windowId, options) {
          let window = windowId == null ?
            windowTracker.topWindow :
            windowTracker.getWindow(windowId, context);

          let tab = tabManager.wrapTab(window.BrowserApp.selectedTab);
          await tabListener.awaitTabReady(tab.nativeTab);

          return tab.capture(context, options);
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
  }
};
