/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

let lazy = {};

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "dragService",
  "@mozilla.org/widget/dragservice;1",
  "nsIDragService"
);

const HTML_NS = "http://www.w3.org/1999/xhtml";

/**
 * The SubDialog resize callback.
 * @callback SubDialog~resizeCallback
 * @param {DOMNode} title - The title element of the dialog.
 * @param {xul:browser} frame - The browser frame of the dialog.
 */

/**
 * SubDialog constructor creates a new subdialog from a template and appends
 * it to the parentElement.
 * @param {DOMNode} template - The template is copied to create a new dialog.
 * @param {DOMNode} parentElement - New dialog is appended onto parentElement.
 * @param {String}  id - A unique identifier for the dialog.
 * @param {Object}  dialogOptions - Dialog options object.
 * @param {String[]} [dialogOptions.styleSheets] - An array of URLs to additional
 * stylesheets to inject into the frame.
 * @param {Boolean} [consumeOutsideClicks] - Whether to close the dialog when
 * its background overlay is clicked.
 * @param {SubDialog~resizeCallback} [resizeCallback] - Function to be called on
 * dialog resize.
 */
export function SubDialog({
  template,
  parentElement,
  id,
  dialogOptions: {
    styleSheets = [],
    consumeOutsideClicks = true,
    resizeCallback,
  } = {},
}) {
  this._id = id;

  this._injectedStyleSheets = this._injectedStyleSheets.concat(styleSheets);
  this._consumeOutsideClicks = consumeOutsideClicks;
  this._resizeCallback = resizeCallback;
  this._overlay = template.cloneNode(true);
  this._box = this._overlay.querySelector(".dialogBox");
  this._titleBar = this._overlay.querySelector(".dialogTitleBar");
  this._titleElement = this._overlay.querySelector(".dialogTitle");
  this._closeButton = this._overlay.querySelector(".dialogClose");
  this._frame = this._overlay.querySelector(".dialogFrame");

  this._overlay.classList.add(`dialogOverlay-${id}`);
  this._frame.setAttribute("name", `dialogFrame-${id}`);
  this._frameCreated = new Promise(resolve => {
    this._frame.addEventListener(
      "load",
      () => {
        // We intentionally avoid handling or passing the event to the
        // resolve method to avoid shutdown window leaks. See bug 1686743.
        resolve();
      },
      {
        once: true,
        capture: true,
      }
    );
  });

  parentElement.appendChild(this._overlay);
  this._overlay.hidden = false;
}

