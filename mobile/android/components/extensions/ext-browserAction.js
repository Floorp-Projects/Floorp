/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-utils.js */

// Import the android BrowserActions module.
ChromeUtils.defineModuleGetter(this, "BrowserActions",
                               "resource://gre/modules/BrowserActions.jsm");

// WeakMap[Extension -> BrowserAction]
let browserActionMap = new WeakMap();

class BrowserAction extends EventEmitter {
  constructor(options, extension) {
    super();

    this.uuid = `{${extension.uuid}}`;

    this.defaults = {
      name: options.default_title || extension.name,
      popup: options.default_popup,
    };

    this.tabContext = new TabContext(tabId => this.defaults);

    this.tabManager = extension.tabManager;

    this.tabContext.on("tab-selected", // eslint-disable-line mozilla/balanced-listeners
                       (evt, tabId) => { this.onTabSelected(tabId); });
    this.tabContext.on("tab-closed", // eslint-disable-line mozilla/balanced-listeners
                       (evt, tabId) => { this.onTabClosed(tabId); });

    BrowserActions.register(this);
  }

  /**
   * Required by the BrowserActions module. This event will get
   * called whenever the browser action is clicked on.
   */
  onClicked() {
    const tab = tabTracker.activeTab;

    this.tabManager.addActiveTabPermission(tab);

    let popup = this.tabContext.get(tab.id).popup || this.defaults.popup;
    if (popup) {
      tabTracker.openExtensionPopupTab(popup);
    } else {
      this.emit("click", tab);
    }
  }

  /**
   * Updates the browser action whenever a tab is selected.
   * @param {string} tabId The tab id to update.
   */
  onTabSelected(tabId) {
    let name = this.tabContext.get(tabId).name || this.defaults.name;
    BrowserActions.update(this.uuid, {name});
  }

  /**
   * Removes the tab from the property map now that it is closed.
   * @param {string} tabId The tab id of the closed tab.
   */
  onTabClosed(tabId) {
    this.tabContext.clear(tabId);
  }

  /**
   * Sets a property for the browser action for the specified tab. If the property is set
   * for the active tab, the browser action is also updated.
   *
   * @param {Object} tab The tab to set. If null, the browser action's default value is set.
   * @param {string} prop The property to update. Currently only "name" is supported.
   * @param {string} value The value to set the property to.
   */
  setProperty(tab, prop, value) {
    if (tab == null) {
      if (value) {
        this.defaults[prop] = value;
      }
    } else {
      let properties = this.tabContext.get(tab.id);
      if (value) {
        properties[prop] = value;
      } else {
        delete properties[prop];
      }
    }

    if (!tab || tab.getActive()) {
      BrowserActions.update(this.uuid, {[prop]: value});
    }
  }

  /**
   * Retreives a property of the browser action for the specified tab.
   *
   * @param {Object} tab The tab to retrieve the property from. If null, the default value is returned.
   * @param {string} prop The property to retreive. Currently only "name" is supported.
   * @returns {string} the value stored for the specified property. If the value is undefined, then the
   *    default value is returned.
   */
  getProperty(tab, prop) {
    if (tab == null) {
      return this.defaults[prop];
    }

    return this.tabContext.get(tab.id)[prop] || this.defaults[prop];
  }

  /**
   * Unregister the browser action from the BrowserActions module.
   */
  shutdown() {
    this.tabContext.shutdown();
    BrowserActions.unregister(this.uuid);
  }
}

this.browserAction = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    let {extension} = this;
    let {manifest} = extension;

    let browserAction = new BrowserAction(manifest.browser_action, extension);
    browserActionMap.set(extension, browserAction);
  }

  onShutdown() {
    let {extension} = this;

    if (browserActionMap.has(extension)) {
      browserActionMap.get(extension).shutdown();
      browserActionMap.delete(extension);
    }
  }

  getAPI(context) {
    const {extension} = context;
    const {tabManager} = extension;

    function getTab(tabId) {
      if (tabId !== null) {
        return tabTracker.getTab(tabId);
      }
      return null;
    }

    return {
      browserAction: {
        onClicked: new EventManager({
          context,
          name: "browserAction.onClicked",
          register: fire => {
            let listener = (event, tab) => {
              fire.async(tabManager.convert(tab));
            };
            browserActionMap.get(extension).on("click", listener);
            return () => {
              browserActionMap.get(extension).off("click", listener);
            };
          },
        }).api(),

        setTitle: function(details) {
          let {tabId, title} = details;
          let tab = getTab(tabId);
          browserActionMap.get(extension).setProperty(tab, "name", title);
        },

        getTitle: function(details) {
          let {tabId} = details;
          let tab = getTab(tabId);
          let title = browserActionMap.get(extension).getProperty(tab, "name");
          return Promise.resolve(title);
        },

        setPopup(details) {
          let tab = getTab(details.tabId);
          let url = details.popup && context.uri.resolve(details.popup);
          browserActionMap.get(extension).setProperty(tab, "popup", url);
        },

        getPopup(details) {
          let tab = getTab(details.tabId);
          let popup = browserActionMap.get(extension).getProperty(tab, "popup");
          return Promise.resolve(popup);
        },
      },
    };
  }
};
