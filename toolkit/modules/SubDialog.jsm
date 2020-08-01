/* - This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this file,
   - You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SubDialog", "SubDialogManager"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

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
 * @param {Boolean} [dialogOptions.reuseDialog] - If false, remove the SubDialog
 * from the DOM when closed. Defaults to true.
 */
function SubDialog({
  template,
  parentElement,
  id,
  dialogOptions: {
    styleSheets = [],
    consumeOutsideClicks = true,
    resizeCallback,
    reuseDialog = true,
  } = {},
}) {
  this._id = id;

  this._injectedStyleSheets = this._injectedStyleSheets.concat(styleSheets);
  this._consumeOutsideClicks = consumeOutsideClicks;
  this._reuseDialog = reuseDialog;
  this._resizeCallback = resizeCallback;
  this._overlay = template.cloneNode(true);
  this._box = this._overlay.querySelector(".dialogBox");
  this._titleBar = this._overlay.querySelector(".dialogTitleBar");
  this._titleElement = this._overlay.querySelector(".dialogTitle");
  this._closeButton = this._overlay.querySelector(".dialogClose");
  this._frame = this._overlay.querySelector(".dialogFrame");

  this._overlay.id = `dialogOverlay-${id}`;
  this._frame.setAttribute("name", `dialogFrame-${id}`);
  this._frameCreated = new Promise(resolve => {
    this._frame.addEventListener("load", resolve, {
      once: true,
      capture: true,
    });
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

  get _window() {
    return this._overlay?.ownerGlobal;
  },

  get _chromeEventHandler() {
    if (!this._window) {
      return null;
    }
    if (this._window.isChromeWindow) {
      return this._frame;
    }
    return this._window.docShell.chromeEventHandler;
  },

  updateTitle(aEvent) {
    if (aEvent.target != this._frame.contentDocument) {
      return;
    }
    this._titleElement.textContent = this._frame.contentDocument.title;
  },

  injectXMLStylesheet(aStylesheetURL) {
    const doc = this._frame.contentDocument;
    if ([...doc.styleSheets].find(s => s.href === aStylesheetURL)) {
      return;
    }
    let contentStylesheet = doc.createProcessingInstruction(
      "xml-stylesheet",
      'href="' + aStylesheetURL + '" type="text/css"'
    );
    doc.insertBefore(contentStylesheet, doc.documentElement);
  },

  async open(aURL, aFeatures = null, aParams = null, aClosingCallback = null) {
    // Create a promise so consumers can tell when we're done setting up.
    this._dialogReady = new Promise(resolve => {
      this._resolveDialogReady = resolve;
    });

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

    // If the parent is chrome we also need open the dialog as chrome, otherwise
    // the openDialog call will fail.
    let features = `resizable,dialog=no,centerscreen,chrome=${
      this._window?.isChromeWindow ? "yes" : "no"
    }`;
    if (aFeatures) {
      features = `${aFeatures},${features}`;
    }

    let dialog = this._window.openDialog(
      aURL,
      `dialogFrame-${this._id}`,
      features,
      aParams
    );
    if (aClosingCallback) {
      this._closingCallback = aClosingCallback.bind(dialog);
    }

    this._closingEvent = null;
    this._isClosing = false;
    this._openedURL = aURL;

    features = features.replace(/,/g, "&");
    let featureParams = new URLSearchParams(features.toLowerCase());
    this._box.setAttribute(
      "resizable",
      featureParams.has("resizable") &&
        featureParams.get("resizable") != "no" &&
        featureParams.get("resizable") != "0"
    );
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
        Cu.reportError(ex);
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
    this._box.style.removeProperty("min-height");
    this._box.style.removeProperty("min-width");

    this._overlay.dispatchEvent(
      new CustomEvent("dialogclose", {
        bubbles: true,
        detail: { dialog: this },
      })
    );

    this._window.setTimeout(() => {
      // Unload the dialog after the event listeners run so that the load of about:blank isn't
      // cancelled by the ESC <key>.
      let onBlankLoad = e => {
        if (this._frame.contentWindow.location.href == "about:blank") {
          this._frame.removeEventListener("load", onBlankLoad, true);
          // We're now officially done closing, so update the state to reflect that.
          this._openedURL = null;
          this._isClosing = false;

          if (!this._reuseDialog) {
            this._overlay.remove();
          }

          this._resolveClosePromise();
        }
      };

      // Depending on the context of the frame, we need either a system caller
      // (chrome) or a null principal (content) to load a new URI.
      let triggeringPrincipal;
      if (this._window.isChromeWindow) {
        triggeringPrincipal = this._window.document.nodePrincipal;
      } else {
        triggeringPrincipal = Services.scriptSecurityManager.createNullPrincipal(
          {}
        );
      }

      this._frame.addEventListener("load", onBlankLoad, true);
      this._frame.loadURI("about:blank", {
        triggeringPrincipal,
      });
    }, 0);
  },

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        // Close the dialog if the user clicked the overlay background, just
        // like when the user presses the ESC key (case "command" below).
        if (this._consumeOutsideClicks && aEvent.target === this._overlay) {
          this._frame.contentWindow.close();
        }
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
    if (aEvent.target.location.href == this._openedURL) {
      this._frame.contentWindow.close();
    }
  },

  _onContentLoaded(aEvent) {
    if (
      aEvent.target != this._frame ||
      aEvent.target.contentWindow.location == "about:blank"
    ) {
      return;
    }

    for (let styleSheetURL of this._injectedStyleSheets) {
      this.injectXMLStylesheet(styleSheetURL);
    }

    // Provide the ability for the dialog to know that it is being loaded "in-content".
    for (let dialog of this._frame.contentDocument.querySelectorAll("dialog")) {
      dialog.setAttribute("subdialog", "true");
    }

    this._frame.contentWindow.addEventListener("dialogclosing", this);

    let oldResizeBy = this._frame.contentWindow.resizeBy;
    this._frame.contentWindow.resizeBy = (resizeByWidth, resizeByHeight) => {
      // Only handle resizeByHeight currently.
      let frameHeight = this._frame.clientHeight;
      let boxMinHeight = parseFloat(
        this._window.getComputedStyle(this._box).minHeight,
        10
      );

      this._frame.style.height = frameHeight + resizeByHeight + "px";
      this._box.style.minHeight = boxMinHeight + resizeByHeight + "px";

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
      if (!closingEvent) {
        closingEvent = new CustomEvent("dialogclosing", {
          bubbles: true,
          detail: { button: null },
        });

        this._frame.contentWindow.dispatchEvent(closingEvent);
      }

      this.close(closingEvent);
      oldClose.call(this._frame.contentWindow);
    };

    // XXX: Hack to make focus during the dialog's load functions work. Make the element visible
    // sooner in DOMContentLoaded but mostly invisible instead of changing visibility just before
    // the dialog's load event.
    this._overlay.style.visibility = "visible";
    this._overlay.style.opacity = "0.01";
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
    let frameMinWidth = docEl.style.width || scrollWidth + "px";
    let frameWidth = docEl.getAttribute("width")
      ? docEl.getAttribute("width") + "px"
      : frameMinWidth;
    this._frame.style.width = frameWidth;
    this._box.style.minWidth =
      "calc(" +
      (boxHorizontalBorder + frameHorizontalMargin) +
      "px + " +
      frameMinWidth +
      ")";

    this.resizeVertically();

    this._overlay.dispatchEvent(
      new CustomEvent("dialogopen", {
        bubbles: true,
        detail: { dialog: this },
      })
    );
    this._overlay.style.visibility = "visible";
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

    // Now do the same but for the height. We need to do this afterwards because otherwise
    // XUL assumes we'll optimize for height and gives us "wrong" values which then are no
    // longer correct after we set the width:
    let { scrollHeight } = docEl.ownerDocument.body || docEl;
    let frameMinHeight = docEl.style.height || scrollHeight + "px";
    let frameHeight = docEl.getAttribute("height")
      ? docEl.getAttribute("height") + "px"
      : frameMinHeight;

    // Now check if the frame height we calculated is possible at this window size,
    // accounting for titlebar, padding/border and some spacing.
    let maxHeight = this._window.innerHeight - frameSizeDifference - 30;
    // Do this with a frame height in pixels...
    let comparisonFrameHeight;
    if (frameHeight.endsWith("em")) {
      let fontSize = parseFloat(
        this._window.getComputedStyle(this._frame).fontSize
      );
      comparisonFrameHeight = parseFloat(frameHeight, 10) * fontSize;
    } else if (frameHeight.endsWith("px")) {
      comparisonFrameHeight = parseFloat(frameHeight, 10);
    } else {
      Cu.reportError(
        "This dialog (" +
          this._frame.contentWindow.location.href +
          ") " +
          "set a height in non-px-non-em units ('" +
          frameHeight +
          "'), " +
          "which is likely to lead to bad sizing in in-content preferences. " +
          "Please consider changing this."
      );
      comparisonFrameHeight = parseFloat(frameHeight);
    }

    if (comparisonFrameHeight > maxHeight) {
      // If the height is bigger than that of the window, we should let the
      // contents scroll. The class is set on the "dialog" element, unless a
      // content pane exists, which is usually the case when the "window"
      // element is used to implement the subdialog instead.
      frameHeight = maxHeight + "px";
      frameMinHeight = maxHeight + "px";
      let contentPane =
        this._frame.contentDocument.querySelector(".contentPane") ||
        this._frame.contentDocument.querySelector("dialog");
      if (contentPane) {
        // There are also instances where the subdialog is neither implemented
        // using a content pane, nor a <dialog> (such as manageAddresses.xhtml)
        // so make sure to check that we actually got a contentPane before we
        // use it.
        contentPane.classList.add("doScroll");
      }
    }

    this._frame.style.height = frameHeight;
    this._box.style.minHeight =
      "calc(" +
      (boxVerticalBorder + titleBarHeight + frameVerticalMargin) +
      "px + " +
      frameMinHeight +
      ")";
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
    if (
      aEvent.currentTarget == this._window &&
      aEvent.keyCode == aEvent.DOM_VK_ESCAPE &&
      !aEvent.defaultPrevented
    ) {
      this.close(aEvent);
      return;
    }
    if (
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
      let parentWin = this._chromeEventHandler.ownerGlobal;
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

  _addDialogEventListeners() {
    // Make the close button work.
    this._closeButton?.addEventListener("command", this);

    let chromeEventHandler = this._chromeEventHandler;

    // Only register an event listener if we have a title to show.
    if (this._titleBar) {
      chromeEventHandler.addEventListener("DOMTitleChanged", this, true);
    }

    // DOMFrameContentLoaded only fires on the top window
    this._window.addEventListener("DOMFrameContentLoaded", this, true);

    // Wait for the stylesheets injected during DOMContentLoaded to load before showing the dialog
    // otherwise there is a flicker of the stylesheet applying.
    this._frame.addEventListener("load", this, true);

    chromeEventHandler.addEventListener("unload", this, true);

    // Ensure we get <esc> keypresses even if nothing in the subdialog is focusable
    // (happens on OS X when only text inputs and lists are focusable, and
    //  the subdialog only has checkboxes/radiobuttons/buttons)
    this._window.addEventListener("keydown", this, true);

    this._overlay.addEventListener("click", this, true);
  },

  _removeDialogEventListeners() {
    let chromeEventHandler = this._chromeEventHandler;
    chromeEventHandler.removeEventListener("DOMTitleChanged", this, true);
    chromeEventHandler.removeEventListener("unload", this, true);

    this._closeButton?.removeEventListener("command", this);

    this._window.removeEventListener("DOMFrameContentLoaded", this, true);
    this._frame.removeEventListener("load", this, true);
    this._frame.contentWindow.removeEventListener("dialogclosing", this);
    this._window.removeEventListener("keydown", this, true);

    this._overlay.removeEventListener("click", this, true);

    if (this._resizeObserver) {
      this._resizeObserver.disconnect();
      this._resizeObserver = null;
    }
    this._untrapFocus();
  },

  _trapFocus() {
    let fm = Services.focus;
    fm.moveFocus(this._frame.contentWindow, null, fm.MOVEFOCUS_FIRST, 0);
    this._frame.contentDocument.addEventListener("keydown", this, true);
    this._closeButton?.addEventListener("keydown", this);

    this._window.addEventListener("focus", this, true);
  },

  _untrapFocus() {
    this._frame.contentDocument.removeEventListener("keydown", this, true);
    this._closeButton?.removeEventListener("keydown", this);
    this._window.removeEventListener("focus", this, true);
  },
};

/**
 * Manages multiple SubDialogs in a dialog stack element.
 */
class SubDialogManager {
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
    this._nextDialogID = 0;
    this._topLevelPrevActiveElement = null;
    this._orderType = orderType;
    this._allowDuplicateDialogs = allowDuplicateDialogs;
    this._dialogOptions = dialogOptions;

    this._preloadDialog = new SubDialog({
      template: this._dialogTemplate,
      parentElement: this._dialogStack,
      id: this._nextDialogID++,
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

  open(aURL, aFeatures = null, aParams = null, aClosingCallback = null) {
    // If we're already open/opening on this URL, do nothing.
    if (!this._allowDuplicateDialogs && this._topDialog?._openedURL == aURL) {
      return;
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
      this._topLevelPrevActiveElement = doc.activeElement;
    }

    this._preloadDialog.open(aURL, aFeatures, aParams, aClosingCallback);
    this._dialogs.push(this._preloadDialog);
    this._preloadDialog = new SubDialog({
      template: this._dialogTemplate,
      parentElement: this._dialogStack,
      id: this._nextDialogID++,
      dialogOptions: this._dialogOptions,
    });

    if (this._dialogs.length == 1) {
      this._ensureStackEventListeners();
    }
  }

  close() {
    this._topDialog.close();
  }

  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "dialogopen": {
        this._onDialogOpen();
        break;
      }
      case "dialogclose": {
        this._onDialogClose(aEvent.detail.dialog);
        break;
      }
    }
  }

  _onDialogOpen() {
    // The first dialog is on top in both QUEUE and STACK order, and since it
    // already has the topmost attribute and the event listeners attached, we
    // don't have to adjust anything.
    if (this._dialogs.length === 1) {
      return;
    }

    let lowerDialog;
    if (this._orderType === SubDialogManager.ORDER_STACK) {
      // Stack dialog on top => hide previous top dialog.
      lowerDialog = this._dialogs[this._dialogs.length - 2];
    } else {
      // Enqueue dialog => hide the new dialog.
      lowerDialog = this._dialogs[this._dialogs.length - 1];
    }

    lowerDialog._overlay.removeAttribute("topmost");
    lowerDialog._removeDialogEventListeners();
  }

  _onDialogClose(dialog) {
    if (this._topDialog == dialog) {
      // XXX: When a top-most dialog is closed, we reuse the closed dialog and
      //      remove the preloadDialog. This is a temporary solution before we
      //      rewrite all the test cases in Bug 1359023.
      this._preloadDialog._overlay.remove();
      if (this._orderType === SubDialogManager.ORDER_STACK) {
        this._preloadDialog = this._dialogs.pop();
      } else {
        this._preloadDialog = this._dialogs.shift();
      }
    } else {
      dialog._overlay.remove();
      this._dialogs.splice(this._dialogs.indexOf(dialog), 1);
    }

    if (this._topDialog) {
      // The prevActiveElement is only set for stacked dialogs
      this._topDialog._prevActiveElement?.focus();
      this._topDialog._overlay.setAttribute("topmost", true);
      this._topDialog._addDialogEventListeners();
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
