/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "clearTimeout",
                                  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "require",
                                  "resource://devtools/shared/Loader.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
                                  "resource://gre/modules/Timer.jsm");

XPCOMUtils.defineLazyGetter(this, "colorUtils", () => {
  return require("devtools/shared/css/color").colorUtils;
});

const {
  stylesheetMap,
} = ExtensionUtils;

/* globals addMessageListener, content, docShell, sendAsyncMessage */

// Minimum time between two resizes.
const RESIZE_TIMEOUT = 100;

const BrowserListener = {
  init({allowScriptsToClose, fixedWidth, maxHeight, maxWidth, stylesheets}) {
    this.fixedWidth = fixedWidth;
    this.stylesheets = stylesheets || [];

    this.maxWidth = maxWidth;
    this.maxHeight = maxHeight;

    this.oldBackground = null;

    if (allowScriptsToClose) {
      content.QueryInterface(Ci.nsIInterfaceRequestor)
             .getInterface(Ci.nsIDOMWindowUtils)
             .allowScriptsToClose();
    }

    addEventListener("DOMWindowCreated", this, true);
    addEventListener("load", this, true);
    addEventListener("DOMContentLoaded", this, true);
    addEventListener("DOMWindowClose", this, true);
    addEventListener("MozScrolledAreaChanged", this, true);
  },

  destroy() {
    removeEventListener("DOMWindowCreated", this, true);
    removeEventListener("load", this, true);
    removeEventListener("DOMContentLoaded", this, true);
    removeEventListener("DOMWindowClose", this, true);
    removeEventListener("MozScrolledAreaChanged", this, true);
  },

  receiveMessage({name, data}) {
    if (name === "Extension:InitBrowser") {
      this.init(data);
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "DOMWindowCreated":
        if (event.target === content.document) {
          let winUtils = content.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindowUtils);

          for (let url of this.stylesheets) {
            winUtils.addSheet(stylesheetMap.get(url), winUtils.AGENT_SHEET);
          }
        }
        break;

      case "DOMWindowClose":
        if (event.target === content) {
          event.preventDefault();

          sendAsyncMessage("Extension:DOMWindowClose");
        }
        break;

      case "DOMContentLoaded":
        if (event.target === content.document) {
          sendAsyncMessage("Extension:BrowserContentLoaded", {url: content.location.href});
          this.handleDOMChange(true);
        }
        break;

      case "load":
        if (event.target.contentWindow === content) {
          // For about:addons inline <browsers>, we currently receive a load
          // event on the <browser> element, but no load or DOMContentLoaded
          // events from the content window.
          sendAsyncMessage("Extension:BrowserContentLoaded", {url: content.location.href});
        } else if (event.target !== content.document) {
          break;
        }

        // We use a capturing listener, so we get this event earlier than any
        // load listeners in the content page. Resizing after a timeout ensures
        // that we calculate the size after the entire event cycle has completed
        // (unless someone spins the event loop, anyway), and hopefully after
        // the content has made any modifications.
        Promise.resolve().then(() => {
          this.handleDOMChange(true);
        });

        // Mutation observer to make sure the panel shrinks when the content does.
        new content.MutationObserver(this.handleDOMChange.bind(this)).observe(
          content.document.documentElement, {
            attributes: true,
            characterData: true,
            childList: true,
            subtree: true,
          });
        break;

      case "MozScrolledAreaChanged":
        this.handleDOMChange();
        break;
    }
  },

  // Resizes the browser to match the preferred size of the content (debounced).
  handleDOMChange(ignoreThrottling = false) {
    if (ignoreThrottling && this.resizeTimeout) {
      clearTimeout(this.resizeTimeout);
      this.resizeTimeout = null;
    }

    if (this.resizeTimeout == null) {
      this.resizeTimeout = setTimeout(() => {
        try {
          if (content) {
            this._handleDOMChange("delayed");
          }
        } finally {
          this.resizeTimeout = null;
        }
      }, RESIZE_TIMEOUT);

      this._handleDOMChange();
    }
  },

  _handleDOMChange(detail) {
    let doc = content.document;

    let body = doc.body;
    if (!body || doc.compatMode === "BackCompat") {
      // In quirks mode, the root element is used as the scroll frame, and the
      // body lies about its scroll geometry, and returns the values for the
      // root instead.
      body = doc.documentElement;
    }


    let result;
    if (this.fixedWidth) {
      // If we're in a fixed-width area (namely a slide-in subview of the main
      // menu panel), we need to calculate the view height based on the
      // preferred height of the content document's root scrollable element at the
      // current width, rather than the complete preferred dimensions of the
      // content window.

      // Compensate for any offsets (margin, padding, ...) between the scroll
      // area of the body and the outer height of the document.
      let getHeight = elem => elem.getBoundingClientRect(elem).height;
      let bodyPadding = getHeight(doc.documentElement) - getHeight(body);

      let height = Math.ceil(body.scrollHeight + bodyPadding);

      result = {height, detail};
    } else {
      let background = doc.defaultView.getComputedStyle(body).backgroundColor;
      let bgColor = colorUtils.colorToRGBA(background);
      if (bgColor.a !== 1) {
        // Ignore non-opaque backgrounds.
        background = null;
      }

      if (background !== this.oldBackground) {
        sendAsyncMessage("Extension:BrowserBackgroundChanged", {background});
      }
      this.oldBackground = background;


      // Adjust the size of the browser based on its content's preferred size.
      let {contentViewer} = docShell;
      let ratio = content.devicePixelRatio;

      let w = {}, h = {};
      contentViewer.getContentSizeConstrained(this.maxWidth * ratio,
                                              this.maxHeight * ratio,
                                              w, h);

      let width = Math.ceil(w.value / ratio);
      let height = Math.ceil(h.value / ratio);

      result = {width, height, detail};
    }

    sendAsyncMessage("Extension:BrowserResized", result);
  },
};

addMessageListener("Extension:InitBrowser", BrowserListener);
