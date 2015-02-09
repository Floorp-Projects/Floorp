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
const {setIgnoreLayoutChanges} = require("devtools/server/actors/layout");

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
const NODE_INFOBAR_HEIGHT = 34; //px
const NODE_INFOBAR_ARROW_SIZE = 9; // px
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

/**
 * The registration mechanism for highlighters provide a quick way to
 * have modular highlighters, instead of a hard coded list.
 * It allow us to split highlighers in sub modules, and add them dynamically
 * using add-on (useful for 3rd party developers, or prototyping)
 *
 * Note that currently, highlighters added using add-ons, can only work on
 * Firefox desktop, or Fennec if the same add-on is installed in both.
 */
const highlighterTypes = new Map();

/**
 * Returns `true` if a highlighter for the given `typeName` is registered,
 * `false` otherwise.
 */
const isTypeRegistered = (typeName) => highlighterTypes.has(typeName);
exports.isTypeRegistered = isTypeRegistered;

/**
 * Registers a given constructor as highlighter, for the `typeName` given.
 * If no `typeName` is provided, is looking for a `typeName` property in
 * the prototype's constructor.
 */
const register = (constructor, typeName=constructor.prototype.typeName) => {
  if (!typeName) {
    throw Error("No type's name found, or provided.")
  }

  if (highlighterTypes.has(typeName)) {
    throw Error(`${typeName} is already registered.`)
  }

  highlighterTypes.set(typeName, constructor);
};
exports.register = register;

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

    let constructor = highlighterTypes.get(typeName);
    if (!constructor) {
      let list = [...highlighterTypes.keys()];

      throw new Error(`${typeName} isn't a valid highlighter class (${list})`);
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
   * Show the highlighter.
   * This calls through to the highlighter instance's |show(node, options)|
   * method.
   *
   * Most custom highlighters are made to highlight DOM nodes, hence the first
   * NodeActor argument (NodeActor as in toolkit/devtools/server/actor/inspector).
   * Note however that some highlighters use this argument merely as a context
   * node: the RectHighlighter for instance uses it to calculate the absolute
   * position of the provided rect. The SelectHighlighter uses it as a base node
   * to run the provided CSS selector on.
   *
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
  this.anonymousContentDocument = this.tabActor.window.document;
  // XXX the next line is a wallpaper for bug 1123362.
  this.anonymousContentGlobal = Cu.getGlobalForObject(this.anonymousContentDocument);

  this._insert();

  this._onNavigate = this._onNavigate.bind(this);
  events.on(this.tabActor, "navigate", this._onNavigate);
}