SubDialog.prototype = {
  _closingCallback: null,
  _closingEvent: null,
  _isClosing: false,
  _frame: null,
  _frameCreated: null,
  _overlay: null,
  _box: null,
  _openedURL: null,
  _injectedStyleSheets: ["chrome://global/skin/in-content/common.css"],
  _resizeObserver: null,
  _template: null,
  _id: null,
  _titleElement: null,
  _closeButton: null,

  get frameContentWindow() {
    return this._frame?.contentWindow;
  },

  get _window() {
    return this._overlay?.ownerGlobal;
  },

  updateTitle(aEvent) {
    if (aEvent.target != this._frame.contentDocument) {
      return;
    }
    this._titleElement.textContent = this._frame.contentDocument.title;
  },

  injectStylesheet(aStylesheetURL) {
    const doc = this._frame.contentDocument;
    if ([...doc.styleSheets].find(s => s.href === aStylesheetURL)) {
      return;
    }

    // Attempt to insert the stylesheet as a link element into the same place in
    // the document as other link elements. It is almost certain that any
    // document will already have a localization or other stylesheet link
    // present.
    let links = doc.getElementsByTagNameNS(HTML_NS, "link");
    if (links.length) {
      let stylesheetLink = doc.createElementNS(HTML_NS, "link");
      stylesheetLink.setAttribute("rel", "stylesheet");
      stylesheetLink.setAttribute("href", aStylesheetURL);

      // Insert after the last found link element.
      links[links.length - 1].after(stylesheetLink);

      return;
    }

    // In the odd case just insert at the top as a processing instruction.
    let contentStylesheet = doc.createProcessingInstruction(
      "xml-stylesheet",
      'href="' + aStylesheetURL + '" type="text/css"'
    );
    doc.insertBefore(contentStylesheet, doc.documentElement);
  },

  async open(
    aURL,
    { features, closingCallback, closedCallback, sizeTo } = {},
    ...aParams
  ) {
    if (["available", "limitheight"].includes(sizeTo)) {
      this._box.setAttribute("sizeto", sizeTo);
    }

    // Create a promise so consumers can tell when we're done setting up.
    this._dialogReady = new Promise(resolve => {
      this._resolveDialogReady = resolve;
    });
    this._frame._dialogReady = this._dialogReady;

    // Assign close callbacks sync to ensure we can always callback even if the
    // SubDialog is closed directly after opening.
    let dialog = null;

    if (closingCallback) {
      this._closingCallback = (...args) => {
        closingCallback.apply(dialog, args);
      };
    }
    if (closedCallback) {
      this._closedCallback = (...args) => {
        closedCallback.apply(dialog, args);
      };
    }

    // Wait until frame is ready to prevent browser crash in tests
    await this._frameCreated;

    if (!this._frame.contentWindow) {
      // Given the binding constructor execution is asynchronous, and "load"
      // event can be dispatched before the browser element is shown, the
      // browser binding might not be constructed at this point.  Forcibly
      // construct the frame and construct the binding.
      // FIXME: Remove this (bug 1437247)
      this._frame.getBoundingClientRect();
    }

    // If we're open on some (other) URL or we're closing, open when closing has finished.
    if (this._openedURL || this._isClosing) {
      if (!this._isClosing) {
        this.close();
      }
      let args = Array.from(arguments);
      this._closingPromise.then(() => {
        this.open.apply(this, args);
      });
      return;
    }
    this._addDialogEventListeners();

    // Ensure we end any pending drag sessions:
    try {
      // The drag service getService call fails in puppeteer tests on Linux,
      // so this is in a try...catch as it shouldn't stop us from opening the
      // dialog. Bug 1806870 tracks fixing this.
      if (lazy.dragService.getCurrentSession()) {
        lazy.dragService.endDragSession(true);
      }
    } catch (ex) {
      console.error(ex);
    }

    // If the parent is chrome we also need open the dialog as chrome, otherwise
    // the openDialog call will fail.
    let dialogFeatures = `resizable,dialog=no,centerscreen,chrome=${
      this._window?.isChromeWindow ? "yes" : "no"
    }`;
    if (features) {
      dialogFeatures = `${features},${dialogFeatures}`;
    }

    dialog = this._window.openDialog(
      aURL,
      `dialogFrame-${this._id}`,
      dialogFeatures,
      ...aParams
    );

    this._closingEvent = null;
    this._isClosing = false;
    this._openedURL = aURL;

    dialogFeatures = dialogFeatures.replace(/,/g, "&");
    let featureParams = new URLSearchParams(dialogFeatures.toLowerCase());
    this._box.setAttribute(
      "resizable",
      featureParams.has("resizable") &&
        featureParams.get("resizable") != "no" &&
        featureParams.get("resizable") != "0"
    );
  },

  /**
   * Close the dialog and mark it as aborted.
   */
  abort() {
    this._closingEvent = new CustomEvent("dialogclosing", {
      bubbles: true,
      detail: { dialog: this, abort: true },
    });
    this._frame.contentWindow.close();
    // It's possible that we're aborting this dialog before we've had a
    // chance to set up the contentWindow.close function override in
    // _onContentLoaded. If so, call this.close() directly to clean things
    // up. That'll be a no-op if the contentWindow.close override had been
    // set up, since this.close is idempotent.
    this.close(this._closingEvent);
  },

  close(aEvent = null) {
    if (this._isClosing) {
      return;
    }
    this._isClosing = true;
    this._closingPromise = new Promise(resolve => {
      this._resolveClosePromise = resolve;
    });

    if (this._closingCallback) {
      try {
        this._closingCallback.call(null, aEvent);
      } catch (ex) {
        console.error(ex);
      }
      this._closingCallback = null;
    }

    this._removeDialogEventListeners();

    this._overlay.style.visibility = "";
    // Clear the sizing inline styles.
    this._frame.removeAttribute("style");
    // Clear the sizing attributes
    this._box.removeAttribute("width");
    this._box.removeAttribute("height");
    this._box.style.removeProperty("--box-max-height-requested");
    this._box.style.removeProperty("--box-max-width-requested");
    this._box.style.removeProperty("min-height");
    this._box.style.removeProperty("min-width");
    this._overlay.style.removeProperty("--subdialog-inner-height");

    let onClosed = () => {
      this._openedURL = null;

      this._resolveClosePromise();

      if (this._closedCallback) {
        try {
          this._closedCallback.call(null, aEvent);
        } catch (ex) {
          console.error(ex);
        }
        this._closedCallback = null;
      }
    };

    // Wait for the frame to unload before running the closed callback.
    if (this._frame.contentWindow) {
      this._frame.contentWindow.addEventListener("unload", onClosed, {
        once: true,
      });
    } else {
      onClosed();
    }

    this._overlay.dispatchEvent(
      new CustomEvent("dialogclose", {
        bubbles: true,
        detail: { dialog: this },
      })
    );

    // Defer removing the overlay so the frame content window can unload.
    Services.tm.dispatchToMainThread(() => {
      this._overlay.remove();
    });
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        // Close the dialog if the user clicked the overlay background, just
        // like when the user presses the ESC key (case "command" below).
        if (aEvent.target !== this._overlay) {
          break;
        }
        if (this._consumeOutsideClicks) {
          this._frame.contentWindow.close();
          break;
        }
        this._frame.focus();
        break;
      case "command":
        this._frame.contentWindow.close();
        break;
      case "dialogclosing":
        this._onDialogClosing(aEvent);
        break;
      case "DOMTitleChanged":
        this.updateTitle(aEvent);
        break;
      case "DOMFrameContentLoaded":
        this._onContentLoaded(aEvent);
        break;
      case "load":
        this._onLoad(aEvent);
        break;
      case "unload":
        this._onUnload(aEvent);
        break;
      case "keydown":
        this._onKeyDown(aEvent);
        break;
      case "focus":
        this._onParentWinFocus(aEvent);
        break;
    }
  },

  /* Private methods */

  _onUnload(aEvent) {
    if (
      aEvent.target !== this._frame?.contentDocument ||
      aEvent.target.location.href !== this._openedURL
    ) {
      return;
    }
    this.abort();
  },

  _onContentLoaded(aEvent) {
    if (
      aEvent.target != this._frame ||
      aEvent.target.contentWindow.location == "about:blank"
    ) {
      return;
    }

    for (let styleSheetURL of this._injectedStyleSheets) {
      this.injectStylesheet(styleSheetURL);
    }

    let { contentDocument } = this._frame;
    // Provide the ability for the dialog to know that it is loaded in a frame
    // rather than as a top-level window.
    for (let dialog of contentDocument.querySelectorAll("dialog")) {
      dialog.setAttribute("subdialog", "true");
    }
    // Sub-dialogs loaded in a chrome window should use the system font size so
    // that the user has a way to increase or decrease it via system settings.
    // Sub-dialogs loaded in the content area, on the other hand, can be zoomed
    // like web content.
    if (this._window.isChromeWindow) {
      contentDocument.documentElement.classList.add("system-font-size");
    }
    // Used by CSS to give the appropriate background colour in dark mode.
    contentDocument.documentElement.setAttribute("dialogroot", "true");

    this._frame.contentWindow.addEventListener("dialogclosing", this);

    let oldResizeBy = this._frame.contentWindow.resizeBy;
    this._frame.contentWindow.resizeBy = (resizeByWidth, resizeByHeight) => {
      // Only handle resizeByHeight currently.
      let frameHeight = this._overlay.style.getPropertyValue(
        "--subdialog-inner-height"
      );
      if (frameHeight) {
        frameHeight = parseFloat(frameHeight);
      } else {
        frameHeight = this._frame.clientHeight;
      }
      let boxMinHeight = parseFloat(
        this._window.getComputedStyle(this._box).minHeight
      );

      this._box.style.minHeight = boxMinHeight + resizeByHeight + "px";

      this._overlay.style.setProperty(
        "--subdialog-inner-height",
        frameHeight + resizeByHeight + "px"
      );

      oldResizeBy.call(
        this._frame.contentWindow,
        resizeByWidth,
        resizeByHeight
      );
    };

    // Make window.close calls work like dialog closing.
    let oldClose = this._frame.contentWindow.close;
    this._frame.contentWindow.close = () => {
      var closingEvent = this._closingEvent;
      // If this._closingEvent is set, the dialog is closed externally
      // (dialog.js) and "dialogclosing" has already been dispatched.
      if (!closingEvent) {
        // If called without closing event, we need to create and dispatch it.
        // This is the case for any external close calls not going through
        // dialog.js.
        closingEvent = new CustomEvent("dialogclosing", {
          bubbles: true,
          detail: { button: null },
        });

        this._frame.contentWindow.dispatchEvent(closingEvent);
      } else if (this._closingEvent.detail?.abort) {
        // If the dialog is aborted (SubDialog#abort) we need to dispatch the
        // "dialogclosing" event ourselves.
        this._frame.contentWindow.dispatchEvent(closingEvent);
      }

      this.close(closingEvent);
      oldClose.call(this._frame.contentWindow);
    };

    // XXX: Hack to make focus during the dialog's load functions work. Make the element visible
    // sooner in DOMContentLoaded but mostly invisible instead of changing visibility just before
    // the dialog's load event.
    // Note that this needs to inherit so that hideDialog() works as expected.
    this._overlay.style.visibility = "inherit";
    this._overlay.style.opacity = "0.01";

    // Ensure the document gets an a11y role of dialog.
    const a11yDoc = contentDocument.body || contentDocument.documentElement;
    a11yDoc.setAttribute("role", "dialog");

    Services.obs.notifyObservers(this._frame.contentWindow, "subdialog-loaded");
  },

  async _onLoad(aEvent) {
    let target = aEvent.currentTarget;
    if (target.contentWindow.location == "about:blank") {
      return;
    }

    // In order to properly calculate the sizing of the subdialog, we need to
    // ensure that all of the l10n is done.
    if (target.contentDocument.l10n) {
      await target.contentDocument.l10n.ready;
    }

    // Some subdialogs may want to perform additional, asynchronous steps during initializations.
    //
    // In that case, we expect them to define a Promise which will delay measuring
    // until the promise is fulfilled.
    if (target.contentDocument.mozSubdialogReady) {
      await target.contentDocument.mozSubdialogReady;
    }

    await this.resizeDialog();
    this._resolveDialogReady();
  },

  async resizeDialog() {
    // Do this on load to wait for the CSS to load and apply before calculating the size.
    let docEl = this._frame.contentDocument.documentElement;

    // These are deduced from styles which we don't change, so it's safe to get them now:
    let boxHorizontalBorder =
      2 * parseFloat(this._window.getComputedStyle(this._box).borderLeftWidth);
    let frameHorizontalMargin =
      2 * parseFloat(this._window.getComputedStyle(this._frame).marginLeft);

    // Then determine and set a bunch of width stuff:
    let { scrollWidth } = docEl.ownerDocument.body || docEl;
    // We need to convert em to px because an em value from the dialog window could
    // translate to something else in the host window, as font sizes may vary.
    let frameMinWidth =
      this._emToPx(docEl.style.minWidth) ||
      this._emToPx(docEl.style.width) ||
      scrollWidth + "px";
    let frameWidth = docEl.getAttribute("width")
      ? docEl.getAttribute("width") + "px"
      : scrollWidth + "px";
    if (
      this._box.getAttribute("sizeto") == "available" &&
      docEl.style.maxWidth
    ) {
      this._box.style.setProperty(
        "--box-max-width-requested",
        this._emToPx(docEl.style.maxWidth)
      );
    }

    if (this._box.getAttribute("sizeto") != "available") {
      this._frame.style.width = frameWidth;
      this._frame.style.minWidth = frameMinWidth;
    }

    let boxMinWidth = `calc(${
      boxHorizontalBorder + frameHorizontalMargin
    }px + ${frameMinWidth})`;

    // Temporary fix to allow parent chrome to collapse properly to min width.
    // See Bug 1658722.
    if (this._window.isChromeWindow) {
      boxMinWidth = `min(80vw, ${boxMinWidth})`;
    }
    this._box.style.minWidth = boxMinWidth;

    this.resizeVertically();

    this._overlay.dispatchEvent(
      new CustomEvent("dialogopen", {
        bubbles: true,
        detail: { dialog: this },
      })
    );
    this._overlay.style.visibility = "inherit";
    this._overlay.style.opacity = ""; // XXX: focus hack continued from _onContentLoaded

    if (this._box.getAttribute("resizable") == "true") {
      this._onResize = this._onResize.bind(this);
      this._resizeObserver = new this._window.MutationObserver(this._onResize);
      this._resizeObserver.observe(this._box, { attributes: true });
    }

    this._trapFocus();

    this._resizeCallback?.({
      title: this._titleElement,
      frame: this._frame,
    });
  },

  resizeVertically() {
    let docEl = this._frame.contentDocument.documentElement;
    let getDocHeight = () => {
      let { scrollHeight } = docEl.ownerDocument.body || docEl;
      // We need to convert em to px because an em value from the dialog window could
      // translate to something else in the host window, as font sizes may vary.
      return this._emToPx(docEl.style.height) || scrollHeight + "px";
    };

    // If the title bar is disabled (not in the template),
    // set its height to 0 for the calculation.
    let titleBarHeight = 0;
    if (this._titleBar) {
      titleBarHeight =
        this._titleBar.clientHeight +
        parseFloat(
          this._window.getComputedStyle(this._titleBar).borderBottomWidth
        );
    }

    let boxVerticalBorder =
      2 * parseFloat(this._window.getComputedStyle(this._box).borderTopWidth);
    let frameVerticalMargin =
      2 * parseFloat(this._window.getComputedStyle(this._frame).marginTop);

    // The difference between the frame and box shouldn't change, either:
    let boxRect = this._box.getBoundingClientRect();
    let frameRect = this._frame.getBoundingClientRect();
    let frameSizeDifference =
      frameRect.top - boxRect.top + (boxRect.bottom - frameRect.bottom);

    let contentPane =
      this._frame.contentDocument.querySelector(".contentPane") ||
      this._frame.contentDocument.querySelector("dialog");

    let sizeTo = this._box.getAttribute("sizeto");
    if (["available", "limitheight"].includes(sizeTo)) {
      if (sizeTo == "limitheight") {
        this._overlay.style.setProperty("--doc-height-px", getDocHeight());
        contentPane?.classList.add("sizeDetermined");
      } else {
        if (docEl.style.maxHeight) {
          this._box.style.setProperty(
            "--box-max-height-requested",
            this._emToPx(docEl.style.maxHeight)
          );
        }
        // Inform the CSS of the toolbar height so the bottom padding can be
        // correctly calculated.
        this._box.style.setProperty("--box-top-px", `${boxRect.top}px`);
      }
      return;
    }

    // Now do the same but for the height. We need to do this afterwards because otherwise
    // XUL assumes we'll optimize for height and gives us "wrong" values which then are no
    // longer correct after we set the width:
    let frameMinHeight = getDocHeight();
    let frameHeight = docEl.getAttribute("height")
      ? docEl.getAttribute("height") + "px"
      : frameMinHeight;

    // Now check if the frame height we calculated is possible at this window size,
    // accounting for titlebar, padding/border and some spacing.
    let frameOverhead = frameSizeDifference + titleBarHeight;
    let maxHeight = this._window.innerHeight - frameOverhead;
    // Do this with a frame height in pixels...
    if (!frameHeight.endsWith("px")) {
      console.error(
        "This dialog (",
        this._frame.contentWindow.location.href,
        ") set a height in non-px-non-em units ('",
        frameHeight,
        "'), " +
          "which is likely to lead to bad sizing in in-content preferences. " +
          "Please consider changing this."
      );
    }

    if (
      parseFloat(frameMinHeight) > maxHeight ||
      parseFloat(frameHeight) > maxHeight
    ) {
      // If the height is bigger than that of the window, we should let the
      // contents scroll. The class is set on the "dialog" element, unless a
      // content pane exists, which is usually the case when the "window"
      // element is used to implement the subdialog instead.
      frameMinHeight = maxHeight + "px";
      // There also instances where the subdialog is neither implemented using
      // a content pane, nor a <dialog> (such as manageAddresses.xhtml)
      // so make sure to check that we actually got a contentPane before we
      // use it.
      contentPane?.classList.add("doScroll");
    }

    this._overlay.style.setProperty("--subdialog-inner-height", frameHeight);
    this._frame.style.height = `min(
      calc(100vh - ${frameOverhead}px),
      var(--subdialog-inner-height, ${frameHeight})
    )`;
    this._box.style.minHeight = `calc(
      ${boxVerticalBorder + titleBarHeight + frameVerticalMargin}px +
      ${frameMinHeight}
    )`;
  },

  /**
   * Helper for converting em to px because an em value from the dialog window could
   * translate to something else in the host window, as font sizes may vary.
   *
   * @param {String} val
   *                 A CSS length value.
   * @return {String} The converted CSS length value, or the original value if
   *                  no conversion took place.
   */
  _emToPx(val) {
    if (val && val.endsWith("em")) {
      let { fontSize } = this.frameContentWindow.getComputedStyle(
        this._frame.contentDocument.documentElement
      );
      return parseFloat(val) * parseFloat(fontSize) + "px";
    }
    return val;
  },

  _onResize(mutations) {
    let frame = this._frame;
    // The width and height styles are needed for the initial
    // layout of the frame, but afterward they need to be removed
    // or their presence will restrict the contents of the <browser>
    // from resizing to a smaller size.
    frame.style.removeProperty("width");
    frame.style.removeProperty("height");

    let docEl = frame.contentDocument.documentElement;
    let persistedAttributes = docEl.getAttribute("persist");
    if (
      !persistedAttributes ||
      (!persistedAttributes.includes("width") &&
        !persistedAttributes.includes("height"))
    ) {
      return;
    }

    for (let mutation of mutations) {
      if (mutation.attributeName == "width") {
        docEl.setAttribute("width", docEl.scrollWidth);
      } else if (mutation.attributeName == "height") {
        docEl.setAttribute("height", docEl.scrollHeight);
      }
    }
  },

  _onDialogClosing(aEvent) {
    this._frame.contentWindow.removeEventListener("dialogclosing", this);
    this._closingEvent = aEvent;
  },

  _onKeyDown(aEvent) {
    // Close on ESC key if target is SubDialog
    // If we're in the parent window, we need to check if the SubDialogs
    // frame is targeted, so we don't close the wrong dialog.
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE && !aEvent.defaultPrevented) {
      if (
        (this._window.isChromeWindow && aEvent.currentTarget == this._box) ||
        (!this._window.isChromeWindow && aEvent.currentTarget == this._window)
      ) {
        // Prevent ESC on SubDialog from cancelling page load (Bug 1665339).
        aEvent.preventDefault();
        this._frame.contentWindow.close();
        return;
      }
    }

    if (
      this._window.isChromeWindow ||
      aEvent.keyCode != aEvent.DOM_VK_TAB ||
      aEvent.ctrlKey ||
      aEvent.altKey ||
      aEvent.metaKey
    ) {
      return;
    }

    let fm = Services.focus;

    let isLastFocusableElement = el => {
      // XXXgijs unfortunately there is no way to get the last focusable element without asking
      // the focus manager to move focus to it.
      let rv =
        el ==
        fm.moveFocus(this._frame.contentWindow, null, fm.MOVEFOCUS_LAST, 0);
      fm.setFocus(el, 0);
      return rv;
    };

    let forward = !aEvent.shiftKey;
    // check if focus is leaving the frame (incl. the close button):
    if (
      (aEvent.target == this._closeButton && !forward) ||
      (isLastFocusableElement(aEvent.originalTarget) && forward)
    ) {
      aEvent.preventDefault();
      aEvent.stopImmediatePropagation();

      let parentWin = this._window.docShell.chromeEventHandler.ownerGlobal;
      if (forward) {
        fm.moveFocus(parentWin, null, fm.MOVEFOCUS_FIRST, fm.FLAG_BYKEY);
      } else {
        // Somehow, moving back 'past' the opening doc is not trivial. Cheat by doing it in 2 steps:
        fm.moveFocus(this._window, null, fm.MOVEFOCUS_ROOT, fm.FLAG_BYKEY);
        fm.moveFocus(parentWin, null, fm.MOVEFOCUS_BACKWARD, fm.FLAG_BYKEY);
      }
    }
  },

  _onParentWinFocus(aEvent) {
    // Explicitly check for the focus target of |window| to avoid triggering this when the window
    // is refocused
    if (
      this._closeButton &&
      aEvent.target != this._closeButton &&
      aEvent.target != this._window
    ) {
      this._closeButton.focus();
    }
  },

  /**
   * Setup dialog event listeners.
   * @param {Boolean} [includeLoad] - Whether to register load/unload listeners.
   */
  _addDialogEventListeners(includeLoad = true) {
    if (this._window.isChromeWindow) {
      // Only register an event listener if we have a title to show.
      if (this._titleBar) {
        this._frame.addEventListener("DOMTitleChanged", this, true);
      }

      if (includeLoad) {
        this._window.addEventListener("unload", this, true);
      }
    } else {
      let chromeBrowser = this._window.docShell.chromeEventHandler;

      if (includeLoad) {
        // For content windows we listen for unload of the browser
        chromeBrowser.addEventListener("unload", this, true);
      }

      if (this._titleBar) {
        chromeBrowser.addEventListener("DOMTitleChanged", this, true);
      }
    }

    // Make the close button work.
    this._closeButton?.addEventListener("command", this);

    if (includeLoad) {
      // DOMFrameContentLoaded only fires on the top window
      this._window.addEventListener("DOMFrameContentLoaded", this, true);

      // Wait for the stylesheets injected during DOMContentLoaded to load before showing the dialog
      // otherwise there is a flicker of the stylesheet applying.
      this._frame.addEventListener("load", this, true);
    }

    // Ensure we get <esc> keypresses even if nothing in the subdialog is focusable
    // (happens on OS X when only text inputs and lists are focusable, and
    //  the subdialog only has checkboxes/radiobuttons/buttons)
    if (!this._window.isChromeWindow) {
      this._window.addEventListener("keydown", this, true);
    }

    this._overlay.addEventListener("click", this, true);
  },

  /**
   * Remove dialog event listeners.
   * @param {Boolean} [includeLoad] - Whether to remove load/unload listeners.
   */
  _removeDialogEventListeners(includeLoad = true) {
    if (this._window.isChromeWindow) {
      this._frame.removeEventListener("DOMTitleChanged", this, true);

      if (includeLoad) {
        this._window.removeEventListener("unload", this, true);
      }
    } else {
      let chromeBrowser = this._window.docShell.chromeEventHandler;
      if (includeLoad) {
        chromeBrowser.removeEventListener("unload", this, true);
      }

      chromeBrowser.removeEventListener("DOMTitleChanged", this, true);
    }

    this._closeButton?.removeEventListener("command", this);

    if (includeLoad) {
      this._window.removeEventListener("DOMFrameContentLoaded", this, true);
      this._frame.removeEventListener("load", this, true);
      this._frame.contentWindow.removeEventListener("dialogclosing", this);
    }

    this._window.removeEventListener("keydown", this, true);

    this._overlay.removeEventListener("click", this, true);

    if (this._resizeObserver) {
      this._resizeObserver.disconnect();
      this._resizeObserver = null;
    }

    this._untrapFocus();
  },

  /**
   * Focus the dialog content.
   * If the embedded document defines a custom focus handler it will be called.
   * Otherwise we will focus the first focusable element in the content window.
   * @param {boolean} [isInitialFocus] - Whether the dialog is focused for the
   * first time after opening.
   */
  focus(isInitialFocus = false) {
    // If the content window has its own focus logic, hand off the focus call.
    let focusHandler = this._frame?.contentDocument?.subDialogSetDefaultFocus;
    if (focusHandler) {
      focusHandler(isInitialFocus);
      return;
    }
    // Handle focus ourselves. Try to move the focus to the first element in
    // the content window.
    let fm = Services.focus;

    // We're intentionally hiding the focus ring here for now per bug 1704882,
    // but we aim to have a better fix that retains the focus ring for users
    // that had brought up the dialog by keyboard in bug 1708261.
    let focusedElement = fm.moveFocus(
      this._frame.contentWindow,
      null,
      fm.MOVEFOCUS_FIRST,
      fm.FLAG_NOSHOWRING
    );
    if (!focusedElement) {
      // Ensure the focus is pulled out of the content document even if there's
      // nothing focusable in the dialog.
      this._frame.focus();
    }
  },

  _trapFocus() {
    // Attach a system event listener so the dialog can cancel keydown events.
    // See Bug 1669990.
    this._box.addEventListener("keydown", this, { mozSystemGroup: true });
    this._closeButton?.addEventListener("keydown", this);

    if (!this._window.isChromeWindow) {
      this._window.addEventListener("focus", this, true);
    }
  },

  _untrapFocus() {
    this._box.removeEventListener("keydown", this, { mozSystemGroup: true });
    this._closeButton?.removeEventListener("keydown", this);
    this._window.removeEventListener("focus", this, true);
  },
};

