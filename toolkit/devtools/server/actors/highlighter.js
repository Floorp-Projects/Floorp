/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Cc, Ci} = require("chrome");
const protocol = require("devtools/server/protocol");
const {Arg, Option, method} = protocol;
const events = require("sdk/event/core");
// Make sure the domnode type is known here
require("devtools/server/actors/inspector");

Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// FIXME: add ":visited" and ":link" after bug 713106 is fixed
const PSEUDO_CLASSES = [":hover", ":active", ":focus"];
const HIGHLIGHTED_PSEUDO_CLASS = ":-moz-devtools-highlighted";
let HELPER_SHEET = ".__fx-devtools-hide-shortcut__ { visibility: hidden !important } ";
HELPER_SHEET += ":-moz-devtools-highlighted { outline: 2px dashed #F06!important; outline-offset: -2px!important } ";
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const HIGHLIGHTER_PICKED_TIMER = 1000;

/**
 * The HighlighterActor is the server-side entry points for any tool that wishes
 * to highlight elements in the content document.
 *
 * The highlighter can be retrieved via the inspector's getHighlighter method.
 */

/**
 * The HighlighterActor class
 */
let HighlighterActor = protocol.ActorClass({
  typeName: "highlighter",

  initialize: function(inspector) {
    protocol.Actor.prototype.initialize.call(this, null);

    this._inspector = inspector;
    this._walker = this._inspector.walker;
    this._tabActor = this._inspector.tabActor;

    if (this._supportsBoxModelHighlighter()) {
      this._boxModelHighlighter = new BoxModelHighlighter(this._tabActor);
    } else {
      this._boxModelHighlighter = new SimpleOutlineHighlighter(this._tabActor);
    }
  },

  get conn() this._inspector && this._inspector.conn,

  /**
   * Can the host support the box model highlighter which requires a parent
   * XUL node to attach itself.
   */
  _supportsBoxModelHighlighter: function() {
    return this._tabActor.browser && !!this._tabActor.browser.parentNode;
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    if (this._boxModelHighlighter) {
      this._boxModelHighlighter.destroy();
      this._boxModelHighlighter = null;
    }
    this._inspector = null;
    this._walker = null;
    this._tabActor = null;
  },

  /**
   * Display the box model highlighting on a given NodeActor.
   * There is only one instance of the box model highlighter, so calling this
   * method several times won't display several highlighters, it will just move
   * the highlighter instance to these nodes.
   *
   * @param NodeActor The node to be highlighted
   * @param Options See the request part for existing options. Note that not
   * all options may be supported by all types of highlighters. The simple
   * outline highlighter for instance does not scrollIntoView
   */
  showBoxModel: method(function(node, options={}) {
    if (this._isNodeValidForHighlighting(node.rawNode)) {
      this._boxModelHighlighter.show(node.rawNode, options);
    }
  }, {
    request: {
      node: Arg(0, "domnode"),
      scrollIntoView: Option(1)
    }
  }),

  _isNodeValidForHighlighting: function(node) {
    // Is it null or dead?
    let isNotDead = node && !Cu.isDeadWrapper(node);

    // Is it connected to the document?
    let isConnected = false;
    try {
      let doc = node.ownerDocument;
      isConnected = (doc && doc.defaultView && doc.documentElement.contains(node));
    } catch (e) {
      // "can't access dead object" error
    }

    // Is it an element node
    let isElementNode = node.nodeType === Ci.nsIDOMNode.ELEMENT_NODE;

    return isNotDead && isConnected && isElementNode;
  },

  /**
   * Hide the box model highlighting if it was shown before
   */
  hideBoxModel: method(function() {
    this._boxModelHighlighter.hide();
  }, {
    request: {}
  }),

  /**
   * Pick a node on click, and highlight hovered nodes in the process.
   *
   * This method doesn't respond anything interesting, however, it starts
   * mousemove, and click listeners on the content document to fire
   * events and let connected clients know when nodes are hovered over or
   * clicked.
   *
   * Once a node is picked, events will cease, and listeners will be removed.
   */
  _isPicking: false,
  _hoveredNode: null,
  pick: method(function() {
    if (this._isPicking) {
      return null;
    }
    this._isPicking = true;

    this._preventContentEvent = event => {
      event.stopPropagation();
      event.preventDefault();
    };

    this._onPick = event => {
      this._preventContentEvent(event);
      this._stopPickerListeners();
      this._isPicking = false;
      this._tabActor.window.setTimeout(() => {
        this._boxModelHighlighter.hide();
      }, HIGHLIGHTER_PICKED_TIMER);
      events.emit(this._walker, "picker-node-picked", this._findAndAttachElement(event));
    };

    this._onHovered = event => {
      this._preventContentEvent(event);
      let res = this._findAndAttachElement(event);
      if (this._hoveredNode !== res.node) {
        this._boxModelHighlighter.show(res.node.rawNode);
        events.emit(this._walker, "picker-node-hovered", res);
        this._hoveredNode = res.node;
      }
    };

    this._tabActor.window.focus();
    this._startPickerListeners();

    return null;
  }),

  _findAndAttachElement: function(event) {
    let doc = event.target.ownerDocument;

    let x = event.clientX;
    let y = event.clientY;

    let node = doc.elementFromPoint(x, y);
    return this._walker.attachElement(node);
  },

  _startPickerListeners: function() {
    // Note that on b2g devices, tabActor.browser actually returns a window
    // object since the browser is in a separate process anyway.
    // In any case, adding the event listener will work, although it won't
    // fire for elements nested inside iframes
    let browser = this._tabActor.browser;
    browser.addEventListener("mousemove", this._onHovered, true);
    browser.addEventListener("click", this._onPick, true);
    browser.addEventListener("mousedown", this._preventContentEvent, true);
    browser.addEventListener("mouseup", this._preventContentEvent, true);
    browser.addEventListener("dblclick", this._preventContentEvent, true);
  },

  _stopPickerListeners: function() {
    let browser = this._tabActor.browser;
    browser.removeEventListener("mousemove", this._onHovered, true);
    browser.removeEventListener("click", this._onPick, true);
    browser.removeEventListener("mousedown", this._preventContentEvent, true);
    browser.removeEventListener("mouseup", this._preventContentEvent, true);
    browser.removeEventListener("dblclick", this._preventContentEvent, true);
  },

  cancelPick: method(function() {
    if (this._isPicking) {
      this._boxModelHighlighter.hide();
      this._stopPickerListeners();
      this._isPicking = false;
      this._hoveredNode = null;
    }
  })
});