CanvasFrameAnonymousContentHelper.prototype = {
  destroy: function() {
    // If the current window isn't the one the content was inserted into, this
    // will fail, but that's fine.
    try {
      let doc = this.anonymousContentDocument;
      doc.removeAnonymousContent(this._content);
    } catch (e) {console.error(e)}
    events.off(this.tabActor, "navigate", this._onNavigate);
    this.tabActor = this.nodeBuilder = this._content = null;
    this.anonymousContentDocument = null;
    this.anonymousContentGlobal = null;
  },

  _insert: function() {
    // Re-insert the content node after page navigation only if the new page
    // isn't XUL.
    if (isXUL(this.tabActor)) {
      return;
    }
    let doc = this.tabActor.window.document;

    // On B2G, for example, when connecting to keyboard just after startup,
    // we connect to a hidden document, which doesn't accept
    // insertAnonymousContent call yet.
    if (doc.hidden) {
      // In such scenario, just wait for the document to be visible
      // before injecting anonymous content.
      let onVisibilityChange = () => {
        doc.removeEventListener("visibilitychange", onVisibilityChange);
        this._insert();
      };
      doc.addEventListener("visibilitychange", onVisibilityChange);
      return;
    }

    // For now highlighter.css is injected in content as a ua sheet because
    // <style scoped> doesn't work inside anonymous content (see bug 1086532).
    // If it did, highlighter.css would be injected as an anonymous content
    // node using CanvasFrameAnonymousContentHelper instead.
    installHelperSheet(this.tabActor.window,
      "@import url('" + HIGHLIGHTER_STYLESHEET_URI + "');");
    let node = this.nodeBuilder();
    this._content = doc.insertAnonymousContent(node);
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
    if (!this._content || Cu.isDeadWrapper(this._content)) {
      return null;
    }
    return this._content;
  },

  /**
   * The canvasFrame anonymous content container gets zoomed in/out with the
   * page. If this is unwanted, i.e. if you want the inserted element to remain
   * unzoomed, then this method can be used.
   *
   * Consumers of the CanvasFrameAnonymousContentHelper should call this method,
   * it isn't executed automatically. Typically, AutoRefreshHighlighter can call
   * it when _update is executed.
   *
   * The matching element will be scaled down or up by 1/zoomLevel (using css
   * transform) to cancel the current zoom. The element's width and height
   * styles will also be set according to the scale. Finally, the element's
   * position will be set as absolute.
   *
   * Note that if the matching element already has an inline style attribute, it
   * *won't* be preserved.
   *
   * @param {DOMNode} node This node is used to determine which container window
   * should be used to read the current zoom value.
   * @param {String} id The ID of the root element inserted with this API.
   */
  scaleRootElement: function(node, id) {
    let zoom = LayoutHelpers.getCurrentZoom(node);
    let value = "position:absolute;width:100%;height:100%;";

    if (zoom !== 1) {
      value = "position:absolute;";
      value += "transform-origin:top left;transform:scale(" + (1/zoom) + ");";
      value += "width:" + (100*zoom) + "%;height:" + (100*zoom) + "%;";
    }

    this.setAttributeForElement(id, "style", value);
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
 *
 * Emits the following events:
 * - shown
 * - hidden
 * - updated
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

    this.emit("shown");
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

    this.emit("hidden");
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
    this.rafWin = win;
    this.update();
  },

  _stopRefreshLoop: function() {
    if (this.rafID && !Cu.isDeadWrapper(this.rafWin)) {
      this.rafWin.cancelAnimationFrame(this.rafID);
    }
    this.rafID = this.rafWin = null;
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
 *   <div class="box-model-root">
 *     <svg class="box-model-elements" hidden="true">
 *       <g class="box-model-regions">
 *         <polygon class="box-model-margin" points="..." />
 *         <polygon class="box-model-border" points="..." />
 *         <polygon class="box-model-padding" points="..." />
 *         <polygon class="box-model-content" points="..." />
 *       </g>
 *       <line class="box-model-guide-top" x1="..." y1="..." x2="..." y2="..." />
 *       <line class="box-model-guide-right" x1="..." y1="..." x2="..." y2="..." />
 *       <line class="box-model-guide-bottom" x1="..." y1="..." x2="..." y2="..." />
 *       <line class="box-model-guide-left" x1="..." y1="..." x2="..." y2="..." />
 *     </svg>
 *     <div class="box-model-nodeinfobar-container">
 *       <div class="box-model-nodeinfobar-arrow highlighter-nodeinfobar-arrow-top" />
 *       <div class="box-model-nodeinfobar">
 *         <div class="box-model-nodeinfobar-text" align="center">
 *           <span class="box-model-nodeinfobar-tagname">Node name</span>
 *           <span class="box-model-nodeinfobar-id">Node id</span>
 *           <span class="box-model-nodeinfobar-classes">.someClass</span>
 *           <span class="box-model-nodeinfobar-pseudo-classes">:hover</span>
 *         </div>
 *       </div>
 *       <div class="box-model-nodeinfobar-arrow box-model-nodeinfobar-arrow-bottom"/>
 *     </div>
 *   </div>
 * </div>
 */
function BoxModelHighlighter(tabActor) {
  AutoRefreshHighlighter.call(this, tabActor);

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
  typeName: "BoxModelHighlighter",

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

    // Build the root wrapper, used to adapt to the page zoom.
    let rootWrapper = createNode(this.win, {
      parent: highlighterContainer,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Building the SVG element with its polygons and lines

    let svg = createSVGNode(this.win, {
      nodeType: "svg",
      parent: rootWrapper,
      attributes: {
        "id": "elements",
        "width": "100%",
        "height": "100%",
        "style": "width:100%;height:100%;",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let regions = createSVGNode(this.win, {
      nodeType: "g",
      parent: svg,
      attributes: {
        "class": "regions"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    for (let region of BOX_MODEL_REGIONS) {
      createSVGNode(this.win, {
        nodeType: "polygon",
        parent: regions,
        attributes: {
          "class": region,
          "id": region
        },
        prefix: this.ID_CLASS_PREFIX
      });
    }

    for (let side of BOX_MODEL_SIDES) {
      createSVGNode(this.win, {
        nodeType: "line",
        parent: svg,
        attributes: {
          "class": "guide-" + side,
          "id": "guide-" + side,
          "stroke-width": GUIDE_STROKE_WIDTH
        },
        prefix: this.ID_CLASS_PREFIX
      });
    }

    // Building the nodeinfo bar markup

    let infobarContainer = createNode(this.win, {
      parent: rootWrapper,
      attributes: {
        "class": "nodeinfobar-container",
        "id": "nodeinfobar-container",
        "position": "top",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let nodeInfobar = createNode(this.win, {
      parent: infobarContainer,
      attributes: {
        "class": "nodeinfobar"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let texthbox = createNode(this.win, {
      parent: nodeInfobar,
      attributes: {
        "class": "nodeinfobar-text"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: texthbox,
      attributes: {
        "class": "nodeinfobar-tagname",
        "id": "nodeinfobar-tagname"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: texthbox,
      attributes: {
        "class": "nodeinfobar-id",
        "id": "nodeinfobar-id"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: texthbox,
      attributes: {
        "class": "nodeinfobar-classes",
        "id": "nodeinfobar-classes"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: texthbox,
      attributes: {
        "class": "nodeinfobar-pseudo-classes",
        "id": "nodeinfobar-pseudo-classes"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createNode(this.win, {
      nodeType: "span",
      parent: texthbox,
      attributes: {
        "class": "nodeinfobar-dimensions",
        "id": "nodeinfobar-dimensions"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return highlighterContainer;
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
    setIgnoreLayoutChanges(true);

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

    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
  },

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide: function() {
    setIgnoreLayoutChanges(true);

    this._untrackMutations();
    this._hideBoxModel();
    this._hideInfobar();

    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
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
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + "elements",
      "hidden", "true");
  },

  /**
   * Show the box model
   */
  _showBoxModel: function() {
    this.markup.removeAttributeForElement(this.ID_CLASS_PREFIX + "elements",
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

      // Un-zoom the root wrapper if the page was zoomed.
      let rootId = this.ID_CLASS_PREFIX + "root";
      this.markup.scaleRootElement(this.currentNode, rootId);

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

    let rect = this.currentQuads.border.bounds;
    let dim = Math.ceil(rect.width) + " \u00D7 " + Math.ceil(rect.height);

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
register(BoxModelHighlighter);
exports.BoxModelHighlighter = BoxModelHighlighter;

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
  typeName: "CssTransformHighlighter",

  ID_CLASS_PREFIX: "css-transform-",

  _buildMarkup: function() {
    let doc = this.win.document;

    let container = createNode(this.win, {
      attributes: {
        "class": "highlighter-container"
      }
    });

    // The root wrapper is used to unzoom the highlighter when needed.
    let rootWrapper = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let svg = createSVGNode(this.win, {
      nodeType: "svg",
      parent: rootWrapper,
      attributes: {
        "id": "elements",
        "hidden": "true",
        "width": "100%",
        "height": "100%"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Add a marker tag to the svg root for the arrow tip
    this.markerId = "arrow-marker-" + MARKER_COUNTER;
    MARKER_COUNTER ++;
    let marker = createSVGNode(this.win, {
      nodeType: "marker",
      parent: svg,
      attributes: {
        "id": this.markerId,
        "markerWidth": "10",
        "markerHeight": "5",
        "orient": "auto",
        "markerUnits": "strokeWidth",
        "refX": "10",
        "refY": "5",
        "viewBox": "0 0 10 10"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createSVGNode(this.win, {
      nodeType: "path",
      parent: marker,
      attributes: {
        "d": "M 0 0 L 10 5 L 0 10 z",
        "fill": "#08C"
      }
    });

    let shapesGroup = createSVGNode(this.win, {
      nodeType: "g",
      parent: svg
    });

    // Create the 2 polygons (transformed and untransformed)
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: shapesGroup,
      attributes: {
        "id": "untransformed",
        "class": "untransformed"
      },
      prefix: this.ID_CLASS_PREFIX
    });
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: shapesGroup,
      attributes: {
        "id": "transformed",
        "class": "transformed"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Create the arrows
    for (let nb of ["1", "2", "3", "4"]) {
      createSVGNode(this.win, {
        nodeType: "line",
        parent: shapesGroup,
        attributes: {
          "id": "line" + nb,
          "class": "line",
          "marker-end": "url(#" + this.markerId + ")"
        },
        prefix: this.ID_CLASS_PREFIX
      });
    }

    return container;
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
    setIgnoreLayoutChanges(true);

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

    // Adapt to the current zoom
    this.markup.scaleRootElement(this.currentNode, this.ID_CLASS_PREFIX + "root");

    this._showShapes();

    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
  },

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  _hide: function() {
    setIgnoreLayoutChanges(true);
    this._hideShapes();
    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
  },

  _hideShapes: function() {
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + "elements",
      "hidden", "true");
  },

  _showShapes: function() {
    this.markup.removeAttributeForElement(this.ID_CLASS_PREFIX + "elements",
      "hidden");
  }
});
register(CssTransformHighlighter);
exports.CssTransformHighlighter = CssTransformHighlighter;


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
  typeName: "SelectorHighlighter",
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
register(SelectorHighlighter);
exports.SelectorHighlighter = SelectorHighlighter;

/**
 * The RectHighlighter is a class that draws a rectangle highlighter at specific
 * coordinates.
 * It does *not* highlight DOM nodes, but rects.
 * It also does *not* update dynamically, it only highlights a rect and remains
 * there as long as it is shown.
 */
function RectHighlighter(tabActor) {
  this.win = tabActor.window;
  this.layoutHelpers = new LayoutHelpers(this.win);
  this.markup = new CanvasFrameAnonymousContentHelper(tabActor,
    this._buildMarkup.bind(this));
}

RectHighlighter.prototype = {
  typeName: "RectHighlighter",

  _buildMarkup: function() {
    let doc = this.win.document;

    let container = doc.createElement("div");
    container.className = "highlighter-container";
    container.innerHTML = '<div id="highlighted-rect" ' +
                          'class="highlighted-rect" hidden="true">';

    return container;
  },

  destroy: function() {
    this.win = null;
    this.layoutHelpers = null;
    this.markup.destroy();
  },

  _hasValidOptions: function(options) {
    let isValidNb = n => typeof n === "number" && n >= 0 && isFinite(n);
    return options && options.rect &&
           isValidNb(options.rect.x) &&
           isValidNb(options.rect.y) &&
           options.rect.width && isValidNb(options.rect.width) &&
           options.rect.height && isValidNb(options.rect.height);
  },

  /**
   * @param {DOMNode} node The highlighter rect is relatively positioned to the
   * viewport this node is in. Using the provided node, the highligther will get
   * the parent documentElement and use it as context to position the
   * highlighter correctly.
   * @param {Object} options Accepts the following options:
   * - rect: mandatory object that should have the x, y, width, height properties
   * - fill: optional fill color for the rect
   */
  show: function(node, options) {
    if (!this._hasValidOptions(options) || !node || !node.ownerDocument) {
      this.hide();
      return;
    }

    let contextNode = node.ownerDocument.documentElement;

    // Caculate the absolute rect based on the context node's adjusted quads.
    let {bounds} = this.layoutHelpers.getAdjustedQuads(contextNode);
    let x = "left:" + (bounds.x + options.rect.x) + "px;";
    let y = "top:" + (bounds.y + options.rect.y) + "px;";
    let width = "width:" + options.rect.width + "px;";
    let height = "height:" + options.rect.height + "px;";

    let style = x + y + width + height;
    if (options.fill) {
      style += "background:" + options.fill + ";";
    }

    // Set the coordinates of the highlighter and show it
    this.markup.setAttributeForElement("highlighted-rect", "style", style);
    this.markup.removeAttributeForElement("highlighted-rect", "hidden");
  },

  hide: function() {
    this.markup.setAttributeForElement("highlighted-rect", "hidden", "true");
  }
};
register(RectHighlighter);
exports.RectHighlighter = RectHighlighter;

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

/**
 * Helper function that creates SVG DOM nodes.
 * @param {Window} This window's document will be used to create the element
 * @param {Object} Options for the node include:
 * - nodeType: the type of node, defaults to "box".
 * - attributes: a {name:value} object to be used as attributes for the node.
 * - prefix: a string that will be used to prefix the values of the id and class
 *   attributes.
 * - parent: if provided, the newly created element will be appended to this
 *   node.
 */
function createSVGNode(win, options) {
  if (!options.nodeType) {
    options.nodeType = "box";
  }
  options.namespace = SVG_NS;
  return createNode(win, options);
}

/**
 * Helper function that creates DOM nodes.
 * @param {Window} This window's document will be used to create the element
 * @param {Object} Options for the node include:
 * - nodeType: the type of node, defaults to "div".
 * - namespace: if passed, doc.createElementNS will be used instead of
 *   doc.creatElement.
 * - attributes: a {name:value} object to be used as attributes for the node.
 * - prefix: a string that will be used to prefix the values of the id and class
 *   attributes.
 * - parent: if provided, the newly created element will be appended to this
 *   node.
 */
function createNode(win, options) {
  let type = options.nodeType || "div";

  let node;
  if (options.namespace) {
    node = win.document.createElementNS(options.namespace, type);
  } else {
    node = win.document.createElement(type);
  }

  for (let name in options.attributes || {}) {
    let value = options.attributes[name];
    if (options.prefix && (name === "class" || name === "id")) {
      value = options.prefix + value
    }
    node.setAttribute(name, value);
  }

  if (options.parent) {
    options.parent.appendChild(node);
  }

  return node;
}

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils)
});
