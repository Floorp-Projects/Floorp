/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Cc, Ci} = require("chrome");
const Services = require("Services");
const protocol = require("devtools/server/protocol");
const {Arg, Option, method} = protocol;
const events = require("sdk/event/core");
const Heritage = require("sdk/core/heritage");

const {CssLogic} = require("devtools/styleinspector/css-logic");
const EventEmitter = require("devtools/toolkit/event-emitter");
const GUIDE_STROKE_WIDTH = 1;

// Make sure the domnode type is known here
require("devtools/server/actors/inspector");

Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// FIXME: add ":visited" and ":link" after bug 713106 is fixed
const PSEUDO_CLASSES = [":hover", ":active", ":focus"];
const HIGHLIGHTED_PSEUDO_CLASS = ":-moz-devtools-highlighted";
let HELPER_SHEET = ".__fx-devtools-hide-shortcut__ { visibility: hidden !important } ";
HELPER_SHEET += ":-moz-devtools-highlighted { outline: 2px dashed #F06!important; outline-offset: -2px!important } ";
const XHTML_NS = "http://www.w3.org/1999/xhtml";
const SVG_NS = "http://www.w3.org/2000/svg";
const HIGHLIGHTER_PICKED_TIMER = 1000;
const INFO_BAR_OFFSET = 5;
// The minimum distance a line should be before it has an arrow marker-end
const ARROW_LINE_MIN_DISTANCE = 10;
// How many maximum nodes can be highlighted at the same time by the
// SelectorHighlighter
const MAX_HIGHLIGHTED_ELEMENTS = 100;

// All possible highlighter classes
let HIGHLIGHTER_CLASSES = exports.HIGHLIGHTER_CLASSES = {
  "BoxModelHighlighter": BoxModelHighlighter,
  "CssTransformHighlighter": CssTransformHighlighter,
  "SelectorHighlighter": SelectorHighlighter
};

/**
 * The Highlighter is the server-side entry points for any tool that wishes to
 * highlight elements in some way in the content document.
 *
 * A little bit of vocabulary:
 * - <something>HighlighterActor classes are the actors that can be used from
 *   the client. They do very little else than instantiate a given
 *   <something>Highlighter and use it to highlight elements.
 * - <something>Highlighter classes aren't actors, they're just JS classes that
 *   know how to create and attach the actual highlighter elements on top of the
 *   content
 *
 * The most used highlighter actor is the HighlighterActor which can be
 * conveniently retrieved via the InspectorActor's 'getHighlighter' method.
 * The InspectorActor will always return the same instance of
 * HighlighterActor if asked several times and this instance is used in the
 * toolbox to highlighter elements's box-model from the markup-view, layout-view,
 * console, debugger, ... as well as select elements with the pointer (pick).
 *
 * Other types of highlighter actors exist and can be accessed via the
 * InspectorActor's 'getHighlighterByType' method.
 */

/**
 * The HighlighterActor class
 */