/**
 * Manages multiple SubDialogs in a dialog stack element.
 */
export class SubDialogManager {
  /**
   * @param {Object} options - Dialog manager options.
   * @param {DOMNode} options.dialogStack - Container element for all dialogs
   * this instance manages.
   * @param {DOMNode} options.dialogTemplate - Element to use as template for
   * constructing new dialogs.
   * @param {Number} [options.orderType] - Whether dialogs should be ordered as
   * a stack or a queue.
   * @param {Boolean} [options.allowDuplicateDialogs] - Whether to allow opening
   * duplicate dialogs (same URI) at the same time. If disabled, opening a
   * dialog with the same URI as an existing dialog will be a no-op.
   * @param {Object} options.dialogOptions - Options passed to every
   * SubDialog instance.
   * @see {@link SubDialog} for a list of dialog options.
   */
  constructor({
    dialogStack,
    dialogTemplate,
    orderType = SubDialogManager.ORDER_STACK,
    allowDuplicateDialogs = false,
    dialogOptions,
  }) {
    /**
     * New dialogs are pushed to the end of the _dialogs array.
     * Depending on the orderType either the last element (stack) or the first
     * element (queue) in the array will be the top and visible.
     * @type {SubDialog[]}
     */
    this._dialogs = [];
    this._dialogStack = dialogStack;
    this._dialogTemplate = dialogTemplate;
    this._topLevelPrevActiveElement = null;
    this._orderType = orderType;
    this._allowDuplicateDialogs = allowDuplicateDialogs;
    this._dialogOptions = dialogOptions;

    this._preloadDialog = new SubDialog({
      template: this._dialogTemplate,
      parentElement: this._dialogStack,
      id: SubDialogManager._nextDialogID++,
      dialogOptions: this._dialogOptions,
    });
  }

