/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "EventEmitter",
                                  "resource://devtools/shared/event-emitter.js");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

// Import the android PageActions module.
XPCOMUtils.defineLazyModuleGetter(this, "PageActions",
                                  "resource://gre/modules/PageActions.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  SingletonEventManager,
} = ExtensionUtils;

// WeakMap[Extension -> PageAction]
var pageActionMap = new WeakMap();

function PageAction(options, extension) {
  this.id = null;

  let DEFAULT_ICON = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACQAAAAkCAYAAADhAJiYAAAC4klEQVRYhdWXLWzbQBSADQtDAwsHC1tUhUxqfL67lk2tdn+OJg0ODU0rLByqgqINBY6tmlbn7LMTJ5FaFVVBk1G0oUGjG2jT2Y7jxmmcbU/6iJ+f36fz+e5sGP9riCGm9hB37RG+scd4Yo/wsDXCZyIE2xuXsce4bY+wXkAsQtzYmExrfFgvkJkRbkzo1ehoxx5iXcgI/9iYUGt8WH9MqDXEcmNChmEYrRCf2SHWeYgQx3x0tLNRIeKQLTtEFyJEep4NTuhk8BC+yMrwEE3+iozo42d8gK7FAOkMsRiiN8QhW2ttSK5QTfRRV4QoymVeJMvPvDp7gCZigD613MN6yRFA3SWarow9QB9LCfG+NeF9qCtjAKOSQjCqVKhfVsiHEQ+grgx/lRGqUihAc1uL8EFD+KCRO+GrF4J61phcoRoPoEzkYhZYpykh5sMb7kOdIeY+jHKur4QI4Feh4AFX1nVeLxrAvQchGsBz5ls6wa2QdwcvIcE2863bTH79KOvsz/uUYJsp+J0pSzNlDckVqqVGUAF+n6uS7txcOl6wot4JVy70ufDLy4pWLUQVPE81pRI0mGe9oxLMHSeohHvMs/STUNaUK6vDPCvOyxMFDx4achehRDJmHnydnkPww5OFfLxrGIZBFDyYl4LpMzlTQFIP6AQx86w2UeYBccFpJrcKv5L9eGDtUAU6RIELqsB74uynjy/UBRF1gS5BTFxwQT1wTiXoUg9MH7m/3NZRRoi5IJytUbMgzv4Wc832+oQkiKgEehmyMkkpKsFkQV11QsRJL5rJYBLItQgRaUZEmnoZXsomz3vGiWw+I9KMF9SVFOqZEemZekli1jN3U/UOqhHHvC6oWWGElhfSpGdOk6+O9prdwvtLj5BjRsQxdRnot+Zeifpy/2/0stktKTRNLmbk0mwXyl8253fyojj+8rxOHNAhjjm5n0/5OOCGOKBzkrMO0Z75lvSAzKlrF32Z/3z8BqLAn+yMV7VhAAAAAElFTkSuQmCC";

  this.popupUrl = options.default_popup;

  this.options = {
    title: options.default_title || extension.name,
    icon: DEFAULT_ICON,
    id: extension.id,
    clickCallback: () => {
      if (this.popupUrl) {
        let win = Services.wm.getMostRecentWindow("navigator:browser");
        win.BrowserApp.addTab(this.popupUrl, {
          selected: true,
          parentId: win.BrowserApp.selectedTab.id,
        });
      } else {
        this.emit("click");
      }
    },
  };

  EventEmitter.decorate(this);
}

PageAction.prototype = {
  show(tabId) {
    // TODO: Only show the PageAction for the tab with the provided tabId.
    if (!this.id) {
      this.id = PageActions.add(this.options);
    }
  },

  hide(tabId) {
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

/* eslint-disable mozilla/balanced-listeners */
extensions.on("manifest_page_action", (type, directive, extension, manifest) => {
  let pageAction = new PageAction(manifest.page_action, extension);
  pageActionMap.set(extension, pageAction);
});

extensions.on("shutdown", (type, extension) => {
  if (pageActionMap.has(extension)) {
    pageActionMap.get(extension).shutdown();
    pageActionMap.delete(extension);
  }
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("pageAction", (extension, context) => {
  return {
    pageAction: {
      onClicked: new SingletonEventManager(context, "pageAction.onClicked", fire => {
        let listener = (event) => {
          fire();
        };
        pageActionMap.get(extension).on("click", listener);
        return () => {
          pageActionMap.get(extension).off("click", listener);
        };
      }).api(),

      show(tabId) {
        pageActionMap.get(extension).show(tabId);
      },

      hide(tabId) {
        pageActionMap.get(extension).hide(tabId);
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
});