exports.HighlighterActor = HighlighterActor;

/**
 * The HighlighterFront class
 */
let HighlighterFront = protocol.FrontClass(HighlighterActor, {});

/**
 * The BoxModelHighlighter is the class that actually draws the the box model
 * regions on top of a node.
 * It is used by the HighlighterActor.
 *
 * Usage example:
 *
 * let h = new BoxModelHighlighter(browser);
 * h.show(node);
 * h.hide();
 * h.destroy();
 *
 * Structure:
 *  <stack class="highlighter-container">
 *    <box class="highlighter-outline-container">
 *      <box class="highlighter-outline" />
 *    </box>
 *    <box class="highlighter-nodeinfobar-container">
 *      <box class="highlighter-nodeinfobar-positioner" position="top/bottom">
 *        <box class="highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-top"/>
 *        <hbox class="highlighter-nodeinfobar">
 *          <hbox class="highlighter-nodeinfobar-text">tagname#id.class1.class2</hbox>
 *        </hbox>
 *        <box class="highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-bottom"/>
 *      </box>
 *    </box>
 *  </stack>
 */
function BoxModelHighlighter(tabActor) {
  this.browser = tabActor.browser;
  this.win = tabActor.window;
  this.chromeDoc = this.browser.ownerDocument;
  this.chromeWin = this.chromeDoc.defaultView;

  this.layoutHelpers = new LayoutHelpers(this.win);
  this.chromeLayoutHelper = new LayoutHelpers(this.chromeWin);

  this.transitionDisabler = null;
  this.pageEventsMuter = null;
  this._update = this._update.bind(this);
  this.currentNode = null;

  this._initMarkup();
}

