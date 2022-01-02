/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  clearTimeout: "resource://gre/modules/Timer.jsm",
  ExtensionCommon: "resource://gre/modules/ExtensionCommon.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
});

/* eslint-env mozilla/frame-script */

// Minimum time between two resizes.
const RESIZE_TIMEOUT = 100;

const BrowserListener = {
  init({
    allowScriptsToClose,
    blockParser,
    fixedWidth,
    maxHeight,
    maxWidth,
    stylesheets,
    isInline,
  }) {
    this.fixedWidth = fixedWidth;
    this.stylesheets = stylesheets || [];

    this.isInline = isInline;
    this.maxWidth = maxWidth;
    this.maxHeight = maxHeight;

    this.blockParser = blockParser;
    this.needsResize = fixedWidth || maxHeight || maxWidth;

    this.oldBackground = null;

    if (allowScriptsToClose) {
      content.windowUtils.allowScriptsToClose();
    }

    // Force external links to open in tabs.
    docShell.isAppTab = true;

    if (this.blockParser) {
      this.blockingPromise = new Promise(resolve => {
        this.unblockParser = resolve;
      });
      addEventListener("DOMDocElementInserted", this, true);
    }

    addEventListener("load", this, true);
    addEventListener("DOMWindowCreated", this, true);
    addEventListener("DOMContentLoaded", this, true);
    addEventListener("MozScrolledAreaChanged", this, true);
  },

  destroy() {
    if (this.blockParser) {
      removeEventListener("DOMDocElementInserted", this, true);
    }

    removeEventListener("load", this, true);
    removeEventListener("DOMWindowCreated", this, true);
    removeEventListener("DOMContentLoaded", this, true);
    removeEventListener("MozScrolledAreaChanged", this, true);
  },

  receiveMessage({ name, data }) {
    if (name === "Extension:InitBrowser") {
      this.init(data);
    } else if (name === "Extension:UnblockParser") {
      if (this.unblockParser) {
        this.unblockParser();
        this.blockingPromise = null;
      }
    } else if (name === "Extension:GrabFocus") {
      content.window.requestAnimationFrame(() => {
        Services.focus.focusedWindow = content.window;
      });
    }
  },

  loadStylesheets() {
    let { windowUtils } = content;

    for (let url of this.stylesheets) {
      windowUtils.addSheet(
        ExtensionCommon.stylesheetMap.get(url),
        windowUtils.AGENT_SHEET
      );
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "DOMDocElementInserted":
        if (this.blockingPromise) {
          event.target.blockParsing(this.blockingPromise);
        }
        break;

      case "DOMWindowCreated":
        if (event.target === content.document) {
          this.loadStylesheets();
        }
        break;

      case "DOMContentLoaded":
        if (event.target === content.document) {
          sendAsyncMessage("Extension:BrowserContentLoaded", {
            url: content.location.href,
          });

          if (this.needsResize) {
            this.handleDOMChange(true);
          }
        }
        break;

      case "load":
        if (event.target.contentWindow === content) {
          // For about:addons inline <browser>s, we currently receive a load
          // event on the <browser> element, but no load or DOMContentLoaded
          // events from the content window.

          // Inline browsers don't receive the "DOMWindowCreated" event, so this
          // is a workaround to load the stylesheets.
          if (this.isInline) {
            this.loadStylesheets();
          }
          sendAsyncMessage("Extension:BrowserContentLoaded", {
            url: content.location.href,
          });
        } else if (event.target !== content.document) {
          break;
        }

        if (!this.needsResize) {
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
          content.document.documentElement,
          {
            attributes: true,
            characterData: true,
            childList: true,
            subtree: true,
          }
        );
        break;

      case "MozScrolledAreaChanged":
        if (this.needsResize) {
          this.handleDOMChange();
        }
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
    const zoom = content.browsingContext.fullZoom;
    if (this.fixedWidth) {
      // If we're in a fixed-width area (namely a slide-in subview of the main
      // menu panel), we need to calculate the view height based on the
      // preferred height of the content document's root scrollable element at the
      // current width, rather than the complete preferred dimensions of the
      // content window.

      // Compensate for any offsets (margin, padding, ...) between the scroll
      // area of the body and the outer height of the document.
      // This calculation is hard to get right for all cases, so take the lower
      // number of the combination of all padding and margins of the document
      // and body elements, or the difference between their heights.
      let getHeight = elem => elem.getBoundingClientRect(elem).height;
      let bodyPadding = getHeight(doc.documentElement) - getHeight(body);

      if (body !== doc.documentElement) {
        let bs = content.getComputedStyle(body);
        let ds = content.getComputedStyle(doc.documentElement);

        let p =
          parseFloat(bs.marginTop) +
          parseFloat(bs.marginBottom) +
          parseFloat(ds.marginTop) +
          parseFloat(ds.marginBottom) +
          parseFloat(ds.paddingTop) +
          parseFloat(ds.paddingBottom);
        bodyPadding = Math.min(p, bodyPadding);
      }

      let height = Math.ceil((body.scrollHeight + bodyPadding) * zoom);

      result = { height, detail };
    } else {
      let background = content.windowUtils.canvasBackgroundColor;
      if (background !== this.oldBackground) {
        sendAsyncMessage("Extension:BrowserBackgroundChanged", { background });
      }
      this.oldBackground = background;

      // Adjust the size of the browser based on its content's preferred size.
      let { contentViewer } = docShell;
      let ratio = content.devicePixelRatio;

      let w = {},
        h = {};
      contentViewer.getContentSizeConstrained(
        this.maxWidth * ratio,
        this.maxHeight * ratio,
        w,
        h
      );

      let width = Math.ceil((w.value * zoom) / ratio);
      let height = Math.ceil((h.value * zoom) / ratio);

      result = { width, height, detail };
    }

    sendAsyncMessage("Extension:BrowserResized", result);
  },
};

addMessageListener("Extension:InitBrowser", BrowserListener);
addMessageListener("Extension:UnblockParser", BrowserListener);
addMessageListener("Extension:GrabFocus", BrowserListener);

// This is a temporary hack to prevent regressions (bug 1471327).
void content;