  /**
   * Get the dialog which is currently on top. This depends on whether the
   * dialogs are in a stack or a queue.
   */
  get _topDialog() {
    if (!this._dialogs.length) {
      return undefined;
    }
    if (this._orderType === SubDialogManager.ORDER_STACK) {
      return this._dialogs[this._dialogs.length - 1];
    }
    return this._dialogs[0];
  }

  open(
    aURL,
    {
      features,
      closingCallback,
      closedCallback,
      allowDuplicateDialogs,
      sizeTo,
      hideContent,
    } = {},
    ...aParams
  ) {
    let allowDuplicates =
      allowDuplicateDialogs != null
        ? allowDuplicateDialogs
        : this._allowDuplicateDialogs;
    // If we're already open/opening on this URL, do nothing.
    if (
      !allowDuplicates &&
      this._dialogs.some(dialog => dialog._openedURL == aURL)
    ) {
      return undefined;
    }

    let doc = this._dialogStack.ownerDocument;

    // For dialog stacks, remember the last active element before opening the
    // next dialog. This allows us to restore focus on dialog close.
    if (
      this._orderType === SubDialogManager.ORDER_STACK &&
      this._dialogs.length
    ) {
      this._topDialog._prevActiveElement = doc.activeElement;
    }

    if (!this._dialogs.length) {
      // When opening the first dialog, show the dialog stack.
      this._dialogStack.hidden = false;
      this._dialogStack.classList.remove("temporarilyHidden");
      this._topLevelPrevActiveElement = doc.activeElement;
    }

    // Consumers may pass this flag to make the dialog overlay background opaque,
    // effectively hiding the content behind it. For example,
    // this is used by the prompt code to prevent certain http authentication spoofing scenarios.
    if (hideContent) {
      this._preloadDialog._overlay.setAttribute("hideContent", true);
    }
    this._dialogs.push(this._preloadDialog);
    this._preloadDialog.open(
      aURL,
      {
        features,
        closingCallback,
        closedCallback,
        sizeTo,
      },
      ...aParams
    );

    let openedDialog = this._preloadDialog;

    this._preloadDialog = new SubDialog({
      template: this._dialogTemplate,
      parentElement: this._dialogStack,
      id: SubDialogManager._nextDialogID++,
      dialogOptions: this._dialogOptions,
    });

    if (this._dialogs.length == 1) {
      this._ensureStackEventListeners();
    }

    return openedDialog;
  }