let HighlighterActor = exports.HighlighterActor = protocol.ActorClass({
  typeName: "highlighter",

  initialize: function(inspector, autohide) {
    protocol.Actor.prototype.initialize.call(this, null);

    this._autohide = autohide;
    this._inspector = inspector;
    this._walker = this._inspector.walker;
    this._tabActor = this._inspector.tabActor;

    this._highlighterReady = this._highlighterReady.bind(this);
    this._highlighterHidden = this._highlighterHidden.bind(this);

    if (supportXULBasedHighlighter(this._tabActor)) {
      this._boxModelHighlighter =
        new BoxModelHighlighter(this._tabActor, this._inspector);

        this._boxModelHighlighter.on("ready", this._highlighterReady);
        this._boxModelHighlighter.on("hide", this._highlighterHidden);
    } else {
      this._boxModelHighlighter = new SimpleOutlineHighlighter(this._tabActor);
    }
  },

  get conn() this._inspector && this._inspector.conn,

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    if (this._boxModelHighlighter) {
      if (supportXULBasedHighlighter(this._tabActor)) {
        this._boxModelHighlighter.off("ready", this._highlighterReady);
        this._boxModelHighlighter.off("hide", this._highlighterHidden);
      }
      this._boxModelHighlighter.destroy();
      this._boxModelHighlighter = null;
    }
    this._autohide = null;
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
   * all options may be supported by all types of highlighters.
   */
  showBoxModel: method(function(node, options={}) {
    if (node && isNodeValid(node.rawNode)) {
      this._boxModelHighlighter.show(node.rawNode, options);
    } else {
      this._boxModelHighlighter.hide();
    }
  }, {
    request: {
      node: Arg(0, "domnode"),
      region: Option(1)
    }
  }),

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
      if (this._autohide) {
        this._tabActor.window.setTimeout(() => {
          this._boxModelHighlighter.hide();
        }, HIGHLIGHTER_PICKED_TIMER);
      }
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

  /**
   * Get the right target for listening to mouse events while in pick mode.
   * - On a firefox desktop content page: tabActor is a BrowserTabActor from
   *   which the browser property will give us a target we can use to listen to
   *   events, even in nested iframes.
   * - On B2G: tabActor is a ContentActor which doesn't have a browser but
   *   since it overrides BrowserTabActor, it does get a browser property
   *   anyway, which points to its window object.
   * - When using the Browser Toolbox (to inspect firefox desktop): tabActor is
   *   the RootActor, in which case, the window property can be used to listen
   *   to events
   */
  _getPickerListenerTarget: function() {
    let actor = this._tabActor;
    return actor.isRootActor ? actor.window : actor.chromeEventHandler;
  },

  _startPickerListeners: function() {
    let target = this._getPickerListenerTarget();
    target.addEventListener("mousemove", this._onHovered, true);
    target.addEventListener("click", this._onPick, true);
    target.addEventListener("mousedown", this._preventContentEvent, true);
    target.addEventListener("mouseup", this._preventContentEvent, true);
    target.addEventListener("dblclick", this._preventContentEvent, true);
  },

  _stopPickerListeners: function() {
    let target = this._getPickerListenerTarget();
    target.removeEventListener("mousemove", this._onHovered, true);
    target.removeEventListener("click", this._onPick, true);
    target.removeEventListener("mousedown", this._preventContentEvent, true);
    target.removeEventListener("mouseup", this._preventContentEvent, true);
    target.removeEventListener("dblclick", this._preventContentEvent, true);
  },

  _highlighterReady: function() {
    events.emit(this._inspector.walker, "highlighter-ready");
  },

  _highlighterHidden: function() {
    events.emit(this._inspector.walker, "highlighter-hide");
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

let HighlighterFront = protocol.FrontClass(HighlighterActor, {});

/**
 * A generic highlighter actor class that instantiate a highlighter given its
 * type name and allows to show/hide it.
 */
let CustomHighlighterActor = exports.CustomHighlighterActor = protocol.ActorClass({
  typeName: "customhighlighter",

  /**
   * Create a highlighter instance given its typename
   * The typename must be one of HIGHLIGHTER_CLASSES and the class must
   * implement constructor(tabActor), show(node), hide(), destroy()
   */
  initialize: function(inspector, typeName) {
    protocol.Actor.prototype.initialize.call(this, null);

    this._inspector = inspector;

    let constructor = HIGHLIGHTER_CLASSES[typeName];
    if (!constructor) {
      throw new Error(typeName + " isn't a valid highlighter class (" +
        Object.keys(HIGHLIGHTER_CLASSES) + ")");
      return;
    }

    // The assumption is that all custom highlighters need a XUL parent in the
    // browser to append their elements
    if (supportXULBasedHighlighter(inspector.tabActor)) {
      this._highlighter = new constructor(inspector.tabActor);
    }
  },

  get conn() this._inspector && this._inspector.conn,

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    this.finalize();
  },

  /**
   * Display the highlighter on a given NodeActor.
   * @param NodeActor The node to be highlighted
   * @param Object Options for the custom highlighter
   */
  show: method(function(node, options) {
    if (!node || !isNodeValid(node.rawNode) || !this._highlighter) {
      return;
    }

    this._highlighter.show(node.rawNode, options);
  }, {
    request: {
      node: Arg(0, "domnode"),
      options: Arg(1, "nullable:json")
    }
  }),

  /**
   * Hide the highlighter if it was shown before
   */
  hide: method(function() {
    if (this._highlighter) {
      this._highlighter.hide();
    }
  }, {
    request: {}
  }),

  /**
   * Kill this actor. This method is called automatically just before the actor
   * is destroyed.
   */
  finalize: method(function() {
    if (this._highlighter) {
      this._highlighter.destroy();
      this._highlighter = null;
    }
  }, {
    oneway: true
  })
});

let CustomHighlighterFront = protocol.FrontClass(CustomHighlighterActor, {});

/**
 * Parent class for XUL-based complex highlighter that are inserted in the
 * parent browser structure
 */
function XULBasedHighlighter(tabActor) {
  this.browser = tabActor.browser;
  this.win = tabActor.window;
  this.chromeDoc = this.browser.ownerDocument;
  this.currentNode = null;

  this.update = this.update.bind(this);
}

XULBasedHighlighter.prototype = {
  /**
   * Show the highlighter on a given node
   * @param {DOMNode} node
   * @param {Object} options
   *        Object used for passing options
   */
  show: function(node, options={}) {
    if (!isNodeValid(node) || node === this.currentNode) {
      return;
    }

    this.options = options;

    this._detachPageListeners();
    this.currentNode = node;
    this._attachPageListeners();
    this._show();
  },

  /**
   * Hide the highlighter
   */
  hide: function() {
    if (!isNodeValid(this.currentNode)) {
      return;
    }

    this._hide();
    this._detachPageListeners();
    this.currentNode = null;
    this.options = null;
  },

  /**
   * Update the highlighter while shown
   */
  update: function() {
    if (isNodeValid(this.currentNode)) {
      this._update();
    }
  },

  _show: function() {
    // To be implemented by sub classes
    // When called, sub classes should actually show the highlighter for
    // this.currentNode
  },

  _update: function() {
    // To be implemented by sub classes
    // When called, sub classes should update the highlighter shown for
    // this.currentNode
    // This is called as a result of a page scroll, zoom or repaint
  },

  _hide: function() {
    // To be implemented by sub classes
    // When called, sub classes should actually hide the highlighter
  },

  /**
   * Listen to changes on the content page to update the highlighter
   */
  _attachPageListeners: function() {
    if (isNodeValid(this.currentNode)) {
      let win = this.currentNode.ownerDocument.defaultView;
      this.browser.addEventListener("MozAfterPaint", this.update);
    }
  },

  /**
   * Stop listening to page changes
   */
  _detachPageListeners: function() {
    if (isNodeValid(this.currentNode)) {
      let win = this.currentNode.ownerDocument.defaultView;
      this.browser.removeEventListener("MozAfterPaint", this.update);
    }
  },

  destroy: function() {
    this.hide();

    this.win = null;
    this.browser = null;
    this.chromeDoc = null;
    this.currentNode = null;
  }
};

/**
 * The BoxModelHighlighter is the class that actually draws the the box model
 * regions on top of a node.
 * It is used by the HighlighterActor.
 *
 * Usage example:
 *
 * let h = new BoxModelHighlighter(browser);
 * h.show(node, options);
 * h.hide();
 * h.destroy();
 *
 * Available options:
 * - region {String}
 *   "content", "padding", "border" or "margin"
 *   This specifies the region that the guides should outline.
 *   Defaults to "content"
 * - hideGuides {Boolean}
 *   Defaults to false
 * - hideInfoBar {Boolean}
 *   Defaults to false
 * - showOnly {String}
 *   "content", "padding", "border" or "margin"
 *    If set, only this region will be highlighted
 *
 * Structure:
 * <stack class="highlighter-container">
 *   <svg class="box-model-root" hidden="true">
 *     <g class="box-model-container">
 *       <polygon class="box-model-margin" points="317,122 747,36 747,181 317,267" />
 *       <polygon class="box-model-border" points="317,128 747,42 747,161 317,247" />
 *       <polygon class="box-model-padding" points="323,127 747,42 747,161 323,246" />
 *       <polygon class="box-model-content" points="335,137 735,57 735,152 335,232" />
 *     </g>
 *     <line class="box-model-guide-top" x1="0" y1="592" x2="99999" y2="592" />
 *     <line class="box-model-guide-right" x1="735" y1="0" x2="735" y2="99999" />
 *     <line class="box-model-guide-bottom" x1="0" y1="612" x2="99999" y2="612" />
 *     <line class="box-model-guide-left" x1="334" y1="0" x2="334" y2="99999" />
 *   </svg>
 *   <box class="highlighter-nodeinfobar-container">
 *     <box class="highlighter-nodeinfobar-positioner" position="top" />
 *       <box class="highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-top" />
 *       <hbox class="highlighter-nodeinfobar">
 *         <hbox class="highlighter-nodeinfobar-text" align="center" flex="1">
 *           <span class="highlighter-nodeinfobar-tagname">Node name</span>
 *           <span class="highlighter-nodeinfobar-id">Node id</span>
 *           <span class="highlighter-nodeinfobar-classes">.someClass</span>
 *           <span class="highlighter-nodeinfobar-pseudo-classes">:hover</span>
 *         </hbox>
 *       </hbox>
 *       <box class="highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-bottom"/>
 *     </box>
 *   </box>
 * </stack>
 */
function BoxModelHighlighter(tabActor) {
  XULBasedHighlighter.call(this, tabActor);
  this.layoutHelpers = new LayoutHelpers(this.win);
  this._initMarkup();
  EventEmitter.decorate(this);

  /**
   * Optionally customize each region's fill color by adding an entry to the
   * regionFill property: `highlighter.regionFill.margin = "red";
   */
  this.regionFill = {};

  this._currentNode = null;
}

BoxModelHighlighter.prototype = Heritage.extend(XULBasedHighlighter.prototype, {
  get zoom() {
    return this.win.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils).fullZoom;
  },

  get currentNode() {
    return this._currentNode;
  },

  set currentNode(node) {
    this._currentNode = node;
    this._computedStyle = null;
  },

  _initMarkup: function() {
    let stack = this.browser.parentNode;

    this._highlighterContainer = this.chromeDoc.createElement("stack");
    this._highlighterContainer.className = "highlighter-container";

    this._svgRoot = this._createSVGNode("root", "svg", this._highlighterContainer);

    // Set the SVG canvas height to 0 to stop content jumping around on small
    // screens.
    this._svgRoot.setAttribute("height", "0");

    this._boxModelContainer = this._createSVGNode("container", "g", this._svgRoot);

    this._boxModelNodes = {
      margin: this._createSVGNode("margin", "polygon", this._boxModelContainer),
      border: this._createSVGNode("border", "polygon", this._boxModelContainer),
      padding: this._createSVGNode("padding", "polygon", this._boxModelContainer),
      content: this._createSVGNode("content", "polygon", this._boxModelContainer)
    };

    this._guideNodes = {
      top: this._createSVGNode("guide-top", "line", this._svgRoot),
      right: this._createSVGNode("guide-right", "line", this._svgRoot),
      bottom: this._createSVGNode("guide-bottom", "line", this._svgRoot),
      left: this._createSVGNode("guide-left", "line", this._svgRoot)
    };

    this._guideNodes.top.setAttribute("stroke-width", GUIDE_STROKE_WIDTH);
    this._guideNodes.right.setAttribute("stroke-width", GUIDE_STROKE_WIDTH);
    this._guideNodes.bottom.setAttribute("stroke-width", GUIDE_STROKE_WIDTH);
    this._guideNodes.left.setAttribute("stroke-width", GUIDE_STROKE_WIDTH);

    this._highlighterContainer.appendChild(this._svgRoot);

    let infobarContainer = this.chromeDoc.createElement("box");
    infobarContainer.className = "highlighter-nodeinfobar-container";
    this._highlighterContainer.appendChild(infobarContainer);

    // Insert the highlighter right after the browser
    stack.insertBefore(this._highlighterContainer, stack.childNodes[1]);

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

    let dimensionBox = this.chromeDoc.createElementNS(XHTML_NS, "span");
    dimensionBox.className = "highlighter-nodeinfobar-dimensions";

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
    texthbox.appendChild(dimensionBox);

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
      dimensionBox: dimensionBox,
      positioner: infobarPositioner,
      barHeight: barHeight,
    };
  },

  _createSVGNode: function(classPostfix, nodeType, parent) {
    let node = this.chromeDoc.createElementNS(SVG_NS, nodeType);
    node.setAttribute("class", "box-model-" + classPostfix);

    parent.appendChild(node);

    return node;
  },

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function() {
    XULBasedHighlighter.prototype.destroy.call(this);

    this._highlighterContainer.remove();
    this._highlighterContainer = null;

    this.nodeInfo = null;
    this._currentNode = null;
  },

  /**
   * Show the highlighter on a given node. We override this method so that the
   * same node can be rehighlighted e.g. to highlight different regions from the
   * layout view.
   *
   * @param {DOMNode} node
   * @param {Object} options
   *        Object used for passing options
   */
  show: function(node, options={}) {
    if (!isNodeValid(node)) {
      return;
    }

    this.options = options;

    if (!this.options.region) {
      this.options.region = "content";
    }

    this._detachPageListeners();
    this.currentNode = node;
    this._attachPageListeners();
    this._show();
  },

  /**
   * Show the highlighter on a given node
   */
  _show: function() {
    this._update();
    this._trackMutations();
    this.emit("ready");
  },

  /**
   * Track the current node markup mutations so that the node info bar can be
   * updated to reflects the node's attributes
   */
  _trackMutations: function() {
    if (isNodeValid(this.currentNode)) {
      let win = this.currentNode.ownerDocument.defaultView;
      this.currentNodeObserver = new win.MutationObserver(this.update);
      this.currentNodeObserver.observe(this.currentNode, {attributes: true});
    }
  },

  _untrackMutations: function() {
    if (isNodeValid(this.currentNode) && this.currentNodeObserver) {
      this.currentNodeObserver.disconnect();
      this.currentNodeObserver = null;
    }
  },

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)).
   * Should be called whenever node size or attributes change
   */
  _update: function() {
    if (this._updateBoxModel()) {
      if (!this.options.hideInfoBar) {
        this._showInfobar();
      }
      this._showBoxModel();
    } else {
      // Nothing to highlight (0px rectangle like a <script> tag for instance)
      this._hide();
    }
  },

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide: function() {
    this._untrackMutations();
    this._hideBoxModel();
    this._hideInfobar();
  },

  /**
   * Hide the infobar
   */
  _hideInfobar: function() {
    this.nodeInfo.positioner.setAttribute("hidden", "true");
  },

  /**
   * Show the infobar
   */
  _showInfobar: function() {
    this.nodeInfo.positioner.removeAttribute("hidden");
    this._updateInfobar();
  },

  /**
   * Hide the box model
   */
  _hideBoxModel: function() {
    this._svgRoot.setAttribute("hidden", "true");
  },

  /**
   * Show the box model
   */
  _showBoxModel: function() {
    this._svgRoot.removeAttribute("hidden");
  },

  /**
   * Update the box model as per the current node.
   *
   * @return {boolean}
   *         True if the current node has a box model to be highlighted
   */
  _updateBoxModel: function() {
    this.options.region = this.options.region || "content";

    if (this._nodeNeedsHighlighting()) {
      for (let boxType in this._boxModelNodes) {

        let quads = this.layoutHelpers.getAdjustedQuads(this.currentNode, boxType);
        if (!quads) {
          continue;
        }
        let {p1, p2, p3, p4} = quads;

        let boxNode = this._boxModelNodes[boxType];

        if (this.regionFill[boxType]) {
          boxNode.setAttribute("style", "fill:" + this.regionFill[boxType]);
        } else {
          boxNode.removeAttribute("style");
        }

        if (!this.options.showOnly || this.options.showOnly === boxType) {
          boxNode.setAttribute("points",
                               p1.x + "," + p1.y + " " +
                               p2.x + "," + p2.y + " " +
                               p3.x + "," + p3.y + " " +
                               p4.x + "," + p4.y);
        } else {
          boxNode.setAttribute("points", "");
        }

        if (boxType === this.options.region && !this.options.hideGuides) {
          this._showGuides(p1, p2, p3, p4);
        } else if (this.options.hideGuides) {
          this._hideGuides();
        }
      }

      return true;
    }

    this._hideBoxModel();
    return false;
  },

  _nodeNeedsHighlighting: function() {
    if (!this.currentNode ||
        Cu.isDeadWrapper(this.currentNode) ||
        this.currentNode.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE ||
        !this.currentNode.ownerDocument ||
        !this.currentNode.ownerDocument.defaultView) {
      return false;
    }

    if (!this._computedStyle) {
      this._computedStyle = CssLogic.getComputedStyle(this.currentNode);
    }

    return this._computedStyle.getPropertyValue("display") !== "none";
  },

  _getOuterBounds: function() {
    for (let region of ["margin", "border", "padding", "content"]) {
      let quads = this.layoutHelpers.getAdjustedQuads(this.currentNode, region);

      if (!quads) {
        // Invisible element such as a script tag.
        break;
      }

      let {bottom, height, left, right, top, width, x, y} = quads.bounds;

      if (width > 0 || height > 0) {
        return this._boundsHelper(bottom, height, left, right, top, width, x, y);
      }
    }

    return this._boundsHelper();
  },

  _boundsHelper: function(bottom=0, height=0, left=0, right=0,
                          top=0, width=0, x=0, y=0) {
    return {
      bottom: bottom,
      height: height,
      left: left,
      right: right,
      top: top,
      width: width,
      x: x,
      y: y
    };
  },

  /**
   * We only want to show guides for horizontal and vertical edges as this helps
   * to line them up. This method finds these edges and displays a guide there.
   *
   * @param  {DOMPoint} p1
   * @param  {DOMPoint} p2
   * @param  {DOMPoint} p3
   * @param  {DOMPoint} p4
   */
  _showGuides: function(p1, p2, p3, p4) {
    let allX = [p1.x, p2.x, p3.x, p4.x].sort((a, b) => a - b);
    let allY = [p1.y, p2.y, p3.y, p4.y].sort((a, b) => a - b);
    let toShowX = [];
    let toShowY = [];

    for (let arr of [allX, allY]) {
      for (let i = 0; i < arr.length; i++) {
        let val = arr[i];

        if (i !== arr.lastIndexOf(val)) {
          if (arr === allX) {
            toShowX.push(val);
          } else {
            toShowY.push(val);
          }
          arr.splice(arr.lastIndexOf(val), 1);
        }
      }
    }

    // Move guide into place or hide it if no valid co-ordinate was found.
    this._updateGuide(this._guideNodes.top, toShowY[0]);
    this._updateGuide(this._guideNodes.right, toShowX[1]);
    this._updateGuide(this._guideNodes.bottom, toShowY[1]);
    this._updateGuide(this._guideNodes.left, toShowX[0]);
  },

  _hideGuides: function() {
    for (let side in this._guideNodes) {
      this._guideNodes[side].setAttribute("hidden", "true");
    }
  },

  /**
   * Move a guide to the appropriate position and display it. If no point is
   * passed then the guide is hidden.
   *
   * @param  {SVGLine} guide
   *         The guide to update
   * @param  {Integer} point
   *         x or y co-ordinate. If this is undefined we hide the guide.
   */
  _updateGuide: function(guide, point=-1) {
    if (point <= 0) {
      guide.setAttribute("hidden", "true");
      return false;
    }

    let offset = GUIDE_STROKE_WIDTH / 2;

    if (guide === this._guideNodes.top || guide === this._guideNodes.left) {
      point -= offset;
    } else {
      point += offset;
    }

    if (guide === this._guideNodes.top || guide === this._guideNodes.bottom) {
      guide.setAttribute("x1", 0);
      guide.setAttribute("y1", point);
      guide.setAttribute("x2", "100%");
      guide.setAttribute("y2", point);
    } else {
      guide.setAttribute("x1", point);
      guide.setAttribute("y1", 0);
      guide.setAttribute("x2", point);
      guide.setAttribute("y2", "100%");
    }
    guide.removeAttribute("hidden");

    return true;
  },

  /**
   * Update node information (tagName#id.class)
   */
  _updateInfobar: function() {
    if (!this.currentNode) {
      return;
    }

    let info = this.nodeInfo;

    let {bindingElement:node, pseudo} =
      CssLogic.getBindingElementAndPseudo(this.currentNode);

    // Update the tag, id, classes, pseudo-classes and dimensions only if they
    // changed to avoid triggering paint events
    let tagName = node.tagName;
    if (info.tagNameLabel.textContent !== tagName) {
      info.tagNameLabel.textContent = tagName;
    }

    let id = node.id ? "#" + node.id : "";
    if (info.idLabel.textContent !== id) {
      info.idLabel.textContent = id;
    }

    let classList = (node.classList || []).length ? "." + [...node.classList].join(".") : "";
    if (info.classesBox.textContent !== classList) {
      info.classesBox.textContent = classList;
    }

    let pseudos = PSEUDO_CLASSES.filter(pseudo => {
      return DOMUtils.hasPseudoClassLock(node, pseudo);
    }, this).join("");

    if (pseudo) {
      // Display :after as ::after
      pseudos += ":" + pseudo;
    }

    if (info.pseudoClassesBox.textContent !== pseudos) {
      info.pseudoClassesBox.textContent = pseudos;
    }

    let rect = node.getBoundingClientRect();
    let dim = Math.ceil(rect.width) + " x " + Math.ceil(rect.height);
    if (info.dimensionBox.textContent !== dim) {
      info.dimensionBox.textContent = dim;
    }

    this._moveInfobar();
  },

  /**
   * Move the Infobar to the right place in the highlighter.
   */
  _moveInfobar: function() {
    let bounds = this._getOuterBounds();
    let winHeight = this.win.innerHeight * this.zoom;
    let winWidth = this.win.innerWidth * this.zoom;

    // Ensure that positionerBottom and positionerTop are at least zero to avoid
    // showing tooltips outside the viewport.
    let positionerBottom = Math.max(0, bounds.bottom);
    let positionerTop = Math.max(0, bounds.top);

    // Avoid showing the nodeInfoBar on top of the findbar or awesomebar.
    if (this.chromeDoc.defaultView.gBrowser) {
      // Get the y co-ordinate of the top of the viewport
      let viewportTop = this.browser.getBoundingClientRect().top;

      // Get the offset to the top of the findbar
      let findbar = this.chromeDoc.defaultView.gBrowser.getFindBar();
      let findTop = findbar.getBoundingClientRect().top - viewportTop;

      // Either show the positioner where it is or move it above the findbar.
      positionerTop = Math.min(positionerTop, findTop);
    }

    this.nodeInfo.positioner.removeAttribute("disabled");
    // Can the bar be above the node?
    if (positionerTop < this.nodeInfo.barHeight) {
      // No. Can we move the toolbar under the node?
      if (positionerBottom + this.nodeInfo.barHeight > winHeight) {
        // No. Let's move it inside.
        this.nodeInfo.positioner.style.top = positionerTop + "px";
        this.nodeInfo.positioner.setAttribute("position", "overlap");
      } else {
        // Yes. Let's move it under the node.
        this.nodeInfo.positioner.style.top = positionerBottom - INFO_BAR_OFFSET + "px";
        this.nodeInfo.positioner.setAttribute("position", "bottom");
      }
    } else {
      // Yes. Let's move it on top of the node.
      this.nodeInfo.positioner.style.top =
        positionerTop + INFO_BAR_OFFSET - this.nodeInfo.barHeight + "px";
      this.nodeInfo.positioner.setAttribute("position", "top");
    }

    let barWidth = this.nodeInfo.positioner.getBoundingClientRect().width;
    let left = bounds.right - bounds.width / 2 - barWidth / 2;

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
  }
});