BoxModelHighlighter.prototype = {
  _initMarkup: function() {
    let stack = this.browser.parentNode;

    this.highlighterContainer = this.chromeDoc.createElement("stack");
    this.highlighterContainer.className = "highlighter-container";

    this.outline = this.chromeDoc.createElement("box");
    this.outline.className = "highlighter-outline";

    let outlineContainer = this.chromeDoc.createElement("box");
    outlineContainer.appendChild(this.outline);
    outlineContainer.className = "highlighter-outline-container";
    this.highlighterContainer.appendChild(outlineContainer);

    let infobarContainer = this.chromeDoc.createElement("box");
    infobarContainer.className = "highlighter-nodeinfobar-container";
    this.highlighterContainer.appendChild(infobarContainer);

    // Insert the highlighter right after the browser
    stack.insertBefore(this.highlighterContainer, stack.childNodes[1]);

    // Building the infobar
    let infobarPositioner = this.chromeDoc.createElement("box");
    infobarPositioner.className = "highlighter-nodeinfobar-positioner";
    infobarPositioner.setAttribute("position", "top");
    infobarPositioner.setAttribute("disabled", "true");

    let nodeInfobar = this.chromeDoc.createElement("hbox");
    nodeInfobar.className = "highlighter-nodeinfobar";

    let arrowBoxTop = this.chromeDoc.createElement("box");
    arrowBoxTop.className = "highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-top";

    let arrowBoxBottom = this.chromeDoc.createElement("box");
    arrowBoxBottom.className = "highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-bottom";

    let tagNameLabel = this.chromeDoc.createElementNS(XHTML_NS, "span");
    tagNameLabel.className = "highlighter-nodeinfobar-tagname";

    let idLabel = this.chromeDoc.createElementNS(XHTML_NS, "span");
    idLabel.className = "highlighter-nodeinfobar-id";

    let classesBox = this.chromeDoc.createElementNS(XHTML_NS, "span");
    classesBox.className = "highlighter-nodeinfobar-classes";

    let pseudoClassesBox = this.chromeDoc.createElementNS(XHTML_NS, "span");
    pseudoClassesBox.className = "highlighter-nodeinfobar-pseudo-classes";

    // Add some content to force a better boundingClientRect
    pseudoClassesBox.textContent = "&nbsp;";

    // <hbox class="highlighter-nodeinfobar-text"/>
    let texthbox = this.chromeDoc.createElement("hbox");
    texthbox.className = "highlighter-nodeinfobar-text";
    texthbox.setAttribute("align", "center");
    texthbox.setAttribute("flex", "1");

    texthbox.appendChild(tagNameLabel);
    texthbox.appendChild(idLabel);
    texthbox.appendChild(classesBox);
    texthbox.appendChild(pseudoClassesBox);

    nodeInfobar.appendChild(texthbox);

    infobarPositioner.appendChild(arrowBoxTop);
    infobarPositioner.appendChild(nodeInfobar);
    infobarPositioner.appendChild(arrowBoxBottom);

    infobarContainer.appendChild(infobarPositioner);

    let barHeight = infobarPositioner.getBoundingClientRect().height;

    this.nodeInfo = {
      tagNameLabel: tagNameLabel,
      idLabel: idLabel,
      classesBox: classesBox,
      pseudoClassesBox: pseudoClassesBox,
      positioner: infobarPositioner,
      barHeight: barHeight,
    };
  },

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function() {
    this.hide();

    this.chromeWin.clearTimeout(this.transitionDisabler);
    this.chromeWin.clearTimeout(this.pageEventsMuter);

    this._contentRect = null;
    this._highlightRect = null;
    this.outline = null;
    this.nodeInfo = null;

    this.highlighterContainer.remove();
    this.highlighterContainer = null;

    this.win = null
    this.browser = null;
    this.chromeDoc = null;
    this.chromeWin = null;
    this.currentNode = null;
  },

  /**
   * Show the highlighter on a given node
   *
   * @param {DOMNode} node
   */
  show: function(node, options={}) {
    if (!this.currentNode || node !== this.currentNode) {
      this.currentNode = node;

      this._showInfobar();
      this._computeZoomFactor();
      this._detachPageListeners();
      this._attachPageListeners();
      this._update();
      this._trackMutations();

      if (options.scrollIntoView) {
        this.chromeLayoutHelper.scrollIntoViewIfNeeded(node);
      }
    }
  },

  _trackMutations: function() {
    if (this.currentNode) {
      let win = this.currentNode.ownerDocument.defaultView;
      this.currentNodeObserver = win.MutationObserver(this._update);
      this.currentNodeObserver.observe(this.currentNode, {attributes: true});
    }
  },

  _untrackMutations: function() {
    if (this.currentNode) {
      if (this.currentNodeObserver) {
        // The following may fail with a "can't access dead object" exception
        // when the actor is being destroyed
        try {
          this.currentNodeObserver.disconnect();
        } catch (e) {}
        this.currentNodeObserver = null;
      }
    }
  },

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)).
   * Should be called whenever node size or attributes change
   * @param {Boolean} brieflyDisableTransitions
   *        In case _update is called during scrolling or repaint, set this
   *        to true to avoid transitions
   */
  _update: function(brieflyDisableTransitions) {
    if (this.currentNode) {
      let rect = this.layoutHelpers.getDirtyRect(this.currentNode);

      if (this._highlightRectangle(rect, brieflyDisableTransitions)) {
        this._moveInfobar();
        this._updateInfobar();
      } else {
        // Nothing to highlight (0px rectangle like a <script> tag for instance)
        this.hide();
      }
    }
  },

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  hide: function() {
    if (this.currentNode) {
      this._untrackMutations();
      this.currentNode = null;
      this._hideOutline();
      this._hideInfobar();
      this._detachPageListeners();
    }
  },

  /**
   * Hide the infobar
   */
  _hideInfobar: function() {
    this.nodeInfo.positioner.setAttribute("force-transitions", "true");
    this.nodeInfo.positioner.setAttribute("hidden", "true");
  },

  /**
   * Show the infobar
   */
  _showInfobar: function() {
    this.nodeInfo.positioner.removeAttribute("hidden");
    this._moveInfobar();
    this.nodeInfo.positioner.removeAttribute("force-transitions");
  },

  /**
   * Hide the outline
   */
  _hideOutline: function() {
    this.outline.setAttribute("hidden", "true");
  },

  /**
   * Show the outline
   */
  _showOutline: function() {
    this.outline.removeAttribute("hidden");
  },

  /**
   * Highlight a rectangular region.
   *
   * @param {object} aRect
   *        The rectangle region to highlight.
   * @param {boolean} brieflyDisableTransitions
   *        Set to true to avoid transitions during the highlighting
   * @return {boolean}
   *         True if the rectangle was highlighted, false otherwise.
   */
  _highlightRectangle: function(aRect, brieflyDisableTransitions) {
    if (!aRect) {
      return false;
    }

    let oldRect = this._contentRect;

    if (oldRect && aRect.top == oldRect.top && aRect.left == oldRect.left &&
        aRect.width == oldRect.width && aRect.height == oldRect.height) {
      return true; // same rectangle
    }

    let aRectScaled = this.layoutHelpers.getZoomedRect(this.win, aRect);
    let isShown = false;

    if (aRectScaled.left >= 0 && aRectScaled.top >= 0 &&
        aRectScaled.width > 0 && aRectScaled.height > 0) {

      // The bottom div and the right div are flexibles (flex=1).
      // We don't need to resize them.
      let top = "top:" + aRectScaled.top + "px;";
      let left = "left:" + aRectScaled.left + "px;";
      let width = "width:" + aRectScaled.width + "px;";
      let height = "height:" + aRectScaled.height + "px;";

      if (brieflyDisableTransitions) {
        this._brieflyDisableTransitions();
      }

      this.outline.setAttribute("style", top + left + width + height);

      isShown = true;
      this._showOutline();
    } else {
      // Only return false if the element really is invisible.
      // A height of 0 and a non-0 width corresponds to a visible element that
      // is below the fold for instance
      if (aRectScaled.width > 0 || aRectScaled.height > 0) {
        isShown = true;
        this._hideOutline();
      }
    }

    this._contentRect = aRect; // save orig (non-scaled) rect
    this._highlightRect = aRectScaled; // and save the scaled rect.

    return isShown;
  },

  /**
   * Update node information (tagName#id.class)
   */
  _updateInfobar: function() {
    if (this.currentNode) {
      // Tag name
      this.nodeInfo.tagNameLabel.textContent = this.currentNode.tagName;

      // ID
      this.nodeInfo.idLabel.textContent = this.currentNode.id ? "#" + this.currentNode.id : "";

      // Classes
      let classes = this.nodeInfo.classesBox;

      classes.textContent = this.currentNode.classList.length ?
                              "." + Array.join(this.currentNode.classList, ".") : "";

      // Pseudo-classes
      let pseudos = PSEUDO_CLASSES.filter(pseudo => {
        return DOMUtils.hasPseudoClassLock(this.currentNode, pseudo);
      }, this);

      let pseudoBox = this.nodeInfo.pseudoClassesBox;
      pseudoBox.textContent = pseudos.join("");
    }
  },

  /**
   * Move the Infobar to the right place in the highlighter.
   */
  _moveInfobar: function() {
    if (this._highlightRect) {
      let winHeight = this.win.innerHeight * this.zoom;
      let winWidth = this.win.innerWidth * this.zoom;

      let rect = {top: this._highlightRect.top,
                  left: this._highlightRect.left,
                  width: this._highlightRect.width,
                  height: this._highlightRect.height};

      rect.top = Math.max(rect.top, 0);
      rect.left = Math.max(rect.left, 0);
      rect.width = Math.max(rect.width, 0);
      rect.height = Math.max(rect.height, 0);

      rect.top = Math.min(rect.top, winHeight);
      rect.left = Math.min(rect.left, winWidth);

      this.nodeInfo.positioner.removeAttribute("disabled");
      // Can the bar be above the node?
      if (rect.top < this.nodeInfo.barHeight) {
        // No. Can we move the toolbar under the node?
        if (rect.top + rect.height +
            this.nodeInfo.barHeight > winHeight) {
          // No. Let's move it inside.
          this.nodeInfo.positioner.style.top = rect.top + "px";
          this.nodeInfo.positioner.setAttribute("position", "overlap");
        } else {
          // Yes. Let's move it under the node.
          this.nodeInfo.positioner.style.top = rect.top + rect.height + "px";
          this.nodeInfo.positioner.setAttribute("position", "bottom");
        }
      } else {
        // Yes. Let's move it on top of the node.
        this.nodeInfo.positioner.style.top =
          rect.top - this.nodeInfo.barHeight + "px";
        this.nodeInfo.positioner.setAttribute("position", "top");
      }

      let barWidth = this.nodeInfo.positioner.getBoundingClientRect().width;
      let left = rect.left + rect.width / 2 - barWidth / 2;

      // Make sure the whole infobar is visible
      if (left < 0) {
        left = 0;
        this.nodeInfo.positioner.setAttribute("hide-arrow", "true");
      } else {
        if (left + barWidth > winWidth) {
          left = winWidth - barWidth;
          this.nodeInfo.positioner.setAttribute("hide-arrow", "true");
        } else {
          this.nodeInfo.positioner.removeAttribute("hide-arrow");
        }
      }
      this.nodeInfo.positioner.style.left = left + "px";
    } else {
      this.nodeInfo.positioner.style.left = "0";
      this.nodeInfo.positioner.style.top = "0";
      this.nodeInfo.positioner.setAttribute("position", "top");
      this.nodeInfo.positioner.setAttribute("hide-arrow", "true");
    }
  },

  /**
   * Store page zoom factor.
   */
  _computeZoomFactor: function() {
    this.zoom =
      this.win.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDOMWindowUtils)
      .fullZoom;
  },

  _attachPageListeners: function() {
    this.browser.addEventListener("resize", this, true);
    this.browser.addEventListener("scroll", this, true);
    this.browser.addEventListener("MozAfterPaint", this, true);
  },

  _detachPageListeners: function() {
    this.browser.removeEventListener("resize", this, true);
    this.browser.removeEventListener("scroll", this, true);
    this.browser.removeEventListener("MozAfterPaint", this, true);
  },

  /**
   * Generic event handler.
   *
   * @param nsIDOMEvent aEvent
   *        The DOM event object.
   */
  handleEvent: function(event) {
    switch (event.type) {
      case "resize":
        this._computeZoomFactor();
        break;
      case "MozAfterPaint":
      case "scroll":
        this._update(true);
        break;
    }
  },

  /**
   * Disable the CSS transitions for a short time to avoid laggy animations
   * during scrolling or resizing.
   */
  _brieflyDisableTransitions: function() {
    if (this.transitionDisabler) {
      this.chromeWin.clearTimeout(this.transitionDisabler);
    } else {
      this.outline.setAttribute("disable-transitions", "true");
      this.nodeInfo.positioner.setAttribute("disable-transitions", "true");
    }
    this.transitionDisabler =
      this.chromeWin.setTimeout(() => {
        this.outline.removeAttribute("disable-transitions");
        this.nodeInfo.positioner.removeAttribute("disable-transitions");
        this.transitionDisabler = null;
      }, 500);
  }
};

