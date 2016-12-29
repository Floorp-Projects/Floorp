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
XPCOMUtils.defineLazyModuleGetter(this, "Rect", "resource://gre/modules/Geometry.jsm");
XPCOMUtils.defineLazyGetter(this, "kDebug", () => {
  const kDebugPref = "findbar.modalHighlight.debug";
  return Services.prefs.getPrefType(kDebugPref) && Services.prefs.getBoolPref(kDebugPref);
});

const kContentChangeThresholdPx = 5;
const kBrightTextSampleSize = 5;
const kModalHighlightRepaintLoFreqMs = 100;
const kModalHighlightRepaintHiFreqMs = 16;
const kHighlightAllPref = "findbar.highlightAll";
const kModalHighlightPref = "findbar.modalHighlight";
const kFontPropsCSS = ["color", "font-family", "font-kerning", "font-size",
  "font-size-adjust", "font-stretch", "font-variant", "font-weight", "line-height",
  "letter-spacing", "text-emphasis", "text-orientation", "text-transform", "word-spacing"];
const kFontPropsCamelCase = kFontPropsCSS.map(prop => {
  let parts = prop.split("-");
  return parts.shift() + parts.map(part => part.charAt(0).toUpperCase() + part.slice(1)).join("");
});
const kRGBRE = /^rgba?\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*/i;
// This uuid is used to prefix HTML element IDs in order to make them unique and
// hard to clash with IDs content authors come up with.
const kModalIdPrefix = "cedee4d0-74c5-4f2d-ab43-4d37c0f9d463";
const kModalOutlineId = kModalIdPrefix + "-findbar-modalHighlight-outline";
const kOutlineBoxColor = "255,197,53";
const kOutlineBoxBorderSize = 2;
const kOutlineBoxBorderRadius = 3;
const kModalStyles = {
  outlineNode: [
    ["background-color", `rgb(${kOutlineBoxColor})`],
    ["background-clip", "padding-box"],
    ["border", `${kOutlineBoxBorderSize}px solid`],
    ["-moz-border-top-colors", `rgba(${kOutlineBoxColor},.1) rgba(${kOutlineBoxColor},.4) rgba(${kOutlineBoxColor},.7)`],
    ["-moz-border-right-colors", `rgba(${kOutlineBoxColor},.1) rgba(${kOutlineBoxColor},.4) rgba(${kOutlineBoxColor},.7)`],
    ["-moz-border-bottom-colors", `rgba(${kOutlineBoxColor},.1) rgba(${kOutlineBoxColor},.4) rgba(${kOutlineBoxColor},.7)`],
    ["-moz-border-left-colors", `rgba(${kOutlineBoxColor},.1) rgba(${kOutlineBoxColor},.4) rgba(${kOutlineBoxColor},.7)`],
    ["border-radius", `${kOutlineBoxBorderRadius}px`],
    ["box-shadow", `0 ${kOutlineBoxBorderSize}px 0 0 rgba(0,0,0,.1)`],
    ["color", "#000"],
    ["display", "-moz-box"],
    ["margin", `-${kOutlineBoxBorderSize}px 0 0 -${kOutlineBoxBorderSize}px !important`],
    ["overflow", "hidden"],
    ["pointer-events", "none"],
    ["position", "absolute"],
    ["white-space", "nowrap"],
    ["will-change", "transform"],
    ["z-index", 2]
  ],
  outlineNodeDebug: [ ["z-index", 2147483647] ],
  outlineText: [
    ["margin", "0 !important"],
    ["padding", "0 !important"],
    ["vertical-align", "top !important"]
  ],
  maskNode: [
    ["background", "rgba(0,0,0,.25)"],
    ["pointer-events", "none"],
    ["position", "absolute"],
    ["z-index", 1]
  ],
  maskNodeTransition: [
    ["transition", "background .2s ease-in"]
  ],
  maskNodeDebug: [
    ["z-index", 2147483646],
    ["top", 0],
    ["left", 0]
  ],
  maskNodeBrightText: [ ["background", "rgba(255,255,255,.25)"] ]
};
const kModalOutlineAnim = {
  "keyframes": [
    { transform: "scaleX(1) scaleY(1)" },
    { transform: "scaleX(1.5) scaleY(1.5)", offset: .5, easing: "ease-in" },
    { transform: "scaleX(1) scaleY(1)" }
  ],
  duration: 50,
};
const kNSHTML = "http://www.w3.org/1999/xhtml";

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
    },
    setAnimationForElement(id, keyframes, duration) {
      return (domNode.querySelector("#" + id) || domNode).animate(keyframes, duration);
    },
    setCutoutRectsForElement(id, rects) {
      // no-op for now.
    }
  };
}

let gWindows = new Map();

/**
 * FinderHighlighter class that is used by Finder.jsm to take care of the
 * 'Highlight All' feature, which can highlight all find occurrences in a page.
 *
 * @param {Finder} finder Finder.jsm instance
 */
