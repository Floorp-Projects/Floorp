/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["FinderHighlighter"];

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Color", "resource://gre/modules/Color.jsm");
XPCOMUtils.defineLazyGetter(this, "kDebug", () => {
  const kDebugPref = "findbar.modalHighlight.debug";
  return Services.prefs.getPrefType(kDebugPref) && Services.prefs.getBoolPref(kDebugPref);
});

const kModalHighlightRepaintFreqMs = 10;
const kHighlightAllPref = "findbar.highlightAll";
const kModalHighlightPref = "findbar.modalHighlight";
const kFontPropsCSS = ["color", "font-family", "font-kerning", "font-size",
  "font-size-adjust", "font-stretch", "font-variant", "font-weight", "letter-spacing",
  "text-emphasis", "text-orientation", "text-transform", "word-spacing"];
const kFontPropsCamelCase = kFontPropsCSS.map(prop => {
  let parts = prop.split("-");
  return parts.shift() + parts.map(part => part.charAt(0).toUpperCase() + part.slice(1)).join("");
});
const kRGBRE = /^rgba?\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*/i
// This uuid is used to prefix HTML element IDs and classNames in order to make
// them unique and hard to clash with IDs and classNames content authors come up
// with, since the stylesheet for modal highlighting is inserted as an agent-sheet
// in the active HTML document.
const kModalIdPrefix = "cedee4d0-74c5-4f2d-ab43-4d37c0f9d463";
const kModalOutlineId = kModalIdPrefix + "-findbar-modalHighlight-outline";
const kModalStyle = `
.findbar-modalHighlight-outline {
  position: absolute;
  background: linear-gradient(to bottom, #f1ee00, #edcc00);
  border: 1px solid #f5e600;
  border-radius: 3px;
  box-shadow: 0px 2px 3px rgba(0,0,0,.8);
  color: #000;
  margin-top: -3px;
  margin-inline-end: 0;
  margin-bottom: 0;
  margin-inline-start: -3px;
  padding-top: 2px;
  padding-inline-end: 2px;
  padding-bottom: 0;
  padding-inline-start: 4px;
  pointer-events: none;
  z-index: 2;
}

.findbar-modalHighlight-outline.findbar-debug {
  z-index: 2147483647;
}

.findbar-modalHighlight-outline[grow] {
  transform: scaleX(1.5) scaleY(1.5)
}

.findbar-modalHighlight-outline[hidden] {
  opacity: 0;
  display: -moz-box;
}

.findbar-modalHighlight-outline:not([disable-transitions]) {
  transition-property: opacity, transform, top, left;
  transition-duration: 50ms;
  transition-timing-function: linear;
}

.findbar-modalHighlight-outlineMask {
  background: #000;
  mix-blend-mode: multiply;
  opacity: .2;
  position: absolute;
  z-index: 1;
}

.findbar-modalHighlight-outlineMask.findbar-debug {
  z-index: 2147483646;
  top: 0;
  left: 0;
}

.findbar-modalHighlight-outlineMask[brighttext] {
  background: #fff;
}

.findbar-modalHighlight-rect {
  background: #fff;
  border: 1px solid #666;
  position: absolute;
}

.findbar-modalHighlight-outlineMask[brighttext] > .findbar-modalHighlight-rect {
  background: #000;
}`;

function mockAnonymousContentNode(domNode) {
  return {
    setTextContentForElement(id, text) {
      (domNode.querySelector("#" + id) || domNode).textContent = text;
    },
    getAttributeForElement(id, attrName) {
      let node = domNode.querySelector("#" + id) || domNode;
      if (!node.hasAttribute(attrName))
        return undefined;
      return node.getAttribute(attrName);
    },
    setAttributeForElement(id, attrName, attrValue) {
      (domNode.querySelector("#" + id) || domNode).setAttribute(attrName, attrValue);
    },
    removeAttributeForElement(id, attrName) {
      let node = domNode.querySelector("#" + id) || domNode;
      if (!node.hasAttribute(attrName))
        return;
      node.removeAttribute(attrName);
    },
    remove() {
      try {
        domNode.parentNode.removeChild(domNode);
      } catch (ex) {}
    }
  };
}

/**
 * FinderHighlighter class that is used by Finder.jsm to take care of the
 * 'Highlight All' feature, which can highlight all find occurrences in a page.
 *
 * @param {Finder} finder Finder.jsm instance
 */
function FinderHighlighter(finder) {
  this._currentFoundRange = null;
  this._modal = Services.prefs.getBoolPref(kModalHighlightPref);
  this._highlightAll = Services.prefs.getBoolPref(kHighlightAllPref);
  this.finder = finder;
  this.visible = false;
}