  close() {
    this._topDialog.close();
  }

  /**
   * Hides the dialog stack for a specific browser, without actually destroying
   * frames for stuff within it.
   *
   * @param aBrowser - The browser associated with the tab dialog.
   */
  hideDialog(aBrowser) {
    aBrowser.removeAttribute("tabDialogShowing");
    this._dialogStack.classList.add("temporarilyHidden");
  }

  /**
   * Abort open dialogs.
   * @param {function} [filterFn] - Function which should return true for
   * dialogs that should be aborted and false for dialogs that should remain
   * open. Defaults to aborting all dialogs.
   */
  abortDialogs(filterFn = () => true) {
    this._dialogs.filter(filterFn).forEach(dialog => dialog.abort());
  }

  get hasDialogs() {
    if (!this._dialogs.length) {
      return false;
    }
    return this._dialogs.some(dialog => !dialog._isClosing);
  }

  get dialogs() {
    return [...this._dialogs];
  }

  focusTopDialog() {
    this._topDialog?.focus();
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "dialogopen": {
        this._onDialogOpen(aEvent.detail.dialog);
        break;
      }
      case "dialogclose": {
        this._onDialogClose(aEvent.detail.dialog);
        break;
      }
    }
  }

  _onDialogOpen(dialog) {
    let lowerDialogs = [];
    if (dialog == this._topDialog) {
      dialog.focus(true);
    } else {
      // Opening dialog is not on top, hide it
      lowerDialogs.push(dialog);
    }

    // For stack order, hide the previous top
    if (
      this._dialogs.length &&
      this._orderType === SubDialogManager.ORDER_STACK
    ) {
      let index = this._dialogs.indexOf(dialog);
      if (index > 0) {
        lowerDialogs.push(this._dialogs[index - 1]);
      }
    }

    lowerDialogs.forEach(d => {
      if (d._overlay.hasAttribute("topmost")) {
        d._overlay.removeAttribute("topmost");
        d._removeDialogEventListeners(false);
      }
    });
  }

  _onDialogClose(dialog) {
    this._dialogs.splice(this._dialogs.indexOf(dialog), 1);

    if (this._topDialog) {
      // The prevActiveElement is only set for stacked dialogs
      if (this._topDialog._prevActiveElement) {
        this._topDialog._prevActiveElement.focus();
      } else {
        this._topDialog.focus(true);
      }
      this._topDialog._overlay.setAttribute("topmost", true);
      this._topDialog._addDialogEventListeners(false);
      this._dialogStack.hidden = false;
      this._dialogStack.classList.remove("temporarilyHidden");
    } else {
      // We have closed the last dialog, do cleanup.
      this._topLevelPrevActiveElement.focus();
      this._dialogStack.hidden = true;
      this._removeStackEventListeners();
    }
  }

  _ensureStackEventListeners() {
    this._dialogStack.addEventListener("dialogopen", this);
    this._dialogStack.addEventListener("dialogclose", this);
  }

  _removeStackEventListeners() {
    this._dialogStack.removeEventListener("dialogopen", this);
    this._dialogStack.removeEventListener("dialogclose", this);
  }
}

// Used for the SubDialogManager orderType option.
SubDialogManager.ORDER_STACK = 0;
SubDialogManager.ORDER_QUEUE = 1;

SubDialogManager._nextDialogID = 0;