/**
 * The CssTransformHighlighter is the class that draws an outline around a
 * transformed element and an outline around where it would be if untransformed
 * as well as arrows connecting the 2 outlines' corners.
 */
function CssTransformHighlighter(tabActor) {
  XULBasedHighlighter.call(this, tabActor);

  this.layoutHelpers = new LayoutHelpers(tabActor.window);
  this._initMarkup();
}

let MARKER_COUNTER = 1;

CssTransformHighlighter.prototype = Heritage.extend(XULBasedHighlighter.prototype, {
  _initMarkup: function() {
    let stack = this.browser.parentNode;

    this._container = this.chromeDoc.createElement("stack");
    this._container.className = "highlighter-container";

    this._svgRoot = this._createSVGNode("root", "svg", this._container);
    this._svgRoot.setAttribute("hidden", "true");

    // Add a marker tag to the svg root for the arrow tip
    let marker = this.chromeDoc.createElementNS(SVG_NS, "marker");
    this.markerId = "css-transform-arrow-marker-" + MARKER_COUNTER;
    MARKER_COUNTER ++;
    marker.setAttribute("id", this.markerId);
    marker.setAttribute("markerWidth", "10");
    marker.setAttribute("markerHeight", "5");
    marker.setAttribute("orient", "auto");
    marker.setAttribute("markerUnits", "strokeWidth");
    marker.setAttribute("refX", "10");
    marker.setAttribute("refY", "5");
    marker.setAttribute("viewBox", "0 0 10 10");
    let path = this.chromeDoc.createElementNS(SVG_NS, "path");
    path.setAttribute("d", "M 0 0 L 10 5 L 0 10 z");
    path.setAttribute("fill", "#08C");
    marker.appendChild(path);
    this._svgRoot.appendChild(marker);

    // Create the 2 polygons (transformed and untransformed)
    let shapesGroup = this._createSVGNode("container", "g", this._svgRoot);
    this._shapes = {
      untransformed: this._createSVGNode("untransformed", "polygon", shapesGroup),
      transformed: this._createSVGNode("transformed", "polygon", shapesGroup)
    };

    // Create the arrows
    for (let nb of ["1", "2", "3", "4"]) {
      let line = this._createSVGNode("line", "line", shapesGroup);
      line.setAttribute("marker-end", "url(#" + this.markerId + ")");
      this._shapes["line" + nb] = line;
    }

    this._container.appendChild(this._svgRoot);

    // Insert the highlighter right after the browser
    stack.insertBefore(this._container, stack.childNodes[1]);
  },

  _createSVGNode: function(classPostfix, nodeType, parent) {
    let node = this.chromeDoc.createElementNS(SVG_NS, nodeType);
    node.setAttribute("class", "css-transform-" + classPostfix);

    parent.appendChild(node);
    return node;
  },

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function() {
    XULBasedHighlighter.prototype.destroy.call(this);

    this._container.remove();
    this._container = null;
  },

  /**
   * Show the highlighter on a given node
   * @param {DOMNode} node
   */
  _show: function() {
    if (!this._isTransformed(this.currentNode)) {
      this.hide();
      return;
    }

    this._update();
  },

  /**
   * Checks if the supplied node is transformed and not inline
   */
  _isTransformed: function(node) {
    let style = CssLogic.getComputedStyle(node);
    return style && (style.transform !== "none" && style.display !== "inline");
  },

  _setPolygonPoints: function(quad, poly) {
    let points = [];
    for (let point of ["p1","p2", "p3", "p4"]) {
      points.push(quad[point].x + "," + quad[point].y);
    }
    poly.setAttribute("points", points.join(" "));
  },

  _setLinePoints: function(p1, p2, line) {
    line.setAttribute("x1", p1.x);
    line.setAttribute("y1", p1.y);
    line.setAttribute("x2", p2.x);
    line.setAttribute("y2", p2.y);

    let dist = Math.sqrt(Math.pow(p2.x - p1.x, 2) + Math.pow(p2.y - p1.y, 2));
    if (dist < ARROW_LINE_MIN_DISTANCE) {
      line.removeAttribute("marker-end");
    } else {
      line.setAttribute("marker-end", "url(#" + this.markerId + ")");
    }
  },

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)).
   * Should be called whenever node size or attributes change
   */
  _update: function() {
    // Getting the points for the transformed shape
    let quad = this.layoutHelpers.getAdjustedQuads(this.currentNode, "border");
    if (!quad || quad.bounds.width <= 0 || quad.bounds.height <= 0) {
      this._hideShapes();
      return null;
    }

    // Getting the points for the untransformed shape
    let untransformedQuad = this.layoutHelpers.getNodeBounds(this.currentNode);

    this._setPolygonPoints(quad, this._shapes.transformed);
    this._setPolygonPoints(untransformedQuad, this._shapes.untransformed);
    for (let nb of ["1", "2", "3", "4"]) {
      this._setLinePoints(untransformedQuad["p" + nb], quad["p" + nb],
        this._shapes["line" + nb]);
    }

    this._showShapes();
  },

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide: function() {
    this._hideShapes();
  },

  _hideShapes: function() {
    this._svgRoot.setAttribute("hidden", "true");
  },

  _showShapes: function() {
    this._svgRoot.removeAttribute("hidden");
  }
});