function FinderHighlighter(finder) {
  this._highlightAll = Services.prefs.getBoolPref(kHighlightAllPref);
  this._modal = Services.prefs.getBoolPref(kModalHighlightPref);
  this.finder = finder;
}

FinderHighlighter.prototype = {
  get iterator() {
    if (this._iterator)
      return this._iterator;
    this._iterator = Cu.import("resource://gre/modules/FinderIterator.jsm", null).FinderIterator;
    return this._iterator;
  },

  /**
   * Each window is unique, globally, and the relation between an active
   * highlighting session and a window is 1:1.
   * For each window we track a number of properties which _at least_ consist of
   *  - {Boolean} detectedGeometryChange Whether the geometry of the found ranges'
   *                                     rectangles has changed substantially
   *  - {Set}     dynamicRangesSet       Set of ranges that may move around, depending
   *                                     on page layout changes and user input
   *  - {Map}     frames                 Collection of frames that were encountered
   *                                     when inspecting the found ranges
   *  - {Map}     modalHighlightRectsMap Collection of ranges and their corresponding
   *                                     Rects and texts
   *
   * @param  {nsIDOMWindow} window
   * @return {Object}
   */
  getForWindow(window, propName = null) {
    if (!gWindows.has(window)) {
      gWindows.set(window, {
        detectedGeometryChange: false,
        dynamicRangesSet: new Set(),
        frames: new Map(),
        modalHighlightRectsMap: new Map(),
        previousRangeRectsCount: 0
      });
    }
    return gWindows.get(window);
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
    let dict = this.getForWindow(window);
    let controller = this.finder._getSelectionController(window);
    let doc = window.document;
    this._found = false;

    if (!controller || !doc || !doc.documentElement) {
      // Without the selection controller,
      // we are unable to (un)highlight any matches
      return this._found;
    }

    if (highlight) {
      let params = {
        allowDistance: 1,
        caseSensitive: this.finder._fastFind.caseSensitive,
        entireWord: this.finder._fastFind.entireWord,
        linksOnly, word,
        finder: this.finder,
        listener: this,
        useCache: true,
        window
      };
      if (this._modal && this.iterator._areParamsEqual(params, dict.lastIteratorParams))
        return this._found;

      if (!this._modal)
        dict.visible = true;
      yield this.iterator.start(params);
      if (this._found) {
        this.finder._outlineLink(true);
        dict.updateAllRanges = true;
      }
    } else {
      this.hide(window);

      // Removing the highlighting always succeeds, so return true.
      this._found = true;
    }

    this.notifyFinished({ highlight, found: this._found });

    return this._found;
  }),

  // FinderIterator listener implementation

  onIteratorRangeFound(range) {
    this.highlightRange(range);
    this._found = true;
  },

  onIteratorReset() {},

  onIteratorRestart() {
    this.clear(this.finder._getWindow());
  },

  onIteratorStart(params) {
    let window = this.finder._getWindow();
    let dict = this.getForWindow(window);
    // Save a clean params set for use later in the `update()` method.
    dict.lastIteratorParams = params;
    if (!this._modal)
      this.hide(window, this.finder._fastFind.getFoundRange());
    this.clear(window);
  },

  /**
   * Add a range to the find selection, i.e. highlight it, and if it's inside an
   * editable node, track it.
   *
   * @param {nsIDOMRange} range Range object to be highlighted
   */
  highlightRange(range) {
    let node = range.startContainer;
    let editableNode = this._getEditableNode(node);
    let window = node.ownerDocument.defaultView;
    let controller = this.finder._getSelectionController(window);
    if (editableNode) {
      controller = editableNode.editor.selectionController;
    }

    if (this._modal) {
      this._modalHighlight(range, controller, window);
    } else {
      let findSelection = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
      findSelection.addRange(range);
      // Check if the range is inside an iframe.
      if (window != window.top) {
        let dict = this.getForWindow(window.top);
        if (!dict.frames.has(window))
          dict.frames.set(window, null);
      }
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
    window = (window || this.finder._getWindow()).top;
    let dict = this.getForWindow(window);
    if (!this._modal || dict.visible)
      return;

    dict.visible = true;

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
    try {
      window = (window || this.finder._getWindow()).top;
    } catch (ex) {
      Cu.reportError(ex);
      return;
    }
    let dict = this.getForWindow(window);

    let isBusySelecting = dict.busySelecting;
    dict.busySelecting = false;
    // Do not hide on anything but a left-click.
    if (event && event.type == "click" && (event.button !== 0 || event.altKey ||
        event.ctrlKey || event.metaKey || event.shiftKey || event.relatedTarget ||
        isBusySelecting || (event.target.localName == "a" && event.target.href))) {
      return;
    }

    this._clearSelection(this.finder._getSelectionController(window), skipRange);
    for (let frame of dict.frames.keys())
      this._clearSelection(this.finder._getSelectionController(frame), skipRange);

    // Next, check our editor cache, for editors belonging to this
    // document
    if (this._editors) {
      let doc = window.document;
      for (let x = this._editors.length - 1; x >= 0; --x) {
        if (this._editors[x].document == doc) {
          this._clearSelection(this._editors[x].selectionController, skipRange);
          // We don't need to listen to this editor any more
          this._unhookListenersAtIndex(x);
        }
      }
    }

    if (dict.modalRepaintScheduler) {
      window.clearTimeout(dict.modalRepaintScheduler);
      dict.modalRepaintScheduler = null;
    }
    dict.lastWindowDimensions = null;

    if (dict.modalHighlightOutline) {
      dict.modalHighlightOutline.setAttributeForElement(kModalOutlineId, "style",
        dict.modalHighlightOutline.getAttributeForElement(kModalOutlineId, "style") +
        "; opacity: 0");
    }

    this._removeHighlightAllMask(window);
    this._removeModalHighlightListeners(window);

    dict.visible = false;
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
    let dict = this.getForWindow(window);
    let foundRange = this.finder._fastFind.getFoundRange();

    if (data.result == Ci.nsITypeAheadFind.FIND_NOTFOUND || !data.searchString || !foundRange) {
      this.hide();
      return;
    }

    if (!this._modal) {
      if (this._highlightAll) {
        dict.currentFoundRange = foundRange;
        let params = this.iterator.params;
        if (dict.visible && this.iterator._areParamsEqual(params, dict.lastIteratorParams))
          return;
        if (!dict.visible && !params)
          params = {word: data.searchString, linksOnly: data.linksOnly};
        if (params)
          this.highlight(true, params.word, params.linksOnly);
      }
      return;
    }

    if (foundRange !== dict.currentFoundRange || data.findAgain) {
      dict.currentFoundRange = foundRange;

      let textContent = this._getRangeContentArray(foundRange);
      if (!textContent.length) {
        this.hide(window);
        return;
      }

      if (data.findAgain)
        dict.updateAllRanges = true;

      if (!dict.visible)
        this.show(window);
      else
        this._maybeCreateModalHighlightNodes(window);
    }

    let outlineNode = dict.modalHighlightOutline;
    if (outlineNode) {
      if (dict.animation)
        dict.animation.finish();
      dict.animation = outlineNode.setAnimationForElement(kModalOutlineId,
        Cu.cloneInto(kModalOutlineAnim.keyframes, window), kModalOutlineAnim.duration);
      dict.animation.onfinish = () => dict.animation = null;
    }

    if (this._highlightAll)
      this.highlight(true, data.searchString, data.linksOnly);
  },

  /**
   * Invalidates the list by clearing the map of highlighted ranges that we
   * keep to build the mask for.
   */
  clear(window = null) {
    if (!window) {
      // Since we're clearing _all the things_, make sure we hide 'em all as well.
      for (let win of gWindows.keys())
        this.hide(win);
      // Reset the Map, because no range references a node anymore.
      gWindows.clear();
      return;
    }

    let dict = this.getForWindow(window.top);
    if (dict.animation)
      dict.animation.finish();
    dict.dynamicRangesSet.clear();
    dict.frames.clear();
    dict.modalHighlightRectsMap.clear();
    dict.brightText = null;
  },

  /**
   * When the current page is refreshed or navigated away from, the CanvasFrame
   * contents is not valid anymore, i.e. all anonymous content is destroyed.
   * We need to clear the references we keep, which'll make sure we redraw
   * everything when the user starts to find in page again.
   */
  onLocationChange() {
    let window = this.finder._getWindow();
    this.hide(window);
    let dict = this.getForWindow(window);
    this.clear(window);
    dict.currentFoundRange = dict.lastIteratorParams = null;

    if (!dict.modalHighlightOutline)
      return;

    if (kDebug) {
      dict.modalHighlightOutline.remove();
    } else {
      try {
        window.document.removeAnonymousContent(dict.modalHighlightOutline);
      } catch (ex) {}
    }

    dict.modalHighlightOutline = null;
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
    if (!highlightAll) {
      let window = this.finder._getWindow();
      if (!this._modal)
        this.hide(window);
      this.clear(window);
      this._scheduleRepaintOfMask(window);
    }
  },

  /**
   * Utility; removes all ranges from the find selection that belongs to a
   * controller. Optionally skips a specific range.
   *
   * @param  {nsISelectionController} controller
   * @param  {nsIDOMRange}            restoreRange
   */
  _clearSelection(controller, restoreRange = null) {
    if (!controller)
      return;
    let sel = controller.getSelection(Ci.nsISelectionController.SELECTION_FIND);
    sel.removeAllRanges();
    if (restoreRange) {
      sel = controller.getSelection(Ci.nsISelectionController.SELECTION_NORMAL);
      sel.addRange(restoreRange);
      controller.setDisplaySelection(Ci.nsISelectionController.SELECTION_ATTENTION);
      controller.repaintSelection(Ci.nsISelectionController.SELECTION_NORMAL);
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
   * Utility; returns the bounds of the page relative to the viewport.
   * If the pages is part of a frameset or inside an iframe of any kind, its
   * offset is accounted for.
   * Geometry.jsm takes care of the DOMRect calculations.
   *
   * @param  {nsIDOMWindow} window          Window to read the boundary rect from
   * @param  {Boolean}      [includeScroll] Whether to ignore the scroll offset,
   *                                        which is useful for comparing DOMRects.
   *                                        Optional, defaults to `true`
   * @return {Rect}
   */
  _getRootBounds(window, includeScroll = true) {
    let dwu = this._getDWU(window.top);
    let cssPageRect = Rect.fromRect(dwu.getRootBounds());
    let scrollX = {};
    let scrollY = {};
    if (includeScroll && window == window.top) {
      dwu.getScrollXY(false, scrollX, scrollY);
      cssPageRect.translate(scrollX.value, scrollY.value);
    }

    // If we're in a frame, update the position of the rect (top/ left).
    let currWin = window;
    while (currWin != window.top) {
      // Since the frame is an element inside a parent window, we'd like to
      // learn its position relative to it.
      let el = this._getDWU(currWin).containerElement;
      currWin = currWin.parent;
      dwu = this._getDWU(currWin);
      let parentRect = Rect.fromRect(dwu.getBoundsWithoutFlushing(el));

      if (includeScroll) {
        dwu.getScrollXY(false, scrollX, scrollY);
        parentRect.translate(scrollX.value, scrollY.value);
        // If the current window is an iframe with scrolling="no" and its parent
        // is also an iframe the scroll offsets from the parents' documentElement
        // (inverse scroll position) needs to be subtracted from the parent
        // window rect.
        if (el.getAttribute("scrolling") == "no" && currWin != window.top) {
          let docEl = currWin.document.documentElement;
          parentRect.translate(-docEl.scrollLeft, -docEl.scrollTop);
        }
      }

      cssPageRect.translate(parentRect.left, parentRect.top);
    }

    return cssPageRect;
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
    // First we'll try without flushing layout, because it's way faster.
    let dwu = this._getDWU(window);
    let { width, height } = dwu.getRootBounds();

    if (!width || !height) {
      // We need a flush after all :'(
      width = window.innerWidth + window.scrollMaxX - window.scrollMinX;
      height = window.innerHeight + window.scrollMaxY - window.scrollMinY;

      let scrollbarHeight = {};
      let scrollbarWidth = {};
      dwu.getScrollbarSize(false, scrollbarWidth, scrollbarHeight);
      width -= scrollbarWidth.value;
      height -= scrollbarHeight.value;
    }

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
    let textContent = [];
    for (let node of content.childNodes) {
      textContent.push(node.textContent || node.nodeValue);
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
        continue;
      style.push(`${kFontPropsCSS[idx]}: ${fontStyle[prop]}`);
    }
    return style.join("; ");
  },

  /**
   * Transform a style definition array as defined in `kModalStyles` into a CSS
   * string that can be used to set the 'style' property of a DOM node.
   *
   * @param  {Array}    stylePairs         Two-dimensional array of style pairs
   * @param  {...Array} [additionalStyles] Optional set of style pairs that will
   *                                       augment or override the styles defined
   *                                       by `stylePairs`
   * @return {String}
   */
  _getStyleString(stylePairs, ...additionalStyles) {
    let baseStyle = new Map(stylePairs);
    for (let additionalStyle of additionalStyles) {
      for (let [prop, value] of additionalStyle)
        baseStyle.set(prop, value);
    }
    return [...baseStyle].map(([cssProp, cssVal]) => `${cssProp}: ${cssVal}`).join("; ");
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
   * Detects if the overall text color in the page can be described as bright.
   * This is done according to the following algorithm:
   *  1. With the entire set of ranges that we have found thusfar;
   *  2. Get an odd-numbered `sampleSize`, with a maximum of `kBrightTextSampleSize`
   *     ranges,
   *  3. Slice the set of ranges into `sampleSize` number of equal parts,
   *  4. Grab the first range for each slice and inspect the brightness of the
   *     color of its text content.
   *  5. When the majority of ranges are counted as contain bright colored text,
   *     the page is considered to contain bright text overall.
   *
   * @param {Object} dict Dictionary of properties belonging to the
   *                      currently active window. The page text color property
   *                      will be recorded in `dict.brightText` as `true` or `false`.
   */
  _detectBrightText(dict) {
    let sampleSize = Math.min(dict.modalHighlightRectsMap.size, kBrightTextSampleSize);
    let ranges = [...dict.modalHighlightRectsMap.keys()];
    let rangesCount = ranges.length;
    // Make sure the sample size is an odd number.
    if (sampleSize % 2 == 0) {
      // Make the currently found range weigh heavier.
      if (dict.currentFoundRange) {
        ranges.push(dict.currentFoundRange);
        ++sampleSize;
        ++rangesCount;
      } else {
        --sampleSize;
      }
    }
    let brightCount = 0;
    for (let i = 0; i < sampleSize; ++i) {
      let range = ranges[Math.floor((rangesCount / sampleSize) * i)];
      let fontStyle = this._getRangeFontStyle(range);
      if (this._isColorBright(fontStyle.color))
        ++brightCount;
    }

    dict.brightText = (brightCount >= Math.ceil(sampleSize / 2));
  },

  /**
   * Checks if a range is inside a DOM node that's positioned in a way that it
   * doesn't scroll along when the document is scrolled and/ or zoomed. This
   * is the case for 'fixed' and 'sticky' positioned elements, elements inside
   * (i)frames and elements that have their overflow styles set to 'auto' or
   * 'scroll'.
   *
   * @param  {nsIDOMRange} range Range that be enclosed in a dynamic container
   * @return {Boolean}
   */
  _isInDynamicContainer(range) {
    const kFixed = new Set(["fixed", "sticky", "scroll", "auto"]);
    let node = range.startContainer;
    while (node.nodeType != 1)
      node = node.parentNode;
    let document = node.ownerDocument;
    let window = document.defaultView;
    let dict = this.getForWindow(window.top);

    // Check if we're in a frameset (including iframes).
    if (window != window.top) {
      if (!dict.frames.has(window))
        dict.frames.set(window, null);
      return true;
    }

    do {
      let style = window.getComputedStyle(node, null);
      if (kFixed.has(style.position) || kFixed.has(style.overflow) ||
          kFixed.has(style.overflowX) || kFixed.has(style.overflowY)) {
        return true;
      }
      node = node.parentNode;
    } while (node && node != document.documentElement)

    return false;
  },

  /**
   * Read and store the rectangles that encompass the entire region of a range
   * for use by the drawing function of the highlighter.
   *
   * @param  {nsIDOMRange} range  Range to fetch the rectangles from
   * @param  {Object}      [dict] Dictionary of properties belonging to
   *                              the currently active window
   * @return {Set}         Set of rects that were found for the range
   */
  _getRangeRectsAndTexts(range, dict = null) {
    let window = range.startContainer.ownerDocument.defaultView;
    let bounds;
    // If the window is part of a frameset, try to cache the bounds query.
    if (dict && dict.frames.has(window)) {
      bounds = dict.frames.get(window);
      if (!bounds) {
        bounds = this._getRootBounds(window);
        dict.frames.set(window, bounds);
      }
    } else
      bounds = this._getRootBounds(window);

    let topBounds = this._getRootBounds(window.top, false);
    let rects = [];
    // A range may consist of multiple rectangles, we can also do these kind of
    // precise cut-outs. range.getBoundingClientRect() returns the fully
    // encompassing rectangle, which is too much for our purpose here.
    let {rectList, textList} = range.getClientRectsAndTexts();
    for (let rect of rectList) {
      rect = Rect.fromRect(rect);
      rect.x += bounds.x;
      rect.y += bounds.y;
      // If the rect is not even visible from the top document, we can ignore it.
      if (rect.intersects(topBounds))
        rects.push(rect);
    }
    return {rectList: rects, textList};
  },

  /**
   * Read and store the rectangles that encompass the entire region of a range
   * for use by the drawing function of the highlighter and store them in the
   * cache.
   *
   * @param  {nsIDOMRange} range            Range to fetch the rectangles from
   * @param  {Boolean}     [checkIfDynamic] Whether we should check if the range
   *                                        is dynamic as per the rules in
   *                                        `_isInDynamicContainer()`. Optional,
   *                                        defaults to `true`
   * @param  {Object}      [dict]           Dictionary of properties belonging to
   *                                        the currently active window
   * @return {Set}         Set of rects that were found for the range
   */
  _updateRangeRects(range, checkIfDynamic = true, dict = null) {
    let window = range.startContainer.ownerDocument.defaultView;
    let rectsAndTexts = this._getRangeRectsAndTexts(range, dict);

    // Only fetch the rect at this point, if not passed in as argument.
    dict = dict || this.getForWindow(window.top);
    let oldRectsAndTexts = dict.modalHighlightRectsMap.get(range);
    dict.modalHighlightRectsMap.set(range, rectsAndTexts);
    // Check here if we suddenly went down to zero rects from more than zero before,
    // which indicates that we should re-iterate the document.
    if (oldRectsAndTexts && oldRectsAndTexts.rectList.length && !rectsAndTexts.rectList.length)
      dict.detectedGeometryChange = true;
    if (checkIfDynamic && this._isInDynamicContainer(range))
      dict.dynamicRangesSet.add(range);
    return rectsAndTexts;
  },

  /**
   * Re-read the rectangles of the ranges that we keep track of separately,
   * because they're enclosed by a position: fixed container DOM node or (i)frame.
   *
   * @param {Object} dict Dictionary of properties belonging to the currently
   *                      active window
   */
  _updateDynamicRangesRects(dict) {
    // Reset the frame bounds cache.
    for (let frame of dict.frames.keys())
      dict.frames.set(frame, null);
    for (let range of dict.dynamicRangesSet)
      this._updateRangeRects(range, false, dict);
  },

  /**
   * Update the content, position and style of the yellow current found range
   * outline that floats atop the mask with the dimmed background.
   * Rebuild it, if necessary, This will deactivate the animation between
   * occurrences.
   *
   * @param {Object} dict          Dictionary of properties belonging to the
   *                               currently active window
   * @param {Array}  [textContent] Array of text that's inside the range. Optional,
   *                               defaults to `null`
   * @param {Object} [fontStyle]   Dictionary of CSS styles in camelCase as
   *                               returned by `_getRangeFontStyle()`. Optional
   */
  _updateRangeOutline(dict, textContent = null, fontStyle = null) {
    let range = dict.currentFoundRange;
    if (!range)
      return;

    fontStyle = fontStyle || this._getRangeFontStyle(range);
    // Text color in the outline is determined by kModalStyles.
    delete fontStyle.color;

    let rectsAndTexts = this._getRangeRectsAndTexts(range);
    textContent = textContent || this._getRangeContentArray(range);

    let outlineAnonNode = dict.modalHighlightOutline;
    let rectCount = rectsAndTexts.rectList.length;
    // (re-)Building the outline is conditional and happens when one of the
    // following conditions is met:
    // 1. No outline nodes were built before, or
    // 2. When the amount of rectangles to draw is different from before, or
    // 3. When there's more than one rectangle to draw, because it's impossible
    //    to animate that consistently with AnonymousContent nodes.
    let rebuildOutline = (!outlineAnonNode || rectCount !== dict.previousRangeRectsCount ||
      rectCount != 1);
    dict.previousRangeRectsCount = rectCount;

    let window = range.startContainer.ownerDocument.defaultView.top;
    let document = window.document;
    // First see if we need to and can remove the previous outline nodes.
    if (rebuildOutline && outlineAnonNode) {
      if (kDebug) {
        outlineAnonNode.remove();
      } else {
        try {
          document.removeAnonymousContent(outlineAnonNode);
        } catch (ex) {}
      }
      dict.modalHighlightOutline = null;
    }

    // Abort when there's no text to highlight.
    if (!rectsAndTexts.textList.length)
      return;

    let outlineBox;
    if (rebuildOutline) {
      // Create the main (yellow) highlight outline box.
      outlineBox = document.createElementNS(kNSHTML, "div");
      outlineBox.setAttribute("id", kModalOutlineId);
    }

    const kModalOutlineTextId = kModalOutlineId + "-text";
    let i = 0;
    for (let rect of rectsAndTexts.rectList) {
      let text = rectsAndTexts.textList[i];

      // Next up is to check of the outline box' borders will not overlap with
      // rects that we drew before or will draw after this one.
      // We're taking the width of the border into account, which is
      // `kOutlineBoxBorderSize` pixels.
      // When left and/ or right sides will overlap with the current, previous
      // or next rect, make sure to make the necessary adjustments to the style.
      // These adjustments will override the styles as defined in `kModalStyles.outlineNode`.
      let intersectingSides = new Set();
      let previous = rectsAndTexts.rectList[i - 1];
      if (previous &&
          rect.left - previous.right <= 2 * kOutlineBoxBorderSize) {
        intersectingSides.add("left");
      }
      let next = rectsAndTexts.rectList[i + 1];
      if (next &&
          next.left - rect.right <= 2 * kOutlineBoxBorderSize) {
        intersectingSides.add("right");
      }
      let borderStyles = [...intersectingSides].map(side => [ "border-" + side, 0 ]);
      if (intersectingSides.size) {
        borderStyles.push([ "margin",  `-${kOutlineBoxBorderSize}px 0 0 ${
          intersectingSides.has("left") ? 0 : -kOutlineBoxBorderSize}px !important`]);
        borderStyles.push([ "border-radius",
          (intersectingSides.has("left") ? 0 : kOutlineBoxBorderRadius) + "px " +
          (intersectingSides.has("right") ? 0 : kOutlineBoxBorderRadius) + "px " +
          (intersectingSides.has("right") ? 0 : kOutlineBoxBorderRadius) + "px " +
          (intersectingSides.has("left") ? 0 : kOutlineBoxBorderRadius) + "px" ]);
      }

      ++i;
      let outlineStyle = this._getStyleString(kModalStyles.outlineNode, [
        ["top", rect.top + "px"],
        ["left", rect.left + "px"],
        ["height", rect.height + "px"],
        ["width", rect.width + "px"]
      ], borderStyles, kDebug ? kModalStyles.outlineNodeDebug : []);
      fontStyle.lineHeight = rect.height + "px";
      let textStyle = this._getStyleString(kModalStyles.outlineText) + "; " +
        this._getHTMLFontStyle(fontStyle);

      if (rebuildOutline) {
        let textBoxParent = (rectCount == 1) ? outlineBox :
          outlineBox.appendChild(document.createElementNS(kNSHTML, "div"));
        textBoxParent.setAttribute("style", outlineStyle);

        let textBox = document.createElementNS(kNSHTML, "span");
        if (rectCount == 1)
          textBox.setAttribute("id", kModalOutlineTextId);
        textBox.setAttribute("style", textStyle);
        textBox.textContent = text;
        textBoxParent.appendChild(textBox);
      } else {
        // Set the appropriate properties on the existing nodes, which will also
        // activate the transitions.
        outlineAnonNode.setAttributeForElement(kModalOutlineId, "style", outlineStyle);
        outlineAnonNode.setAttributeForElement(kModalOutlineTextId, "style", textStyle);
        outlineAnonNode.setTextContentForElement(kModalOutlineTextId, text);
      }
    }

    if (rebuildOutline) {
      dict.modalHighlightOutline = kDebug ?
        mockAnonymousContentNode((document.body ||
          document.documentElement).appendChild(outlineBox)) :
        document.insertAnonymousContent(outlineBox);
    }
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

    this._updateRangeRects(range);

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
    window = window.top;
    let dict = this.getForWindow(window);
    if (dict.modalHighlightOutline) {
      if (!dict.modalHighlightAllMask) {
        // Make sure to at least show the dimmed background.
        this._repaintHighlightAllMask(window, false);
        this._scheduleRepaintOfMask(window);
      } else {
        this._scheduleRepaintOfMask(window, { scrollOnly: true });
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
    window = window.top;
    let dict = this.getForWindow(window);

    const kMaskId = kModalIdPrefix + "-findbar-modalHighlight-outlineMask";
    if (!dict.modalHighlightAllMask) {
      let document = window.document;
      let maskNode = document.createElementNS(kNSHTML, "div");
      maskNode.setAttribute("id", kMaskId);
      dict.modalHighlightAllMask = kDebug ?
        mockAnonymousContentNode((document.body || document.documentElement).appendChild(maskNode)) :
        document.insertAnonymousContent(maskNode);
    }

    // Make sure the dimmed mask node takes the full width and height that's available.
    let {width, height} = dict.lastWindowDimensions = this._getWindowDimensions(window);
    if (typeof dict.brightText != "boolean" || dict.updateAllRanges)
      this._detectBrightText(dict);
    let maskStyle = this._getStyleString(kModalStyles.maskNode,
      [ ["width", width + "px"], ["height", height + "px"] ],
      dict.brightText ? kModalStyles.maskNodeBrightText : [],
      paintContent ? kModalStyles.maskNodeTransition : [],
      kDebug ? kModalStyles.maskNodeDebug : []);
    dict.modalHighlightAllMask.setAttributeForElement(kMaskId, "style", maskStyle);

    this._updateRangeOutline(dict);

    let allRects = [];
    if (paintContent || dict.modalHighlightAllMask) {
      this._updateDynamicRangesRects(dict);

      let DOMRect = window.DOMRect;
      for (let [range, rectsAndTexts] of dict.modalHighlightRectsMap) {
        if (dict.updateAllRanges)
          rectsAndTexts = this._updateRangeRects(range);

        // If a geometry change was detected, we bail out right away here, because
        // the current set of ranges has been invalidated.
        if (dict.detectedGeometryChange)
          return;

        for (let rect of rectsAndTexts.rectList)
          allRects.push(new DOMRect(rect.x, rect.y, rect.width, rect.height));
      }
      dict.updateAllRanges = false;
    }

    dict.modalHighlightAllMask.setCutoutRectsForElement(kMaskId, allRects);
  },

  /**
   * Safely remove the mask AnoymousContent node from the CanvasFrame.
   *
   * @param {nsIDOMWindow} window
   */
  _removeHighlightAllMask(window) {
    window = window.top;
    let dict = this.getForWindow(window);
    if (!dict.modalHighlightAllMask)
      return;

    // If the current window isn't the one the content was inserted into, this
    // will fail, but that's fine.
    if (kDebug) {
      dict.modalHighlightAllMask.remove();
    } else {
      try {
        window.document.removeAnonymousContent(dict.modalHighlightAllMask);
      } catch (ex) {}
    }
    dict.modalHighlightAllMask = null;
  },

  /**
   * Doing a full repaint each time a range is delivered by the highlight iterator
   * is way too costly, thus we pipe the frequency down to every
   * `kModalHighlightRepaintLoFreqMs` milliseconds. If there are dynamic ranges
   * found (see `_isInDynamicContainer()` for the definition), the frequency
   * will be upscaled to `kModalHighlightRepaintHiFreqMs`.
   *
   * @param {nsIDOMWindow} window
   * @param {Object}       options Dictionary of painter hints that contains the
   *                               following properties:
   *   {Boolean} contentChanged  Whether the documents' content changed in the
   *                             meantime. This happens when the DOM is updated
   *                             whilst the page is loaded.
   *   {Boolean} scrollOnly      TRUE when the page has scrolled in the meantime,
   *                             which means that the dynamically positioned
   *                             elements need to be repainted.
   *   {Boolean} updateAllRanges Whether to recalculate the rects of all ranges
   *                             that were found up until now.
   */
  _scheduleRepaintOfMask(window, { contentChanged, scrollOnly, updateAllRanges } =
                                 { contentChanged: false, scrollOnly: false, updateAllRanges: false }) {
    if (!this._modal)
      return;

    window = window.top;
    let dict = this.getForWindow(window);
    let hasDynamicRanges = !!dict.dynamicRangesSet.size;
    let repaintDynamicRanges = ((scrollOnly || contentChanged) && hasDynamicRanges);

    // When we request to repaint unconditionally, we mean to call
    // `_repaintHighlightAllMask()` right after the timeout.
    if (!dict.unconditionalRepaintRequested)
      dict.unconditionalRepaintRequested = !contentChanged || repaintDynamicRanges;
    // Some events, like a resize, call for recalculation of all the rects of all ranges.
    if (!dict.updateAllRanges)
      dict.updateAllRanges = updateAllRanges;

    if (dict.modalRepaintScheduler)
      return;

    dict.modalRepaintScheduler = window.setTimeout(() => {
      dict.modalRepaintScheduler = null;

      let { width: previousWidth, height: previousHeight } = dict.lastWindowDimensions;
      let { width, height } = dict.lastWindowDimensions = this._getWindowDimensions(window);
      let pageContentChanged = dict.detectedGeometryChange ||
                               (Math.abs(previousWidth - width) > kContentChangeThresholdPx ||
                                Math.abs(previousHeight - height) > kContentChangeThresholdPx);
      dict.detectedGeometryChange = false;
      // When the page has changed significantly enough in size, we'll restart
      // the iterator with the same parameters as before to find us new ranges.
      if (pageContentChanged)
        this.iterator.restart(this.finder);

      if (dict.unconditionalRepaintRequested ||
          (dict.modalHighlightRectsMap.size && pageContentChanged)) {
        dict.unconditionalRepaintRequested = false;
        this._repaintHighlightAllMask(window);
      }
    }, hasDynamicRanges ? kModalHighlightRepaintHiFreqMs : kModalHighlightRepaintLoFreqMs);
  },

  /**
   * Add event listeners to the content which will cause the modal highlight
   * AnonymousContent to be re-painted or hidden.
   *
   * @param {nsIDOMWindow} window
   */
  _addModalHighlightListeners(window) {
    window = window.top;
    let dict = this.getForWindow(window);
    if (dict.highlightListeners)
      return;

    window = window.top;
    dict.highlightListeners = [
      this._scheduleRepaintOfMask.bind(this, window, { contentChanged: true }),
      this._scheduleRepaintOfMask.bind(this, window, { updateAllRanges: true }),
      this._scheduleRepaintOfMask.bind(this, window, { scrollOnly: true }),
      this.hide.bind(this, window, null),
      () => dict.busySelecting = true
    ];
    let target = this.iterator._getDocShell(window).chromeEventHandler;
    target.addEventListener("MozAfterPaint", dict.highlightListeners[0]);
    target.addEventListener("resize", dict.highlightListeners[1]);
    target.addEventListener("scroll", dict.highlightListeners[2]);
    target.addEventListener("click", dict.highlightListeners[3]);
    target.addEventListener("selectstart", dict.highlightListeners[4]);
  },

  /**
   * Remove event listeners from content.
   *
   * @param {nsIDOMWindow} window
   */
  _removeModalHighlightListeners(window) {
    window = window.top;
    let dict = this.getForWindow(window);
    if (!dict.highlightListeners)
      return;

    let target = this.iterator._getDocShell(window).chromeEventHandler;
    target.removeEventListener("MozAfterPaint", dict.highlightListeners[0]);
    target.removeEventListener("resize", dict.highlightListeners[1]);
    target.removeEventListener("scroll", dict.highlightListeners[2]);
    target.removeEventListener("click", dict.highlightListeners[3]);
    target.removeEventListener("selectstart", dict.highlightListeners[4]);

    dict.highlightListeners = null;
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
    if (!selectionRange || !findRange)
      return false;
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
                       range.startOffset + aString.length);
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

      QueryInterface(iid) {
        if (iid.equals(Ci.nsIDocumentStateListener) ||
            iid.equals(Ci.nsISupports))
          return this;

        throw Components.results.NS_ERROR_NO_INTERFACE;
      },

      NotifyDocumentWillBeDestroyed() {
        this.findbar._onEditorDestruction(this);
      },

      // Unimplemented
      notifyDocumentCreated() {},
      notifyDocumentStateChanged(aDirty) {}
    };
  }
};