/**
 * The SimpleOutlineHighlighter is a class that has the same API than the
 * BoxModelHighlighter, but adds a pseudo-class on the target element itself
 * to draw a simple outline.
 * It is used by the HighlighterActor too, but in case the more complex
 * BoxModelHighlighter can't be attached (which is the case for FirefoxOS and
 * Fennec targets for instance).
 */
function SimpleOutlineHighlighter(tabActor) {
  this.chromeDoc = tabActor.window.document;
}

SimpleOutlineHighlighter.prototype = {
  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function() {
    this.hide();
    if (this.installedHelpers) {
      this.installedHelpers.clear();
    }
    this.chromeDoc = null;
  },

  _installHelperSheet: function(node) {
    if (!this.installedHelpers) {
      this.installedHelpers = new WeakMap;
    }
    let win = node.ownerDocument.defaultView;
    if (!this.installedHelpers.has(win)) {
      let {Style} = require("sdk/stylesheet/style");
      let {attach} = require("sdk/content/mod");
      let style = Style({source: HELPER_SHEET, type: "agent"});
      attach(style, win);
      this.installedHelpers.set(win, style);
    }
  },

  /**
   * Show the highlighter on a given node
   * @param {DOMNode} node
   */
  show: function(node) {
    if (!this.currentNode || node !== this.currentNode) {
      this.hide();
      this.currentNode = node;
      this._installHelperSheet(node);
      DOMUtils.addPseudoClassLock(node, HIGHLIGHTED_PSEUDO_CLASS);
    }
  },

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  hide: function() {
    if (this.currentNode) {
      DOMUtils.removePseudoClassLock(this.currentNode, HIGHLIGHTED_PSEUDO_CLASS);
      this.currentNode = null;
    }
  }
};

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils)
});