/**
 * The SelectorHighlighter runs a given selector through querySelectorAll on the
 * document of the provided context node and then uses the BoxModelHighlighter
 * to highlight the matching nodes
 */
function SelectorHighlighter(tabActor) {
  this.tabActor = tabActor;
  this._highlighters = [];
}

SelectorHighlighter.prototype = {
  /**
   * Show BoxModelHighlighter on each node that matches that provided selector.
   * @param {DOMNode} node A context node that is used to get the document on
   * which querySelectorAll should be executed. This node will NOT be
   * highlighted.
   * @param {Object} options Should at least contain the 'selector' option, a
   * string that will be used in querySelectorAll. On top of this, all of the
   * valid options to BoxModelHighlighter.show are also valid here.
   */
  show: function(node, options={}) {
    this.hide();

    if (!isNodeValid(node) || !options.selector) {
      return;
    }

    let nodes = [];
    try {
      nodes = [...node.ownerDocument.querySelectorAll(options.selector)];
    } catch (e) {}

    delete options.selector;

    let i = 0;
    for (let matchingNode of nodes) {
      if (i >= MAX_HIGHLIGHTED_ELEMENTS) {
        break;
      }

      let highlighter = new BoxModelHighlighter(this.tabActor);
      if (options.fill) {
        highlighter.regionFill[options.region || "border"] = options.fill;
      }
      highlighter.show(matchingNode, options);
      this._highlighters.push(highlighter);
      i ++;
    }
  },

  hide: function() {
    for (let highlighter of this._highlighters) {
      highlighter.destroy();
    }
    this._highlighters = [];
  },

  destroy: function() {
    this.hide();
    this.tabActor = null;
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

/**
 * Can the host support the XUL-based highlighters which require a parent
 * XUL node to get attached.
 * @param {TabActor}
 * @return {Boolean}
 */
function supportXULBasedHighlighter(tabActor) {
  // Note that <browser>s on Fennec also have a XUL parentNode but the box
  // model highlighter doesn't display correctly on Fennec (bug 993190)
  return tabActor.browser &&
         !!tabActor.browser.parentNode &&
         Services.appinfo.ID !== "{aa3c5121-dab2-40e2-81ca-7ea25febc110}";
}

function isNodeValid(node) {
  // Is it null or dead?
  if(!node || Cu.isDeadWrapper(node)) {
    return false;
  }

  // Is it an element node
  if (node.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE) {
    return false;
  }

  // Is the document inaccessible?
  let doc = node.ownerDocument;
  if (!doc || !doc.defaultView) {
    return false;
  }

  // Is the node connected to the document? Using getBindingParent adds
  // support for anonymous elements generated by a node in the document.
  let bindingParent = LayoutHelpers.getRootBindingParent(node);
  if (!doc.documentElement.contains(bindingParent)) {
    return false;
  }

  return true;
}

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils)
});
