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

Cu.import("resource://gre/modules/devtools/LayoutHelpers.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

// FIXME: add ":visited" and ":link" after bug 713106 is fixed
const PSEUDO_CLASSES = [":hover", ":active", ":focus"];
const BOX_MODEL_REGIONS = ["margin", "border", "padding", "content"];
const BOX_MODEL_SIDES = ["top", "right", "bottom", "left"];
const SVG_NS = "http://www.w3.org/2000/svg";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const HIGHLIGHTER_STYLESHEET_URI = "resource://gre/modules/devtools/server/actors/highlighter.css";
const HIGHLIGHTER_PICKED_TIMER = 1000;
// How high is the nodeinfobar
const NODE_INFOBAR_HEIGHT = 40; //px
const NODE_INFOBAR_ARROW_SIZE = 15; // px
// Width of boxmodelhighlighter guides
const GUIDE_STROKE_WIDTH = 1;
// The minimum distance a line should be before it has an arrow marker-end
const ARROW_LINE_MIN_DISTANCE = 10;
// How many maximum nodes can be highlighted at the same time by the
// SelectorHighlighter
const MAX_HIGHLIGHTED_ELEMENTS = 100;
// SimpleOutlineHighlighter's stylesheet
const HIGHLIGHTED_PSEUDO_CLASS = ":-moz-devtools-highlighted";
const SIMPLE_OUTLINE_SHEET = ".__fx-devtools-hide-shortcut__ {" +
                             "  visibility: hidden !important" +
                             "}" +
                             HIGHLIGHTED_PSEUDO_CLASS + " {" +
                             "  outline: 2px dashed #F06!important;" +
                             "  outline-offset: -2px!important;" +
                             "}";

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
    this._onNavigate = this._onNavigate.bind(this);

    this._createHighlighter();

    // Listen to navigation events to switch from the BoxModelHighlighter to the
    // SimpleOutlineHighlighter, and back, if the top level window changes.
    events.on(this._tabActor, "navigate", this._onNavigate);
  },

  get conn() this._inspector && this._inspector.conn,

  _createHighlighter: function() {
    this._isPreviousWindowXUL = isXUL(this._tabActor);

    if (!this._isPreviousWindowXUL) {
      this._highlighter = new BoxModelHighlighter(this._tabActor,
                                                          this._inspector);
      this._highlighter.on("ready", this._highlighterReady);
      this._highlighter.on("hide", this._highlighterHidden);
    } else {
      this._highlighter = new SimpleOutlineHighlighter(this._tabActor);
    }
  },

  _destroyHighlighter: function() {
    if (this._highlighter) {
      if (!this._isPreviousWindowXUL) {
        this._highlighter.off("ready", this._highlighterReady);
        this._highlighter.off("hide", this._highlighterHidden);
      }
      this._highlighter.destroy();
      this._highlighter = null;
    }
  },

  _onNavigate: function({isTopLevel}) {
    if (!isTopLevel) {
      return;
    }

    // Only rebuild the highlighter if the window type changed.
    if (isXUL(this._tabActor) !== this._isPreviousWindowXUL) {
      this._destroyHighlighter();
      this._createHighlighter();
    }
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);

    this._destroyHighlighter();
    events.off(this._tabActor, "navigate", this._onNavigate);
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
      this._highlighter.show(node.rawNode, options);
    } else {
      this._highlighter.hide();
    }
  }, {
    request: {
      node: Arg(0, "domnode"),
      region: Option(1),
      hideInfoBar: Option(1),
      hideGuides: Option(1),
      showOnly: Option(1)
    }
  }),

  /**
   * Hide the box model highlighting if it was shown before
   */
  hideBoxModel: method(function() {
    this._highlighter.hide();
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
          this._highlighter.hide();
        }, HIGHLIGHTER_PICKED_TIMER);
      }
      events.emit(this._walker, "picker-node-picked", this._findAndAttachElement(event));
    };

    this._onHovered = event => {
      this._preventContentEvent(event);
      let res = this._findAndAttachElement(event);
      if (this._hoveredNode !== res.node) {
        this._highlighter.show(res.node.rawNode);
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
      this._highlighter.hide();
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

    // The assumption is that all custom highlighters need the canvasframe
    // container to append their elements, so if this is a XUL window, bail out.
    if (!isXUL(this._inspector.tabActor)) {
      this._highlighter = new constructor(inspector.tabActor);
    } else {
      throw new Error("Custom " + typeName +
        "highlighter cannot be created in a XUL window");
      return;
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
 * Every highlighters should insert their markup content into the document's
 * canvasFrame anonymous content container (see dom/webidl/Document.webidl).
 *
 * Since this container gets cleared when the document navigates, highlighters
 * should use this helper to have their markup content automatically re-inserted
 * in the new document.
 *
 * Since the markup content is inserted in the canvasFrame using
 * insertAnonymousContent, this means that it can be modified using the API
 * described in AnonymousContent.webidl.
 * To retrieve the AnonymousContent instance, use the content getter.
 *
 * @param {TabActor} tabActor
 *        The tabactor which windows will be used to insert the node
 * @param {Function} nodeBuilder
 *        A function that, when executed, returns a DOM node to be inserted into
 *        the canvasFrame
 */
function CanvasFrameAnonymousContentHelper(tabActor, nodeBuilder) {
  this.tabActor = tabActor;
  this.nodeBuilder = nodeBuilder;

  this._insert();

  this._onNavigate = this._onNavigate.bind(this);
  events.on(this.tabActor, "navigate", this._onNavigate);
}

CanvasFrameAnonymousContentHelper.prototype = {
  destroy: function() {
    // If the current window isn't the one the content was inserted into, this
    // will fail, but that's fine.
    try {
      let doc = this.tabActor.window.document;
      doc.removeAnonymousContent(this._content);
    } catch (e) {}
    events.off(this.tabActor, "navigate", this._onNavigate);
    this.tabActor = this.nodeBuilder = this._content = null;
  },

  _insert: function() {
    // Re-insert the content node after page navigation only if the new page
    // isn't XUL.
    if (!isXUL(this.tabActor)) {
      // For now highlighter.css is injected in content as a ua sheet because
      // <style scoped> doesn't work inside anonymous content (see bug 1086532).
      // If it did, highlighter.css would be injected as an anonymous content
      // node using CanvasFrameAnonymousContentHelper instead.
      installHelperSheet(this.tabActor.window,
        "@import url('" + HIGHLIGHTER_STYLESHEET_URI + "');");
      let node = this.nodeBuilder();
      let doc = this.tabActor.window.document;
      this._content = doc.insertAnonymousContent(node);
    }
  },

  _onNavigate: function({isTopLevel}) {
    if (isTopLevel) {
      this._insert();
    }
  },

  getTextContentForElement: function(id) {
    if (!this.content) {
      return null;
    }
    return this.content.getTextContentForElement(id);
  },

  setTextContentForElement: function(id, text) {
    if (this.content) {
      this.content.setTextContentForElement(id, text);
    }
  },

  setAttributeForElement: function(id, name, value) {
    if (this.content) {
      this.content.setAttributeForElement(id, name, value);
    }
  },

  getAttributeForElement: function(id, name) {
    if (!this.content) {
      return null;
    }
    return this.content.getAttributeForElement(id, name);
  },

  removeAttributeForElement: function(id, name) {
    if (this.content) {
      this.content.removeAttributeForElement(id, name);
    }
  },

  get content() {
    if (Cu.isDeadWrapper(this._content)) {
      return null;
    }
    return this._content;
  }
};

/**
 * Base class for auto-refresh-on-change highlighters. Sub classes will have a
 * chance to update whenever the current node's geometry changes.
 *
 * Sub classes must implement the following methods:
 * _show: called when the highlighter should be shown,
 * _hide: called when the highlighter should be hidden,
 * _update: called while the highlighter is shown and the geometry of the
 *          current node changes.
 *
 * Sub classes will have access to the following properties:
 * - this.currentNode: the node to be shown
 * - this.currentQuads: all of the node's box model region quads
 * - this.win: the current window
 */
function AutoRefreshHighlighter(tabActor) {
  EventEmitter.decorate(this);

  this.tabActor = tabActor;
  this.browser = tabActor.browser;
  this.win = tabActor.window;

  this.currentNode = null;
  this.currentQuads = {};

  this.layoutHelpers = new LayoutHelpers(this.win);

  this.update = this.update.bind(this);
}

AutoRefreshHighlighter.prototype = {
  /**
   * Show the highlighter on a given node
   * @param {DOMNode} node
   * @param {Object} options
   *        Object used for passing options
   */
  show: function(node, options={}) {
    let isSameNode = node === this.currentNode;
    let isSameOptions = this._isSameOptions(options);

    if (!isNodeValid(node) || (isSameNode && isSameOptions)) {
      return;
    }

    this.options = options;

    this._stopRefreshLoop();
    this.currentNode = node;
    this._updateAdjustedQuads();
    this._startRefreshLoop();
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
    this._stopRefreshLoop();
    this.currentNode = null;
    this.currentQuads = {};
    this.options = null;
  },

  /**
   * Are the provided options the same as the currently stored options?
   * Returns false if there are no options stored currently.
   */
  _isSameOptions: function(options) {
    if (!this.options) {
      return false;
    }

    let keys = Object.keys(options);

    if (keys.length !== Object.keys(this.options).length) {
      return false;
    }

    for (let key of keys) {
      if (this.options[key] !== options[key]) {
        return false;
      }
    }

    return true;
  },

  /**
   * Update the stored box quads by reading the current node's box quads.
   */
  _updateAdjustedQuads: function() {
    for (let region of BOX_MODEL_REGIONS) {
      this.currentQuads[region] = this.layoutHelpers.getAdjustedQuads(
        this.currentNode, region);
    }
  },

  /**
   * Update the knowledge we have of the current node's boxquads and return true
   * if any of the points x/y or bounds have change since.
   * @return {Boolean}
   */
  _hasMoved: function() {
    let oldQuads = JSON.stringify(this.currentQuads);
    this._updateAdjustedQuads();
    let newQuads = JSON.stringify(this.currentQuads);
    return oldQuads !== newQuads;
  },

  /**
   * Update the highlighter if the node has moved since the last update.
   */
  update: function(e) {
    if (!isNodeValid(this.currentNode) || !this._hasMoved()) {
      return;
    }

    this._update();
    this.emit("updated");
  },

  _show: function() {
    // To be implemented by sub classes
    // When called, sub classes should actually show the highlighter for
    // this.currentNode, potentially using options in this.options
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

  _startRefreshLoop: function() {
    let win = this.currentNode.ownerDocument.defaultView;
    this.rafID = win.requestAnimationFrame(this._startRefreshLoop.bind(this));
    this.update();
  },

  _stopRefreshLoop: function() {
    if (!this.rafID) {
      return;
    }
    let win = this.currentNode.ownerDocument.defaultView;
    win.cancelAnimationFrame(this.rafID);
    this.rafID = null;
  },

  destroy: function() {
    this.hide();

    this.tabActor = null;
    this.win = null;
    this.browser = null;
    this.currentNode = null;
    this.layoutHelpers = null;
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
 * <div class="highlighter-container">
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
 *   <div class="highlighter-nodeinfobar-container">
 *     <div class="highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-top" />
 *     <div class="highlighter-nodeinfobar">
 *       <div class="highlighter-nodeinfobar-text" align="center" flex="1">
 *         <span class="highlighter-nodeinfobar-tagname">Node name</span>
 *         <span class="highlighter-nodeinfobar-id">Node id</span>
 *         <span class="highlighter-nodeinfobar-classes">.someClass</span>
 *         <span class="highlighter-nodeinfobar-pseudo-classes">:hover</span>
 *       </div>
 *     </div>
 *     <div class="highlighter-nodeinfobar-arrow highlighter-nodeinfobar-arrow-bottom"/>
 *   </div>
 * </div>
 */
function BoxModelHighlighter(tabActor) {
  AutoRefreshHighlighter.call(this, tabActor);
  EventEmitter.decorate(this);

  this.markup = new CanvasFrameAnonymousContentHelper(this.tabActor,
    this._buildMarkup.bind(this));

  /**
   * Optionally customize each region's fill color by adding an entry to the
   * regionFill property: `highlighter.regionFill.margin = "red";
   */
  this.regionFill = {};

  this._currentNode = null;
}

BoxModelHighlighter.prototype = Heritage.extend(AutoRefreshHighlighter.prototype, {
  ID_CLASS_PREFIX: "box-model-",

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

  _buildMarkup: function() {
    let doc = this.win.document;

    let highlighterContainer = doc.createElement("div");
    highlighterContainer.className = "highlighter-container";

    // Building the SVG element with its polygons and lines

    let svgRoot = this._createSVGNode("svg", highlighterContainer, {
      "id": "root",
      "class": "root",
      "width": "100%",
      "height": "100%",
      "style": "width:100%;height:100%;",
      "hidden": "true"
    });

    let boxModelContainer = this._createSVGNode("g", svgRoot, {
      "class": "container"
    });

    for (let region of BOX_MODEL_REGIONS) {
      this._createSVGNode("polygon", boxModelContainer, {
        "class": region,
        "id": region
      });
    }

    for (let side of BOX_MODEL_SIDES) {
      this._createSVGNode("line", svgRoot, {
        "class": "guide-" + side,
        "id": "guide-" + side,
        "stroke-width": GUIDE_STROKE_WIDTH
      });
    }

    highlighterContainer.appendChild(svgRoot);

    // Building the nodeinfo bar markup

    let infobarContainer = this._createNode("div", highlighterContainer, {
      "class": "nodeinfobar-container",
      "id": "nodeinfobar-container",
      "position": "top",
      "hidden": "true"
    });

    let nodeInfobar = this._createNode("div", infobarContainer, {
      "class": "nodeinfobar"
    });

    let texthbox = this._createNode("div", nodeInfobar, {
      "class": "nodeinfobar-text"
    });
    this._createNode("span", texthbox, {
      "class": "nodeinfobar-tagname",
      "id": "nodeinfobar-tagname"
    });
    this._createNode("span", texthbox, {
      "class": "nodeinfobar-id",
      "id": "nodeinfobar-id"
    });
    this._createNode("span", texthbox, {
      "class": "nodeinfobar-classes",
      "id": "nodeinfobar-classes"
    });
    this._createNode("span", texthbox, {
      "class": "nodeinfobar-pseudo-classes",
      "id": "nodeinfobar-pseudo-classes"
    });
    this._createNode("span", texthbox, {
      "class": "nodeinfobar-dimensions",
      "id": "nodeinfobar-dimensions"
    });

    return highlighterContainer;
  },

  _createSVGNode: function(nodeType, parent, attributes={}) {
    return this._createNode(nodeType, parent, attributes, SVG_NS);
  },

  _createNode: function(nodeType, parent, attributes={}, namespace=null) {
    let node;
    if (namespace) {
      node = this.win.document.createElementNS(namespace, nodeType);
    } else {
      node = this.win.document.createElement(nodeType);
    }

    for (let name in attributes) {
      let value = attributes[name];
      if (name === "class" || name === "id") {
        value = this.ID_CLASS_PREFIX + value
      }
      node.setAttribute(name, value);
    }

    parent.appendChild(node);
    return node;
  },

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function() {
    AutoRefreshHighlighter.prototype.destroy.call(this);

    this.markup.destroy();

    this._currentNode = null;
  },

  /**
   * Show the highlighter on a given node
   */
  _show: function() {
    if (BOX_MODEL_REGIONS.indexOf(this.options.region) == -1)  {
      this.options.region = "content";
    }

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
      } else {
        this._hideInfobar();
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
    this.markup.setAttributeForElement(
      this.ID_CLASS_PREFIX + "nodeinfobar-container", "hidden", "true");
  },

  /**
   * Show the infobar
   */
  _showInfobar: function() {
    this.markup.removeAttributeForElement(
      this.ID_CLASS_PREFIX + "nodeinfobar-container", "hidden");
    this._updateInfobar();
  },

  /**
   * Hide the box model
   */
  _hideBoxModel: function() {
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + "root", "hidden",
      "true");
  },

  /**
   * Show the box model
   */
  _showBoxModel: function() {
    this.markup.removeAttributeForElement(this.ID_CLASS_PREFIX + "root",
      "hidden");
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
      for (let boxType of BOX_MODEL_REGIONS) {
        let {p1, p2, p3, p4} = this.currentQuads[boxType];

        if (this.regionFill[boxType]) {
          this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + boxType,
            "style", "fill:" + this.regionFill[boxType]);
        } else {
          this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + boxType,
            "style", "");
        }

        if (!this.options.showOnly || this.options.showOnly === boxType) {
          this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + boxType,
            "points", p1.x + "," + p1.y + " " +
                      p2.x + "," + p2.y + " " +
                      p3.x + "," + p3.y + " " +
                      p4.x + "," + p4.y);
        } else {
          this.markup.removeAttributeForElement(this.ID_CLASS_PREFIX + boxType, "points");
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
    let hasNoQuads = !this.currentQuads.margin &&
                     !this.currentQuads.border &&
                     !this.currentQuads.padding &&
                     !this.currentQuads.content;
    if (!this.currentNode ||
        Cu.isDeadWrapper(this.currentNode) ||
        this.currentNode.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE ||
        !this.currentNode.ownerDocument ||
        !this.currentNode.ownerDocument.defaultView ||
        hasNoQuads) {
      return false;
    }

    if (!this._computedStyle) {
      this._computedStyle = CssLogic.getComputedStyle(this.currentNode);
    }

    return this._computedStyle.getPropertyValue("display") !== "none";
  },

  _getOuterBounds: function() {
    for (let region of ["margin", "border", "padding", "content"]) {
      let quads = this.currentQuads[region];

      if (!quads) {
        // Invisible element such as a script tag.
        break;
      }

      let {bottom, height, left, right, top, width, x, y} = quads.bounds;

      if (width > 0 || height > 0) {
        return {bottom, height, left, right, top, width, x, y};
      }
    }

    return {
      bottom: 0,
      height: 0,
      left: 0,
      right: 0,
      top: 0,
      width: 0,
      x: 0,
      y: 0
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
    this._updateGuide("top", toShowY[0]);
    this._updateGuide("right", toShowX[1]);
    this._updateGuide("bottom", toShowY[1]);
    this._updateGuide("left", toShowX[0]);
  },

  _hideGuides: function() {
    for (let side of BOX_MODEL_SIDES) {
      this.markup.setAttributeForElement(
        this.ID_CLASS_PREFIX + "guide-" + side, "hidden", "true");
    }
  },

  /**
   * Move a guide to the appropriate position and display it. If no point is
   * passed then the guide is hidden.
   *
   * @param  {String} side
   *         The guide to update
   * @param  {Integer} point
   *         x or y co-ordinate. If this is undefined we hide the guide.
   */
  _updateGuide: function(side, point=-1) {
    let guideId = this.ID_CLASS_PREFIX + "guide-" + side;

    if (point <= 0) {
      this.markup.setAttributeForElement(guideId, "hidden", "true");
      return false;
    }

    let offset = GUIDE_STROKE_WIDTH / 2;

    if (side === "top" || side === "left") {
      point -= offset;
    } else {
      point += offset;
    }

    if (side === "top" || side === "bottom") {
      this.markup.setAttributeForElement(guideId, "x1", "0");
      this.markup.setAttributeForElement(guideId, "y1", point + "");
      this.markup.setAttributeForElement(guideId, "x2", "100%");
      this.markup.setAttributeForElement(guideId, "y2", point + "");
    } else {
      this.markup.setAttributeForElement(guideId, "x1", point + "");
      this.markup.setAttributeForElement(guideId, "y1", "0");
      this.markup.setAttributeForElement(guideId, "x2", point + "");
      this.markup.setAttributeForElement(guideId, "y2", "100%");
    }

    this.markup.removeAttributeForElement(guideId, "hidden");

    return true;
  },

  /**
   * Update node information (tagName#id.class)
   */
  _updateInfobar: function() {
    if (!this.currentNode) {
      return;
    }

    let {bindingElement:node, pseudo} =
      CssLogic.getBindingElementAndPseudo(this.currentNode);

    // Update the tag, id, classes, pseudo-classes and dimensions
    let tagName = node.tagName;

    let id = node.id ? "#" + node.id : "";

    let classList = (node.classList || []).length ? "." + [...node.classList].join(".") : "";

    let pseudos = PSEUDO_CLASSES.filter(pseudo => {
      return DOMUtils.hasPseudoClassLock(node, pseudo);
    }, this).join("");
    if (pseudo) {
      // Display :after as ::after
      pseudos += ":" + pseudo;
    }

    let rect = node.getBoundingClientRect();
    let dim = Math.ceil(rect.width) + " x " + Math.ceil(rect.height);

    let elementId = this.ID_CLASS_PREFIX + "nodeinfobar-";
    this.markup.setTextContentForElement(elementId + "tagname", tagName);
    this.markup.setTextContentForElement(elementId + "id", id);
    this.markup.setTextContentForElement(elementId + "classes", classList);
    this.markup.setTextContentForElement(elementId + "pseudo-classes", pseudos);
    this.markup.setTextContentForElement(elementId + "dimensions", dim);

    this._moveInfobar();
  },

  /**
   * Move the Infobar to the right place in the highlighter.
   */
  _moveInfobar: function() {
    let bounds = this._getOuterBounds();
    let winHeight = this.win.innerHeight * this.zoom;
    let winWidth = this.win.innerWidth * this.zoom;

    // Ensure that containerBottom and containerTop are at least zero to avoid
    // showing tooltips outside the viewport.
    let containerBottom = Math.max(0, bounds.bottom) + NODE_INFOBAR_ARROW_SIZE;
    let containerTop = Math.min(winHeight, bounds.top);
    let containerId = this.ID_CLASS_PREFIX + "nodeinfobar-container";

    // Can the bar be above the node?
    let top;
    if (containerTop < NODE_INFOBAR_HEIGHT) {
      // No. Can we move the bar under the node?
      if (containerBottom + NODE_INFOBAR_HEIGHT > winHeight) {
        // No. Let's move it inside.
        top = containerTop;
        this.markup.setAttributeForElement(containerId, "position", "overlap");
      } else {
        // Yes. Let's move it under the node.
        top = containerBottom;
        this.markup.setAttributeForElement(containerId, "position", "bottom");
      }
    } else {
      // Yes. Let's move it on top of the node.
      top = containerTop - NODE_INFOBAR_HEIGHT;
      this.markup.setAttributeForElement(containerId, "position", "top");
    }

    // Align the bar with the box's center if possible.
    let left = bounds.right - bounds.width / 2;
    // Make sure the while infobar is visible.
    let buffer = 100;
    if (left < buffer) {
      left = buffer;
      this.markup.setAttributeForElement(containerId, "hide-arrow", "true");
    } else if (left > winWidth - buffer) {
      left = winWidth - buffer;
      this.markup.setAttributeForElement(containerId, "hide-arrow", "true");
    } else {
      this.markup.removeAttributeForElement(containerId, "hide-arrow");
    }

    let style = "top:" + top + "px;left:" + left + "px;";
    this.markup.setAttributeForElement(containerId, "style", style);
  }
});

/**
 * The CssTransformHighlighter is the class that draws an outline around a
 * transformed element and an outline around where it would be if untransformed
 * as well as arrows connecting the 2 outlines' corners.
 */
function CssTransformHighlighter(tabActor) {
  AutoRefreshHighlighter.call(this, tabActor);

  this.markup = new CanvasFrameAnonymousContentHelper(this.tabActor,
    this._buildMarkup.bind(this));
}

let MARKER_COUNTER = 1;

CssTransformHighlighter.prototype = Heritage.extend(AutoRefreshHighlighter.prototype, {
  ID_CLASS_PREFIX: "css-transform-",

  _buildMarkup: function() {
    let doc = this.win.document;

    let container = doc.createElement("div");
    container.className = "highlighter-container";

    let svgRoot = this._createSVGNode("svg", container, {
      "class": "root",
      "id": "root",
      "hidden": "true",
      "width": "100%",
      "height": "100%"
    });

    // Add a marker tag to the svg root for the arrow tip
    this.markerId = "arrow-marker-" + MARKER_COUNTER;
    MARKER_COUNTER ++;
    let marker = this._createSVGNode("marker", svgRoot, {
      "id": this.markerId,
      "markerWidth": "10",
      "markerHeight": "5",
      "orient": "auto",
      "markerUnits": "strokeWidth",
      "refX": "10",
      "refY": "5",
      "viewBox": "0 0 10 10",
    });
    this._createSVGNode("path", marker, {
      "d": "M 0 0 L 10 5 L 0 10 z",
      "fill": "#08C"
    });

    let shapesGroup = this._createSVGNode("g", svgRoot);

    // Create the 2 polygons (transformed and untransformed)
    this._createSVGNode("polygon", shapesGroup, {
      "id": "untransformed",
      "class": "untransformed"
    });
    this._createSVGNode("polygon", shapesGroup, {
      "id": "transformed",
      "class": "transformed"
    });

    // Create the arrows
    for (let nb of ["1", "2", "3", "4"]) {
      this._createSVGNode("line", shapesGroup, {
        "id": "line" + nb,
        "class": "line",
        "marker-end": "url(#" + this.markerId + ")"
      });
    }

    container.appendChild(svgRoot);

    return container;
  },

  _createSVGNode: function(nodeType, parent, attributes={}) {
    let node = this.win.document.createElementNS(SVG_NS, nodeType);

    for (let name in attributes) {
      let value = attributes[name];
      if (name === "class" || name === "id") {
        value = this.ID_CLASS_PREFIX + value
      }
      node.setAttribute(name, value);
    }

    parent.appendChild(node);
    return node;
  },

  /**
   * Destroy the nodes. Remove listeners.
   */
  destroy: function() {
    AutoRefreshHighlighter.prototype.destroy.call(this);
    this.markup.destroy();
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

  _setPolygonPoints: function(quad, id) {
    let points = [];
    for (let point of ["p1","p2", "p3", "p4"]) {
      points.push(quad[point].x + "," + quad[point].y);
    }
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + id,
                                       "points",
                                       points.join(" "));
  },

  _setLinePoints: function(p1, p2, id) {
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + id, "x1", p1.x);
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + id, "y1", p1.y);
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + id, "x2", p2.x);
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + id, "y2", p2.y);

    let dist = Math.sqrt(Math.pow(p2.x - p1.x, 2) + Math.pow(p2.y - p1.y, 2));
    if (dist < ARROW_LINE_MIN_DISTANCE) {
      this.markup.removeAttributeForElement(this.ID_CLASS_PREFIX + id, "marker-end");
    } else {
      this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + id, "marker-end",
                                         "url(#" + this.markerId + ")");
    }
  },

  /**
   * Update the highlighter on the current highlighted node (the one that was
   * passed as an argument to show(node)).
   * Should be called whenever node size or attributes change
   */
  _update: function() {
    // Getting the points for the transformed shape
    let quad = this.currentQuads.border;
    if (!quad || quad.bounds.width <= 0 || quad.bounds.height <= 0) {
      this._hideShapes();
      return null;
    }

    // Getting the points for the untransformed shape
    let untransformedQuad = this.layoutHelpers.getNodeBounds(this.currentNode);

    this._setPolygonPoints(quad, "transformed");
    this._setPolygonPoints(untransformedQuad, "untransformed");
    for (let nb of ["1", "2", "3", "4"]) {
      this._setLinePoints(untransformedQuad["p" + nb], quad["p" + nb], "line" + nb);
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
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + "root", "hidden", "true");
  },

  _showShapes: function() {
    this.markup.removeAttributeForElement(this.ID_CLASS_PREFIX + "root", "hidden");
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
 * to draw a simple css outline around the element.
 * It is used by the HighlighterActor when canvasframe-based highlighters can't
 * be used. This is the case for XUL windows.
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
    this.chromeDoc = null;
  },

  /**
   * Show the highlighter on a given node
   * @param {DOMNode} node
   */
  show: function(node) {
    if (!this.currentNode || node !== this.currentNode) {
      this.hide();
      this.currentNode = node;
      installHelperSheet(node.ownerDocument.defaultView, SIMPLE_OUTLINE_SHEET);
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

/**
 * Inject a helper stylesheet in the window.
 */
let installedHelperSheets = new WeakMap;
function installHelperSheet(win, source, type="agent") {
  if (installedHelperSheets.has(win.document)) {
    return;
  }
  let {Style} = require("sdk/stylesheet/style");
  let {attach} = require("sdk/content/mod");
  let style = Style({source, type});
  attach(style, win);
  installedHelperSheets.set(win.document, style);
}

/**
 * Is the content window in this tabActor a XUL window
 */
function isXUL(tabActor) {
  return tabActor.window.document.documentElement.namespaceURI === XUL_NS;
}

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils)
});
