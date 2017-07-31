/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource://gre/modules/EventEmitter.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

// Import the android PageActions module.
XPCOMUtils.defineLazyModuleGetter(this, "PageActions",
                                  "resource://gre/modules/PageActions.jsm");

Cu.import("resource://gre/modules/ExtensionParent.jsm");

var {
  IconDetails,
} = ExtensionParent;

// WeakMap[Extension -> PageAction]
let pageActionMap = new WeakMap();

class PageAction {
  constructor(manifest, extension) {
    this.id = null;

    this.extension = extension;

    this.defaults = {
      icons: IconDetails.normalize({path: manifest.default_icon}, extension),
      popup: manifest.default_popup,
    };

    this.tabManager = extension.tabManager;
    this.context = null;

    this.tabContext = new TabContext(() => Object.create(this.defaults), extension);

    this.options = {
      title: manifest.default_title || extension.name,
      id: `{${extension.uuid}}`,
      clickCallback: () => {
        let tab = tabTracker.activeTab;
        let popup = this.tabContext.get(tab.id).popup || this.defaults.popup;
        if (popup) {
          let win = Services.wm.getMostRecentWindow("navigator:browser");
          win.BrowserApp.addTab(popup, {
            selected: true,
            parentId: win.BrowserApp.selectedTab.id,
          });
        } else {
          this.emit("click", tab);
        }
      },
    };

    this.shouldShow = false;

    this.tabContext.on("tab-selected", // eslint-disable-line mozilla/balanced-listeners
                       (evt, tabId) => { this.onTabSelected(tabId); });
    this.tabContext.on("tab-closed", // eslint-disable-line mozilla/balanced-listeners
                       (evt, tabId) => { this.onTabClosed(tabId); });

    EventEmitter.decorate(this);
  }

  /**
   * Updates the page action whenever a tab is selected.
   * @param {Integer} tabId The ID of the selected tab.
   */
  onTabSelected(tabId) {
    if (this.options.icon) {
      this.hide();
      let shouldShow = this.tabContext.get(tabId).show;
      if (shouldShow) {
        this.show();
      }
    }
  }

  /**
   * Removes the tab from the property map now that it is closed.
   * @param {Integer} tabId The ID of the closed tab.
   */
  onTabClosed(tabId) {
    this.tabContext.clear(tabId);
  }

  /**
   * Sets the context for the page action.
   * @param {Object} context The extension context.
   */
  setContext(context) {
    this.context = context;
  }

  /**
   * Sets a property for the page action for the specified tab. If the property is set
   * for the active tab, the page action is also updated.
   *
   * @param {Object} tab The tab to set.
   * @param {string} prop The property to update - either "show" or "popup".
   * @param {string} value The value to set the property to. If falsy, the property is deleted.
   * @returns {Object} Promise which resolves when the property is set and the page action is
   *    shown if necessary.
   */
  setProperty(tab, prop, value) {
    if (tab == null) {
      throw new Error("Tab must not be null");
    }

    let properties = this.tabContext.get(tab.id);
    if (value) {
      properties[prop] = value;
    } else {
      delete properties[prop];
    }

    if (prop === "show" && tab.id == tabTracker.activeTab.id) {
      if (this.id && !value) {
        return this.hide();
      } else if (!this.id && value) {
        return this.show();
      }
    }
  }

  /**
   * Retreives a property of the page action for the specified tab.
   *
   * @param {Object} tab The tab to retrieve the property from. If null, the default value is returned.
   * @param {string} prop The property to retreive - currently only "popup" is supported.
   * @returns {string} the value stored for the specified property. If the value for the tab is undefined, then the
   *    default value is returned.
   */
  getProperty(tab, prop) {
    if (tab == null) {
      return this.defaults[prop];
    }

    return this.tabContext.get(tab.id)[prop] || this.defaults[prop];
  }

  /**
   * Show the page action for the active tab.
   * @returns {Promise} resolves when the page action is shown.
   */
  show() {
    if (this.id) {
      return Promise.resolve();
    }

    if (this.options.icon) {
      this.id = PageActions.add(this.options);
      return Promise.resolve();
    }

    this.shouldShow = true;

    // Bug 1372782: Remove dependency on contentWindow from this file. It should
    // be put in a separate file called ext-c-pageAction.js.
    // Note: Fennec is not going to be multi-process for the foreseaable future,
    // so this layering violation has no immediate impact. However, it is should
    // be done at some point.
    let {contentWindow} = this.context.xulBrowser;

    // Bug 1372783: Why is this contentWindow.devicePixelRatio, while
    // convertImageURLToDataURL uses browserWindow.devicePixelRatio?
    let {icon} = IconDetails.getPreferredIcon(this.defaults.icons, this.extension,
                                              18 * contentWindow.devicePixelRatio);

    let browserWindow = Services.wm.getMostRecentWindow("navigator:browser");
    return IconDetails.convertImageURLToDataURL(icon, contentWindow, browserWindow).then(dataURI => {
      if (this.shouldShow) {
        this.options.icon = dataURI;
        this.id = PageActions.add(this.options);
      }
    }).catch(() => {
      return Promise.reject({
        message: "Failed to load PageAction icon",
      });
    });
  }

  /**
   * Hides the page action for the active tab.
   */
  hide() {
    this.shouldShow = false;
    if (this.id) {
      PageActions.remove(this.id);
      this.id = null;
    }
  }

  shutdown() {
    this.tabContext.shutdown();
    this.hide();
  }
};

this.pageAction = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    let pageAction = new PageAction(manifest.page_action, extension);
    pageActionMap.set(extension, pageAction);
  }

  onShutdown(reason) {
    let {extension} = this;

    if (pageActionMap.has(extension)) {
      pageActionMap.get(extension).shutdown();
      pageActionMap.delete(extension);
    }
  }

  getAPI(context) {
    const {extension} = context;
    const {tabManager} = extension;

    pageActionMap.get(extension).setContext(context);

    return {
      pageAction: {
        onClicked: new EventManager(context, "pageAction.onClicked", fire => {
          let listener = (event, tab) => {
            fire.async(tabManager.convert(tab));
          };
          pageActionMap.get(extension).on("click", listener);
          return () => {
            pageActionMap.get(extension).off("click", listener);
          };
        }).api(),

        show(tabId) {
          let tab = tabTracker.getTab(tabId);
          return pageActionMap.get(extension).setProperty(tab, "show", true);
        },

        hide(tabId) {
          let tab = tabTracker.getTab(tabId);
          pageActionMap.get(extension).setProperty(tab, "show", false);
        },

        setPopup(details) {
          let tab = tabTracker.getTab(details.tabId);
          let url = details.popup && context.uri.resolve(details.popup);
          pageActionMap.get(extension).setProperty(tab, "popup", url);
        },

        getPopup(details) {
          let tab = tabTracker.getTab(details.tabId);
          let popup = pageActionMap.get(extension).getProperty(tab, "popup");
          return Promise.resolve(popup);
        },
      },
    };
  }
};
