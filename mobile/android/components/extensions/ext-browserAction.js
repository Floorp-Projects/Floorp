/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource://devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

// Import the android BrowserActions module.
XPCOMUtils.defineLazyModuleGetter(this, "BrowserActions",
                                  "resource://gre/modules/BrowserActions.jsm");

// WeakMap[Extension -> BrowserAction]
var browserActionMap = new WeakMap();

const {
  DefaultMap,
} = ExtensionUtils;

class BrowserAction {
  constructor(options, extension) {
    // Map[TabID -> Object]
    this.tabIdToPropertyMap = new DefaultMap(() => ({}));
    this.tabManager = extension.tabManager;
    this.uuid = `{${extension.uuid}}`;
    this.name = options.default_title || extension.name;

    GlobalEventDispatcher.registerListener(this, ["Tab:Selected"]);
    GlobalEventDispatcher.registerListener(this, ["Tab:Closed"]);

    BrowserActions.register(this);
    EventEmitter.decorate(this);
  }

  /**
   * Retrieves the name for the active tab. Used for testing only.
   * @returns {string} the name used for the active tab.
   */
  get activeName() {
    let tab = tabTracker.activeTab;
    return this.tabIdToPropertyMap.get(tab.id).name || this.name;
  }

  /**
   * Required by the BrowserActions module. This event will get
   * called whenever the browser action is clicked on.
   */
  onClicked() {
    this.emit("click", tabTracker.activeTab);
  }

  /**
   * Required by the GlobalEventDispatcher module. This event will get
   * called whenever one of the registered listeners fires.
   * @param {string} event The event which fired.
   * @param {object} data Information about the event which fired.
   */
  onEvent(event, data) {
    switch (event) {
      case "Tab:Selected":
        this.onTabSelected(this.tabManager.get(data.id));
        break;
      case "Tab:Closed":
        this.onTabClosed(this.tabManager.get(data.tabId));
        break;
    }
  }

  /**
   * Updates the browser action whenever a tab is selected.
   * @param {Object} tab The tab to update.
   */
  onTabSelected(tab) {
    let name = this.tabIdToPropertyMap.get(tab.id).name || this.name;
    BrowserActions.update(this.uuid, {name});
  }

  /**
   * Removes the tab from the property map now that it is closed.
   * @param {Object} tab The tab which closed.
   */
  onTabClosed(tab) {
    this.tabIdToPropertyMap.delete(tab.id);
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
        this[prop] = value;
      }
    } else {
      let properties = this.tabIdToPropertyMap.get(tab.id);
      if (value) {
        properties[prop] = value;
      } else {
        delete properties[prop];
      }
    }

    if (tab && tab.selected) {
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
      return this[prop];
    }

    return this.tabIdToPropertyMap.get(tab.id)[prop] || this[prop];
  }

  /**
   * Unregister the browser action from the BrowserActions module.
   */
  shutdown() {
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

  onShutdown(reason) {
    let {extension} = this;

    if (browserActionMap.has(extension)) {
      browserActionMap.get(extension).shutdown();
      browserActionMap.delete(extension);
    }
  }

  getAPI(context) {
    const {extension} = context;
    const {tabManager} = extension;

    return {
      browserAction: {
        onClicked: new SingletonEventManager(context, "browserAction.onClicked", fire => {
          let listener = (event, tab) => {
            fire.async(tabManager.convert(tab));
          };
          browserActionMap.get(extension).on("click", listener);
          return () => {
            browserActionMap.get(extension).off("click", listener);
          };
        }).api(),

        setTitle: function(details) {
          let {tabId, title} = details;
          let tab = tabId ? tabTracker.getTab(tabId) : null;
          browserActionMap.get(extension).setProperty(tab, "name", title);
        },

        getTitle: function(details) {
          let {tabId} = details;
          let tab = tabId ? tabTracker.getTab(tabId) : null;
          let title = browserActionMap.get(extension).getProperty(tab, "name");
          return Promise.resolve(title);
        },
      },
    };
  }
};