FinderHighlighter.prototype = {
  get iterator() {
    if (this._iterator)
      return this._iterator;
    this._iterator = Cu.import("resource://gre/modules/FinderIterator.jsm", null).FinderIterator;
    return this._iterator;
  },

  get modalStyleSheet() {
    if (!this._modalStyleSheet) {
      this._modalStyleSheet = kModalStyle.replace(/(\.|#)findbar-/g,
        "$1" + kModalIdPrefix + "-findbar-");
    }
    return this._modalStyleSheet;
  },

  get modalStyleSheetURI() {
    if (!this._modalStyleSheetURI) {
      this._modalStyleSheetURI = "data:text/css;charset=utf-8," +
        encodeURIComponent(this.modalStyleSheet.replace(/[\n]+/g, " "));
    }
    return this._modalStyleSheetURI;
  },

  /**
   * Notify all registered listeners that the 'Highlight All' operation finished.
   *
   * @param {Boolean} highlight Whether highlighting was turned on
   */
  notifyFinished(highlight) {
    for (let l of this.finder._listeners) {
      try {
        l.onHighlightFinished(highlight);
      } catch (ex) {}
    }
  },

  /**
   * Toggle highlighting all occurrences of a word in a page. This method will
   * be called recursively for each (i)frame inside a page.
   *
   * @param {Booolean} highlight Whether highlighting should be turned on
   * @param {String}   word      Needle to search for and highlight when found
   * @param {Boolean}  linksOnly Only consider nodes that are links for the search
   * @yield {Promise}  that resolves once the operation has finished
   */
  highlight: Task.async(function* (highlight, word, linksOnly) {
    let window = this.finder._getWindow();
    let controller = this.finder._getSelectionController(window);
    let doc = window.document;
    let found = false;

    this.clear();

    if (!controller || !doc || !doc.documentElement) {
      // Without the selection controller,
      // we are unable to (un)highlight any matches
      return found;
    }

    if (highlight) {
      yield this.iterator.start({
        linksOnly, word,
        finder: this.finder,
        onRange: range => {
          this.highlightRange(range, controller, window);
          found = true;
        },
        useCache: true
      });
    } else {
      this.hide(window);
      this.clear();
      this.iterator.reset();

      // Removing the highlighting always succeeds, so return true.
      found = true;
    }

    return found;
  }),

  /**
   * Add a range to the find selection, i.e. highlight it, and if it's inside an
   * editable node, track it.
   *
   * @param {nsIDOMRange}            range      Range object to be highlighted
   * @param {nsISelectionController} controller Selection controller of the
   *                                            document that the range belongs
   *                                            to
   * @param {nsIDOMWindow}           window     Window object, whose DOM tree
   *                                            is being traversed
   */
  highlightRange(range, controller, window) {
    let node = range.startContainer;
    let editableNode = this._getEditableNode(node);
    if (editableNode) {
      controller = editableNode.editor.selectionController;
    }

    if (this._modal) {
      this._modalHighlight(range, controller, window);
    } else {
      let findSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
      findSelection.addRange(range);
    }

    if (editableNode) {
      // Highlighting added, so cache this editor, and hook up listeners
      // to ensure we deal properly with edits within the highlighting
      this._addEditorListeners(editableNode.editor);
    }
  },

  /**
   * If modal highlighting is enabled, show the dimmed background that will overlay
   * the page.
   *
   * @param {nsIDOMWindow} window The dimmed background will overlay this window.
   *                              Optional, defaults to the finder window.
   */
  show(window = null) {
    if (!this._modal || this.visible)
      return;

    this.visible = true;
    window = window || this.finder._getWindow();
    this._maybeCreateModalHighlightNodes(window);
    this._addModalHighlightListeners(window);
  },

  /**
   * Clear all highlighted matches. If modal highlighting is enabled and
   * the outline + dimmed background is currently visible, both will be hidden.
   *
   * @param {nsIDOMWindow} window    The dimmed background will overlay this window.
   *                                 Optional, defaults to the finder window.
   * @param {nsIDOMRange}  skipRange A range that should not be removed from the
   *                                 find selection.
   * @param {nsIDOMEvent}  event     When called from an event handler, this will
   *                                 be the triggering event.
   */
  hide(window = null, skipRange = null, event = null) {
    // Do not hide on anything but a left-click.
    if (event && event.type == "click" && event.button !== 0)
      return;

    window = window || this.finder._getWindow();

    let doc = window.document;
    this._clearSelection(this.finder._getSelectionController(window), skipRange);

    // Next, check our editor cache, for editors belonging to this
    // document
    if (this._editors) {
      for (let x = this._editors.length - 1; x >= 0; --x) {
        if (this._editors[x].document == doc) {
          this._clearSelection(this._editors[x].selectionController, skipRange);
          // We don't need to listen to this editor any more
          this._unhookListenersAtIndex(x);
        }
      }
    }

    if (!this._modal || !this.visible)
      return;

    if (this._modalHighlightOutline)
      this._modalHighlightOutline.setAttributeForElement(kModalOutlineId, "hidden", "true");

    this._removeHighlightAllMask(window);
    this._removeModalHighlightListeners(window);
    delete this._brightText;

    this.visible = false;
  },

  /**
   * Called by the Finder after a find result comes in; update the position and
   * content of the outline to the newly found occurrence.
   * To make sure that the outline covers the found range completely, all the
   * CSS styles that influence the text are copied and applied to the outline.
   *
   * @param {Object} data Dictionary coming from Finder that contains the
   *                      following properties:
   *   {Number}  result        One of the nsITypeAheadFind.FIND_* constants
   *                           indicating the result of a search operation.
   *   {Boolean} findBackwards If TRUE, the search was performed backwards,
   *                           FALSE if forwards.
   *   {Boolean} findAgain     If TRUE, the search was performed using the same
   *                           search string as before.
   *   {String}  linkURL       If a link was hit, this will contain a URL string.
   *   {Rect}    rect          An object with top, left, width and height
   *                           coordinates of the current selection.
   *   {String}  searchString  The string the search was performed with.
   *   {Boolean} storeResult   Indicator if the search string should be stored
   *                           by the consumer of the Finder.
   */
  update(data) {
    let window = this.finder._getWindow();
    let foundRange = this.finder._fastFind.getFoundRange();
    if (!this._modal) {
      if (this._highlightAll) {
        this.hide(window, foundRange);
        let params = this.iterator.params;
        if (params.word)
          this.highlight(true, params.word, params.linksOnly);
      }
      return;
    }

    // Place the match placeholder on top of the current found range.
    if (data.result == Ci.nsITypeAheadFind.FIND_NOTFOUND || !foundRange) {
      this.hide();
      return;
    }

    let outlineNode;
    if (foundRange !== this._currentFoundRange || data.findAgain) {
      this._currentFoundRange = foundRange;

      let textContent = this._getRangeContentArray(foundRange);
      if (!textContent.length) {
        this.hide(window);
        return;
      }

      let rect = foundRange.getBoundingClientRect();
      let fontStyle = this._getRangeFontStyle(foundRange);
      if (typeof this._brightText == "undefined") {
        this._brightText = this._isColorBright(fontStyle.color);
      }

      // Text color in the outline is determined by our stylesheet.
      delete fontStyle.color;

      if (!this.visible)
        this.show(window);
      else
        this._maybeCreateModalHighlightNodes(window);

      outlineNode = this._modalHighlightOutline;
      outlineNode.setTextContentForElement(kModalOutlineId + "-text", textContent.join(" "));
      outlineNode.setAttributeForElement(kModalOutlineId + "-text", "style",
        this._getHTMLFontStyle(fontStyle));

      if (typeof outlineNode.getAttributeForElement(kModalOutlineId, "hidden") == "string")
        outlineNode.removeAttributeForElement(kModalOutlineId, "hidden");
      let { scrollX, scrollY } = this._getScrollPosition(window);
      outlineNode.setAttributeForElement(kModalOutlineId, "style",
        `top: ${scrollY + rect.top}px; left: ${scrollX + rect.left}px`);
    }

    outlineNode = this._modalHighlightOutline;
    if (typeof outlineNode.getAttributeForElement(kModalOutlineId, "grow") == "string")
      return;

    window.requestAnimationFrame(() => {
      outlineNode.setAttributeForElement(kModalOutlineId, "grow", true);
      this._listenForOutlineEvent(kModalOutlineId, "transitionend", () => {
        try {
          outlineNode.removeAttributeForElement(kModalOutlineId, "grow");
        } catch (ex) {}
      });
    });
  },

  /**
   * Invalidates the list by clearing the map of highglighted ranges that we
   * keep to build the mask for.
   */
  clear() {
    if (!this._modal)
      return;

    // Reset the Map, because no range references a node anymore.
    if (this._modalHighlightRectsMap)
      this._modalHighlightRectsMap.clear();
  },

  /**
   * When the current page is refreshed or navigated away from, the CanvasFrame
   * contents is not valid anymore, i.e. all anonymous content is destroyed.
   * We need to clear the references we keep, which'll make sure we redraw
   * everything when the user starts to find in page again.
   */
  onLocationChange() {
    if (!this._modalHighlightOutline)
      return;

    if (kDebug)
      this._modalHighlightOutline.remove();
    try {
      this.finder._getWindow().document
        .removeAnonymousContent(this._modalHighlightOutline);
    } catch(ex) {}

    this._modalHighlightOutline = null;
  },

  /**
   * When `kModalHighlightPref` pref changed during a session, this callback is
   * invoked. When modal highlighting is turned off, we hide the CanvasFrame
   * contents.
   *
   * @param {Boolean} useModalHighlight
   */
  onModalHighlightChange(useModalHighlight) {
    if (this._modal && !useModalHighlight) {
      this.hide();
      this.clear();
    }
    this._modal = useModalHighlight;
  },

  /**
   * When 'Highlight All' is toggled during a session, this callback is invoked
   * and when it's turned off, the found occurrences will be removed from the mask.
   *
   * @param {Boolean} highlightAll
   */
  onHighlightAllChange(highlightAll) {
    this._highlightAll = highlightAll;
    if (this._modal && !highlightAll) {
      this.clear();
      this._scheduleRepaintOfMask(this.finder._getWindow());
    }
  },

  /**
   * Utility; removes all ranges from the find selection that belongs to a
   * controller. Optionally skips a specific range.
   *
   * @param  {nsISelectionController} controller
   * @param  {nsIDOMRange}            skipRange
   */
  _clearSelection(controller, skipRange = null) {
    let sel = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    if (!skipRange) {
      sel.removeAllRanges();
    } else {
      for (let i = sel.rangeCount - 1; i >= 0; --i) {
        let range = sel.getRangeAt(i);
        if (range !== skipRange)
          sel.removeRange(range);
      }
    }
  },

  /**
   * Utility; get the nsIDOMWindowUtils for a window.
   *
   * @param  {nsIDOMWindow} window Optional, defaults to the finder window.
   * @return {nsIDOMWindowUtils}
   */
  _getDWU(window = null) {
    return (window || this.finder._getWindow())
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils);
  },

  /**
   * Utility; wrapper around nsIDOMWindowUtils#getScrollXY.
   *
   * @param  {nsDOMWindow} window Optional, defaults to the finder window.
   * @return {Object} The current scroll position.
   */
  _getScrollPosition(window = null) {
    let scrollX = {};
    let scrollY = {};
    this._getDWU(window).getScrollXY(false, scrollX, scrollY);
    return {
      scrollX: scrollX.value,
      scrollY: scrollY.value
    };
  },

  /**
   * Utility; fetch the full width and height of the current window, excluding
   * scrollbars.
   *
   * @param  {nsiDOMWindow} window The current finder window.
   * @return {Object} The current full page dimensions with `width` and `height`
   *                  properties
   */
  _getWindowDimensions(window) {
    let width = window.innerWidth + window.scrollMaxX - window.scrollMinX;
    let height = window.innerHeight + window.scrollMaxY - window.scrollMinY;

    let scrollbarHeight = {};
    let scrollbarWidth = {};
    this._getDWU(window).getScrollbarSize(false, scrollbarWidth, scrollbarHeight);
    width -= scrollbarWidth.value;
    height -= scrollbarHeight.value;

    return { width, height };
  },

  /**
   * Utility; fetch the current text contents of a given range.
   *
   * @param  {nsIDOMRange} range Range object to extract the contents from.
   * @return {Array} Snippets of text.
   */
  _getRangeContentArray(range) {
    let content = range.cloneContents();
    let t, textContent = [];
    for (let node of content.childNodes) {
      t = node.textContent || node.nodeValue;
      //if (t && t.trim())
        textContent.push(t);
    }
    return textContent;
  },

  /**
   * Utility; get all available font styles as applied to the content of a given
   * range. The CSS properties we look for can be found in `kFontPropsCSS`.
   *
   * @param  {nsIDOMRange} range Range to fetch style info from.
   * @return {Object} Dictionary consisting of the styles that were found.
   */
  _getRangeFontStyle(range) {
    let node = range.startContainer;
    while (node.nodeType != 1)
      node = node.parentNode;
    let style = node.ownerDocument.defaultView.getComputedStyle(node, "");
    let props = {};
    for (let prop of kFontPropsCamelCase) {
      if (prop in style && style[prop])
        props[prop] = style[prop];
    }
    return props;
  },

  /**
   * Utility; transform a dictionary object as returned by `_getRangeFontStyle`
   * above into a HTML style attribute value.
   *
   * @param  {Object} fontStyle
   * @return {String}
   */
  _getHTMLFontStyle(fontStyle) {
    let style = [];
    for (let prop of Object.getOwnPropertyNames(fontStyle)) {
      let idx = kFontPropsCamelCase.indexOf(prop);
      if (idx == -1)
        continue
      style.push(`${kFontPropsCSS[idx]}: ${fontStyle[prop]};`);
    }
    return style.join(" ");
  },

  /**
   * Checks whether a CSS RGB color value can be classified as being 'bright'.
   *
   * @param  {String} cssColor RGB color value in the default format rgb[a](r,g,b)
   * @return {Boolean}
   */
  _isColorBright(cssColor) {
    cssColor = cssColor.match(kRGBRE);
    if (!cssColor || !cssColor.length)
      return false;
    cssColor.shift();
    return new Color(...cssColor).isBright;
  },

  /**
   * Add a range to the list of ranges to highlight on, or cut out of, the dimmed
   * background.
   *
   * @param {nsIDOMRange}  range  Range object that should be inspected
   * @param {nsIDOMWindow} window Window object, whose DOM tree is being traversed
   */
  _modalHighlight(range, controller, window) {
    if (!this._getRangeContentArray(range).length)
      return;

    let rects = new Set();
    // Absolute positions should include the viewport scroll offset.
    let { scrollX, scrollY } = this._getScrollPosition(window);
    // A range may consist of multiple rectangles, we can also do these kind of
    // precise cut-outs. range.getBoundingClientRect() returns the fully
    // encompassing rectangle, which is too much for our purpose here.
    for (let dims of range.getClientRects()) {
      rects.add({
        height: dims.bottom - dims.top,
        width: dims.right - dims.left,
        y: dims.top + scrollY,
        x: dims.left + scrollX
      });
    }

    if (!this._modalHighlightRectsMap)
      this._modalHighlightRectsMap = new Map();
    this._modalHighlightRectsMap.set(range, rects);

    this.show(window);
    // We don't repaint the mask right away, but pass it off to a render loop of
    // sorts.
    this._scheduleRepaintOfMask(window);
  },

  /**
   * Lazily insert the nodes we need as anonymous content into the CanvasFrame
   * of a window.
   *
   * @param {nsIDOMWindow} window Window to draw in.
   */
  _maybeCreateModalHighlightNodes(window) {
    if (this._modalHighlightOutline) {
      if (!this._modalHighlightAllMask) {
        // Make sure to at least show the dimmed background.
        this._repaintHighlightAllMask(window, false);
        this._scheduleRepaintOfMask(window);
      }
      return;
    }

    let document = window.document;
    // A hidden document doesn't accept insertAnonymousContent calls yet.
    if (document.hidden) {
      let onVisibilityChange = () => {
        document.removeEventListener("visibilitychange", onVisibilityChange);
        this._maybeCreateModalHighlightNodes(window);
      };
      document.addEventListener("visibilitychange", onVisibilityChange);
      return;
    }

    this._maybeInstallStyleSheet(window);

    // The outline needs to be sitting inside a container, otherwise the anonymous
    // content API won't find it by its ID later...
    let container = document.createElement("div");

    // Create the main (yellow) highlight outline box.
    let outlineBox = document.createElement("div");
    outlineBox.setAttribute("id", kModalOutlineId);
    outlineBox.className = kModalOutlineId + (kDebug ? ` ${kModalIdPrefix}-findbar-debug` : "");
    let outlineBoxText = document.createElement("span");
    outlineBoxText.setAttribute("id", kModalOutlineId + "-text");
    outlineBox.appendChild(outlineBoxText);

    container.appendChild(outlineBox);
    this._modalHighlightOutline = kDebug ?
      mockAnonymousContentNode(document.body.appendChild(container.firstChild)) :
      document.insertAnonymousContent(container);

    // Make sure to at least show the dimmed background.
    this._repaintHighlightAllMask(window, false);
  },

  /**
   * Build and draw the mask that takes care of the dimmed background that
   * overlays the current page and the mask that cuts out all the rectangles of
   * the ranges that were found.
   *
   * @param {nsIDOMWindow} window Window to draw in.
   * @param {Boolean} [paintContent]
   */
  _repaintHighlightAllMask(window, paintContent = true) {
    let document = window.document;

    const kMaskId = kModalIdPrefix + "-findbar-modalHighlight-outlineMask";
    let maskNode = document.createElement("div");

    // Make sure the dimmed mask node takes the full width and height that's available.
    let {width, height} = this._getWindowDimensions(window);
    maskNode.setAttribute("id", kMaskId);
    maskNode.setAttribute("class", kMaskId + (kDebug ? ` ${kModalIdPrefix}-findbar-debug` : ""));
    maskNode.setAttribute("style", `width: ${width}px; height: ${height}px;`);
    if (this._brightText)
      maskNode.setAttribute("brighttext", "true");

    if (paintContent) {
      // Create a DOM node for each rectangle representing the ranges we found.
      let maskContent = [];
      const kRectClassName = kModalIdPrefix + "-findbar-modalHighlight-rect";
      if (this._modalHighlightRectsMap) {
        for (let rects of this._modalHighlightRectsMap.values()) {
          for (let rect of rects) {
            maskContent.push(`<div class="${kRectClassName}" style="top: ${rect.y}px;
              left: ${rect.x}px; height: ${rect.height}px; width: ${rect.width}px;"></div>`);
          }
        }
      }
      maskNode.innerHTML = maskContent.join("");
    }

    // Always remove the current mask and insert it a-fresh, because we're not
    // free to alter DOM nodes inside the CanvasFrame.
    this._removeHighlightAllMask(window);

    this._modalHighlightAllMask = kDebug ?
      mockAnonymousContentNode(document.body.appendChild(maskNode)) :
      document.insertAnonymousContent(maskNode);
  },

  /**
   * Safely remove the mask AnoymousContent node from the CanvasFrame.
   *
   * @param {nsIDOMWindow} window
   */
  _removeHighlightAllMask(window) {
    if (this._modalHighlightAllMask) {
      // If the current window isn't the one the content was inserted into, this
      // will fail, but that's fine.
      if (kDebug)
        this._modalHighlightAllMask.remove();
      try {
        window.document.removeAnonymousContent(this._modalHighlightAllMask);
      } catch(ex) {}
      this._modalHighlightAllMask = null;
    }
  },

  /**
   * Doing a full repaint each time a range is delivered by the highlight iterator
   * is way too costly, thus we pipe the frequency down to every
   * `kModalHighlightRepaintFreqMs` milliseconds.
   *
   * @param {nsIDOMWindow} window
   */
  _scheduleRepaintOfMask(window) {
    if (this._modalRepaintScheduler)
      window.clearTimeout(this._modalRepaintScheduler);
    this._modalRepaintScheduler = window.setTimeout(
      this._repaintHighlightAllMask.bind(this, window), kModalHighlightRepaintFreqMs);
  },

  /**
   * The outline that shows/ highlights the current found range is styled and
   * animated using CSS. This style can be found in `kModalStyle`, but to have it
   * applied on any DOM node we insert using the AnonymousContent API we need to
   * inject an agent sheet into the document.
   *
   * @param {nsIDOMWindow} window
   */
  _maybeInstallStyleSheet(window) {
    let document = window.document;
    // The WeakMap is a cheap method to make sure we don't needlessly insert the
    // same sheet twice.
    if (!this._modalInstalledSheets)
      this._modalInstalledSheets = new WeakMap();
    if (this._modalInstalledSheets.has(document))
      return;

    let dwu = this._getDWU(window);
    let uri = this.modalStyleSheetURI;
    try {
      dwu.loadSheetUsingURIString(uri, dwu.AGENT_SHEET);
    } catch (e) {}
    this._modalInstalledSheets.set(document, uri);
  },

  /**
   * One can not simply listen to events on a specific AnonymousContent node.
   * That's why we need to listen on the chromeEventHandler instead and check if
   * the IDs match of the event target.
   * IMPORTANT: once the event was fired on the specified element and the handler
   *            invoked, we remove the event listener right away. That's because
   *            we don't need more in this class.
   *
   * @param {String}   elementId Identifier of the element we expect the event from.
   * @param {String}   eventName Name of the event to start listening for.
   * @param {Function} handler   Function to invoke when we detected the event
   *                             on the designated node.
   */
  _listenForOutlineEvent(elementId, eventName, handler) {
    let target = this.finder._docShell.chromeEventHandler;
    target.addEventListener(eventName, function onEvent(event) {
      // Start at originalTarget, bubble through ancestors and call handlers when
      // needed.
      let node = event.originalTarget;
      while (node) {
        if (node.id == elementId) {
          handler();
          target.removeEventListener(eventName, onEvent);
          break;
        }
        node = node.parentNode;
      }
    });
  },

  /**
   * Add event listeners to the content which will cause the modal highlight
   * AnonymousContent to be re-painted or hidden.
   *
   * @param {nsIDOMWindow} window
   */
  _addModalHighlightListeners(window) {
    if (this._highlightListeners)
      return;

    this._highlightListeners = [
      this._scheduleRepaintOfMask.bind(this, window),
      this.hide.bind(this, window, null)
    ];
    window.addEventListener("DOMContentLoaded", this._highlightListeners[0]);
    window.addEventListener("click", this._highlightListeners[1]);
    window.addEventListener("resize", this._highlightListeners[1]);
  },

  /**
   * Remove event listeners from content.
   *
   * @param {nsIDOMWindow} window
   */
  _removeModalHighlightListeners(window) {
    if (!this._highlightListeners)
      return;

    window.removeEventListener("DOMContentLoaded", this._highlightListeners[0]);
    window.removeEventListener("click", this._highlightListeners[1]);
    window.removeEventListener("resize", this._highlightListeners[1]);

    this._highlightListeners = null;
  },

  /**
   * For a given node returns its editable parent or null if there is none.
   * It's enough to check if node is a text node and its parent's parent is
   * instance of nsIDOMNSEditableElement.
   *
   * @param node the node we want to check
   * @returns the first node in the parent chain that is editable,
   *          null if there is no such node
   */
  _getEditableNode(node) {
    if (node.nodeType === node.TEXT_NODE && node.parentNode && node.parentNode.parentNode &&
        node.parentNode.parentNode instanceof Ci.nsIDOMNSEditableElement) {
      return node.parentNode.parentNode;
    }
    return null;
  },

  /**
   * Add ourselves as an nsIEditActionListener and nsIDocumentStateListener for
   * a given editor
   *
   * @param editor the editor we'd like to listen to
   */
  _addEditorListeners(editor) {
    if (!this._editors) {
      this._editors = [];
      this._stateListeners = [];
    }

    let existingIndex = this._editors.indexOf(editor);
    if (existingIndex == -1) {
      let x = this._editors.length;
      this._editors[x] = editor;
      this._stateListeners[x] = this._createStateListener();
      this._editors[x].addEditActionListener(this);
      this._editors[x].addDocumentStateListener(this._stateListeners[x]);
    }
  },

  /**
   * Helper method to unhook listeners, remove cached editors
   * and keep the relevant arrays in sync
   *
   * @param idx the index into the array of editors/state listeners
   *        we wish to remove
   */
  _unhookListenersAtIndex(idx) {
    this._editors[idx].removeEditActionListener(this);
    this._editors[idx]
        .removeDocumentStateListener(this._stateListeners[idx]);
    this._editors.splice(idx, 1);
    this._stateListeners.splice(idx, 1);
    if (!this._editors.length) {
      delete this._editors;
      delete this._stateListeners;
    }
  },

  /**
   * Remove ourselves as an nsIEditActionListener and
   * nsIDocumentStateListener from a given cached editor
   *
   * @param editor the editor we no longer wish to listen to
   */
  _removeEditorListeners(editor) {
    // editor is an editor that we listen to, so therefore must be
    // cached. Find the index of this editor
    let idx = this._editors.indexOf(editor);
    if (idx == -1) {
      return;
    }
    // Now unhook ourselves, and remove our cached copy
    this._unhookListenersAtIndex(idx);
  },

  /*
   * nsIEditActionListener logic follows
   *
   * We implement this interface to allow us to catch the case where
   * the findbar found a match in a HTML <input> or <textarea>. If the
   * user adjusts the text in some way, it will no longer match, so we
   * want to remove the highlight, rather than have it expand/contract
   * when letters are added or removed.
   */

  /**
   * Helper method used to check whether a selection intersects with
   * some highlighting
   *
   * @param selectionRange the range from the selection to check
   * @param findRange the highlighted range to check against
   * @returns true if they intersect, false otherwise
   */
  _checkOverlap(selectionRange, findRange) {
    // The ranges overlap if one of the following is true:
    // 1) At least one of the endpoints of the deleted selection
    //    is in the find selection
    // 2) At least one of the endpoints of the find selection
    //    is in the deleted selection
    if (findRange.isPointInRange(selectionRange.startContainer,
                                 selectionRange.startOffset))
      return true;
    if (findRange.isPointInRange(selectionRange.endContainer,
                                 selectionRange.endOffset))
      return true;
    if (selectionRange.isPointInRange(findRange.startContainer,
                                      findRange.startOffset))
      return true;
    if (selectionRange.isPointInRange(findRange.endContainer,
                                      findRange.endOffset))
      return true;

    return false;
  },

  /**
   * Helper method to determine if an edit occurred within a highlight
   *
   * @param selection the selection we wish to check
   * @param node the node we want to check is contained in selection
   * @param offset the offset into node that we want to check
   * @returns the range containing (node, offset) or null if no ranges
   *          in the selection contain it
   */
  _findRange(selection, node, offset) {
    let rangeCount = selection.rangeCount;
    let rangeidx = 0;
    let foundContainingRange = false;
    let range = null;

    // Check to see if this node is inside one of the selection's ranges
    while (!foundContainingRange && rangeidx < rangeCount) {
      range = selection.getRangeAt(rangeidx);
      if (range.isPointInRange(node, offset)) {
        foundContainingRange = true;
        break;
      }
      rangeidx++;
    }

    if (foundContainingRange) {
      return range;
    }

    return null;
  },

  // Start of nsIEditActionListener implementations

  WillDeleteText(textNode, offset, length) {
    let editor = this._getEditableNode(textNode).editor;
    let controller = editor.selectionController;
    let fSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    let range = this._findRange(fSelection, textNode, offset);

    if (range) {
      // Don't remove the highlighting if the deleted text is at the
      // end of the range
      if (textNode != range.endContainer ||
          offset != range.endOffset) {
        // Text within the highlight is being removed - the text can
        // no longer be a match, so remove the highlighting
        fSelection.removeRange(range);
        if (fSelection.rangeCount == 0) {
          this._removeEditorListeners(editor);
        }
      }
    }
  },

  DidInsertText(textNode, offset, aString) {
    let editor = this._getEditableNode(textNode).editor;
    let controller = editor.selectionController;
    let fSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    let range = this._findRange(fSelection, textNode, offset);

    if (range) {
      // If the text was inserted before the highlight
      // adjust the highlight's bounds accordingly
      if (textNode == range.startContainer &&
          offset == range.startOffset) {
        range.setStart(range.startContainer,
                       range.startOffset+aString.length);
      } else if (textNode != range.endContainer ||
                 offset != range.endOffset) {
        // The edit occurred within the highlight - any addition of text
        // will result in the text no longer being a match,
        // so remove the highlighting
        fSelection.removeRange(range);
        if (fSelection.rangeCount == 0) {
          this._removeEditorListeners(editor);
        }
      }
    }
  },

  WillDeleteSelection(selection) {
    let editor = this._getEditableNode(selection.getRangeAt(0)
                                                 .startContainer).editor;
    let controller = editor.selectionController;
    let fSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);

    let selectionIndex = 0;
    let findSelectionIndex = 0;
    let shouldDelete = {};
    let numberOfDeletedSelections = 0;
    let numberOfMatches = fSelection.rangeCount;

    // We need to test if any ranges in the deleted selection (selection)
    // are in any of the ranges of the find selection
    // Usually both selections will only contain one range, however
    // either may contain more than one.

    for (let fIndex = 0; fIndex < numberOfMatches; fIndex++) {
      shouldDelete[fIndex] = false;
      let fRange = fSelection.getRangeAt(fIndex);

      for (let index = 0; index < selection.rangeCount; index++) {
        if (shouldDelete[fIndex]) {
          continue;
        }

        let selRange = selection.getRangeAt(index);
        let doesOverlap = this._checkOverlap(selRange, fRange);
        if (doesOverlap) {
          shouldDelete[fIndex] = true;
          numberOfDeletedSelections++;
        }
      }
    }

    // OK, so now we know what matches (if any) are in the selection
    // that is being deleted. Time to remove them.
    if (!numberOfDeletedSelections) {
      return;
    }

    for (let i = numberOfMatches - 1; i >= 0; i--) {
      if (shouldDelete[i])
        fSelection.removeRange(fSelection.getRangeAt(i));
    }

    // Remove listeners if no more highlights left
    if (!fSelection.rangeCount) {
      this._removeEditorListeners(editor);
    }
  },

  /*
   * nsIDocumentStateListener logic follows
   *
   * When attaching nsIEditActionListeners, there are no guarantees
   * as to whether the findbar or the documents in the browser will get
   * destructed first. This leads to the potential to either leak, or to
   * hold on to a reference an editable element's editor for too long,
   * preventing it from being destructed.
   *
   * However, when an editor's owning node is being destroyed, the editor
   * sends out a DocumentWillBeDestroyed notification. We can use this to
   * clean up our references to the object, to allow it to be destroyed in a
   * timely fashion.
   */

  /**
   * Unhook ourselves when one of our state listeners has been called.
   * This can happen in 4 cases:
   *  1) The document the editor belongs to is navigated away from, and
   *     the document is not being cached
   *
   *  2) The document the editor belongs to is expired from the cache
   *
   *  3) The tab containing the owning document is closed
   *
   *  4) The <input> or <textarea> that owns the editor is explicitly
   *     removed from the DOM
   *
   * @param the listener that was invoked
   */
  _onEditorDestruction(aListener) {
    // First find the index of the editor the given listener listens to.
    // The listeners and editors arrays must always be in sync.
    // The listener will be in our array of cached listeners, as this
    // method could not have been called otherwise.
    let idx = 0;
    while (this._stateListeners[idx] != aListener) {
      idx++;
    }

    // Unhook both listeners
    this._unhookListenersAtIndex(idx);
  },

  /**
   * Creates a unique document state listener for an editor.
   *
   * It is not possible to simply have the findbar implement the
   * listener interface itself, as it wouldn't have sufficient information
   * to work out which editor was being destroyed. Therefore, we create new
   * listeners on the fly, and cache them in sync with the editors they
   * listen to.
   */
  _createStateListener() {
    return {
      findbar: this,

      QueryInterface: function(iid) {
        if (iid.equals(Ci.nsIDocumentStateListener) ||
            iid.equals(Ci.nsISupports))
          return this;

        throw Components.results.NS_ERROR_NO_INTERFACE;
      },

      NotifyDocumentWillBeDestroyed: function() {
        this.findbar._onEditorDestruction(this);
      },

      // Unimplemented
      notifyDocumentCreated: function() {},
      notifyDocumentStateChanged: function(aDirty) {}
    };
  }
};
