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

class BrowserAction {
  constructor(options, extension) {
    this.id = `{${extension.uuid}}`;
    this.name = options.default_title || extension.name;
    BrowserActions.register(this);
    EventEmitter.decorate(this);
  }

  /**
   * Required by the BrowserActions module. This event will get
   * called whenever the browser action is clicked on.
   */
  onClicked() {
    this.emit("click", tabTracker.activeTab);
  }

  /**
   * Unregister the browser action from the BrowserActions module.
   */
  shutdown() {
    BrowserActions.unregister(this.id);
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
      },
    };
  }
};
