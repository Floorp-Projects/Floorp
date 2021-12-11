/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineModuleGetter(
  this,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "GeckoViewTabBridge",
  "resource://gre/modules/GeckoViewTab.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "mobileWindowTracker",
  "resource://gre/modules/GeckoViewWebExtension.jsm"
);

const getBrowserWindow = window => {
  return window.browsingContext.topChromeWindow;
};

const tabListener = {
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
      const { tab } = browser.ownerGlobal;

      // Ignore initial about:blank
      if (!request && this.initializingTabs.has(tab)) {
        return;
      }

      // Now we are certain that the first page in the tab was loaded.
      this.initializingTabs.delete(tab);

      // browser.innerWindowID is now set, resolve the promises if any.
      const deferred = this.tabReadyPromises.get(tab);
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
   * @param {NativeTab} nativeTab The native tab object.
   * @returns {Promise} Resolves with the given tab once ready.
   */
  awaitTabReady(nativeTab) {
    let deferred = this.tabReadyPromises.get(nativeTab);
    if (!deferred) {
      deferred = PromiseUtils.defer();
      if (
        !this.initializingTabs.has(nativeTab) &&
        (nativeTab.browser.innerWindowID ||
          nativeTab.browser.currentURI.spec === "about:blank")
      ) {
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
    const { extension } = context;

    const { tabManager } = extension;

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
      if (!tab) {
        throw new ExtensionError(
          tabId == null ? "Cannot access activeTab" : `Invalid tab ID: ${tabId}`
        );
      }

      await tabListener.awaitTabReady(tab.nativeTab);

      return tab;
    }

    const self = {
      tabs: {
        onActivated: new EventManager({
          context,
          name: "tabs.onActivated",
          register: fire => {
            const listener = (eventName, event) => {
              const { windowId, tabId, isPrivate } = event;
              if (isPrivate && !context.privateBrowsingAllowed) {
                return;
              }
              fire.async({
                windowId,
                tabId,
                // In GeckoView each window has only one tab, so previousTabId is omitted.
              });
            };

            mobileWindowTracker.on("tab-activated", listener);
            return () => {
              mobileWindowTracker.off("tab-activated", listener);
            };
          },
        }).api(),

        onCreated: new EventManager({
          context,
          name: "tabs.onCreated",
          register: fire => {
            const listener = (eventName, event) => {
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
        onHighlighted: makeGlobalEvent(
          context,
          "tabs.onHighlighted",
          "Tab:Selected",
          (fire, data) => {
            const tab = tabManager.get(data.id);

            fire.async({ tabIds: [tab.id], windowId: tab.windowId });
          }
        ),

        onAttached: new EventManager({
          context,
          name: "tabs.onAttached",
          register: fire => {
            return () => {};
          },
        }).api(),

        onDetached: new EventManager({
          context,
          name: "tabs.onDetached",
          register: fire => {
            return () => {};
          },
        }).api(),

        onRemoved: new EventManager({
          context,
          name: "tabs.onRemoved",
          register: fire => {
            const listener = (eventName, event) => {
              fire.async(event.tabId, {
                windowId: event.windowId,
                isWindowClosing: event.isWindowClosing,
              });
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
          register: fire => {
            return () => {};
          },
        }).api(),

        onMoved: new EventManager({
          context,
          name: "tabs.onMoved",
          register: fire => {
            return () => {};
          },
        }).api(),

        onUpdated: new EventManager({
          context,
          name: "tabs.onUpdated",
          register: fire => {
            const restricted = ["url", "favIconUrl", "title"];

            function sanitize(tab, changeInfo) {
              const result = {};
              let nonempty = false;
              for (const prop in changeInfo) {
                // In practice, changeInfo contains at most one property from
                // restricted. Therefore it is not necessary to cache the value
                // of tab.hasTabPermission outside the loop.
                if (!restricted.includes(prop) || tab.hasTabPermission) {
                  nonempty = true;
                  result[prop] = changeInfo[prop];
                }
              }
              return [nonempty, result];
            }

            const fireForTab = (tab, changed) => {
              const [needed, changeInfo] = sanitize(tab, changed);
              if (needed) {
                fire.async(tab.id, changeInfo, tab.convert());
              }
            };

            const listener = event => {
              const needed = [];
              let nativeTab;
              switch (event.type) {
                case "pagetitlechanged": {
                  const window = getBrowserWindow(event.target.ownerGlobal);
                  nativeTab = window.tab;

                  needed.push("title");
                  break;
                }

                case "DOMAudioPlaybackStarted":
                case "DOMAudioPlaybackStopped": {
                  const window = event.target.ownerGlobal;
                  nativeTab = window.tab;
                  needed.push("audible");
                  break;
                }
              }

              if (!nativeTab) {
                return;
              }

              const tab = tabManager.getWrapper(nativeTab);
              const changeInfo = {};
              for (const prop of needed) {
                changeInfo[prop] = tab[prop];
              }

              fireForTab(tab, changeInfo);
            };

            const statusListener = ({ browser, status, url }) => {
              const { tab } = browser.ownerGlobal;
              if (tab) {
                const changed = { status };
                if (url) {
                  changed.url = url;
                }

                fireForTab(tabManager.wrapTab(tab), changed);
              }
            };

            windowTracker.addListener("status", statusListener);
            windowTracker.addListener("pagetitlechanged", listener);
            return () => {
              windowTracker.removeListener("status", statusListener);
              windowTracker.removeListener("pagetitlechanged", listener);
            };
          },
        }).api(),

        async create({
          active,
          cookieStoreId,
          discarded,
          index,
          openInReaderMode,
          pinned,
          title,
          url,
        } = {}) {
          if (active === null) {
            active = true;
          }

          tabListener.initTabReady();

          if (url !== null) {
            url = context.uri.resolve(url);

            if (!context.checkLoadURL(url, { dontReportErrors: true })) {
              return Promise.reject({ message: `Illegal URL: ${url}` });
            }
          }

          if (cookieStoreId) {
            cookieStoreId = getUserContextIdForCookieStoreId(
              extension,
              cookieStoreId,
              false // TODO bug 1372178: support creation of private browsing tabs
            );
          }
          cookieStoreId = cookieStoreId ? cookieStoreId.toString() : undefined;

          const nativeTab = await GeckoViewTabBridge.createNewTab({
            extensionId: context.extension.id,
            createProperties: {
              active,
              cookieStoreId,
              discarded,
              index,
              openInReaderMode,
              pinned,
              url,
            },
          });

          const { browser } = nativeTab;

          let flags = Ci.nsIWebNavigation.LOAD_FLAGS_NONE;

          // Make sure things like about:blank URIs never inherit,
          // and instead always get a NullPrincipal.
          if (url !== null) {
            tabListener.initializingTabs.add(nativeTab);
          } else {
            url = "about:blank";
          }

          let { principal } = context;
          if (url.startsWith("about:")) {
            // Make sure things like about:blank and other about: URIs never
            // inherit, and instead always get a NullPrincipal.
            flags |= Ci.nsIWebNavigation.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;
            // Falling back to content here as about: requires it, however is safe.
            principal = Services.scriptSecurityManager.getLoadContextContentPrincipal(
              Services.io.newURI(url),
              browser.loadContext
            );
          }

          browser.loadURI(url, {
            flags,
            triggeringPrincipal: principal,
          });

          if (active) {
            const newWindow = browser.ownerGlobal;
            mobileWindowTracker.setTabActive(newWindow, true);
          }

          return tabManager.convert(nativeTab);
        },

        async remove(tabs) {
          if (!Array.isArray(tabs)) {
            tabs = [tabs];
          }

          await Promise.all(
            tabs.map(async tabId => {
              const windowId = GeckoViewTabBridge.tabIdToWindowId(tabId);
              const window = windowTracker.getWindow(windowId, context, false);
              if (!window) {
                throw new ExtensionError(`Invalid tab ID ${tabId}`);
              }
              await GeckoViewTabBridge.closeTab({
                window,
                extensionId: context.extension.id,
              });
            })
          );
        },

        async update(
          tabId,
          { active, autoDiscardable, highlighted, muted, pinned, url } = {}
        ) {
          const nativeTab = getTabOrActive(tabId);
          const window = nativeTab.browser.ownerGlobal;

          if (url !== null) {
            url = context.uri.resolve(url);

            if (!context.checkLoadURL(url, { dontReportErrors: true })) {
              return Promise.reject({ message: `Illegal URL: ${url}` });
            }
          }

          await GeckoViewTabBridge.updateTab({
            window,
            extensionId: context.extension.id,
            updateProperties: {
              active,
              autoDiscardable,
              highlighted,
              muted,
              pinned,
              url,
            },
          });

          if (url !== null) {
            nativeTab.browser.loadURI(url, {
              triggeringPrincipal: context.principal,
            });
          }

          // FIXME: openerTabId, successorTabId
          if (active) {
            mobileWindowTracker.setTabActive(window, true);
          }

          return tabManager.convert(nativeTab);
        },

        async reload(tabId, reloadProperties) {
          const nativeTab = getTabOrActive(tabId);

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
          return Array.from(tabManager.query(queryInfo, context), tab =>
            tab.convert()
          );
        },

        async captureTab(tabId, options) {
          const nativeTab = getTabOrActive(tabId);
          await tabListener.awaitTabReady(nativeTab);

          const { browser } = nativeTab;
          const window = browser.ownerGlobal;
          const zoom = window.windowUtils.fullZoom;

          const tab = tabManager.wrapTab(nativeTab);
          return tab.capture(context, zoom, options);
        },

        async captureVisibleTab(windowId, options) {
          const window =
            windowId == null
              ? windowTracker.topWindow
              : windowTracker.getWindow(windowId, context);

          const tab = tabManager.wrapTab(window.tab);
          await tabListener.awaitTabReady(tab.nativeTab);
          const zoom = window.windowUtils.fullZoom;

          return tab.capture(context, zoom, options);
        },

        async executeScript(tabId, details) {
          const tab = await promiseTabWhenReady(tabId);

          return tab.executeScript(context, details);
        },

        async insertCSS(tabId, details) {
          const tab = await promiseTabWhenReady(tabId);

          return tab.insertCSS(context, details);
        },

        async removeCSS(tabId, details) {
          const tab = await promiseTabWhenReady(tabId);

          return tab.removeCSS(context, details);
        },

        goForward(tabId) {
          const { browser } = getTabOrActive(tabId);
          browser.goForward();
        },

        goBack(tabId) {
          const { browser } = getTabOrActive(tabId);
          browser.goBack();
        },
      },
    };
    return self;
  }
};
