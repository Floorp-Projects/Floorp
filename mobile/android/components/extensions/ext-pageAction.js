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

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  IconDetails,
} = ExtensionUtils;

// WeakMap[Extension -> PageAction]
var pageActionMap = new WeakMap();

function PageAction(options, extension) {
  this.id = null;

  this.extension = extension;
  this.icons = IconDetails.normalize({path: options.default_icon}, extension);

  this.popupUrl = options.default_popup;

  this.options = {
    title: options.default_title || extension.name,
    id: `{${extension.uuid}}`,
    clickCallback: () => {
      if (this.popupUrl) {
        let win = Services.wm.getMostRecentWindow("navigator:browser");
        win.BrowserApp.addTab(this.popupUrl, {
          selected: true,
          parentId: win.BrowserApp.selectedTab.id,
        });
      } else {
        this.emit("click", tabTracker.activeTab);
      }
    },
  };

  this.shouldShow = false;

  EventEmitter.decorate(this);
}

PageAction.prototype = {
  show(tabId, context) {
    if (this.id) {
      return Promise.resolve();
    }

    if (this.options.icon) {
      this.id = PageActions.add(this.options);
      return Promise.resolve();
    }

    this.shouldShow = true;

    // TODO(robwu): Remove dependency on contentWindow from this file. It should
    // be put in a separate file called ext-c-pageAction.js.
    // Note: Fennec is not going to be multi-process for the foreseaable future,
    // so this layering violation has no immediate impact. However, it is should
    // be done at some point.
    let {contentWindow} = context.xulBrowser;

    // TODO(robwu): Why is this contentWindow.devicePixelRatio, while
    // convertImageURLToDataURL uses browserWindow.devicePixelRatio?
    let {icon} = IconDetails.getPreferredIcon(this.icons, this.extension,
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
  },

  hide(tabId) {
    this.shouldShow = false;
    if (this.id) {
      PageActions.remove(this.id);
      this.id = null;
    }
  },

  setPopup(tab, url) {
    // TODO: Only set the popup for the specified tab once we have Tabs API support.
    this.popupUrl = url;
  },

  getPopup(tab) {
    // TODO: Only return the popup for the specified tab once we have Tabs API support.
    return this.popupUrl;
  },

  shutdown() {
    this.hide();
  },
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

    return {
      pageAction: {
        onClicked: new SingletonEventManager(context, "pageAction.onClicked", fire => {
          let listener = (event, tab) => {
            fire.async(tabManager.convert(tab));
          };
          pageActionMap.get(extension).on("click", listener);
          return () => {
            pageActionMap.get(extension).off("click", listener);
          };
        }).api(),

        show(tabId) {
          return pageActionMap.get(extension)
                              .show(tabId, context)
                              .then(() => {});
        },

        hide(tabId) {
          pageActionMap.get(extension).hide(tabId);
          return Promise.resolve();
        },

        setPopup(details) {
          // TODO: Use the Tabs API to get the tab from details.tabId.
          let tab = null;
          let url = details.popup && context.uri.resolve(details.popup);
          pageActionMap.get(extension).setPopup(tab, url);
        },

        getPopup(details) {
          // TODO: Use the Tabs API to get the tab from details.tabId.
          let tab = null;
          let popup = pageActionMap.get(extension).getPopup(tab);
          return Promise.resolve(popup);
        },
      },
    };
  }
};
