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
// Distance of the width or height handles from the node's edge.
const GEOMETRY_SIZE_ARROW_OFFSET = .25; // 25%
const GEOMETRY_LABEL_SIZE = 6;

// Maximum size, in pixel, for the horizontal ruler and vertical ruler
// used by RulersHighlighter
const RULERS_MAX_X_AXIS = 10000;
const RULERS_MAX_Y_AXIS = 15000;
// Number of steps after we add a graduation, marker and text in
// RulersHighliter; currently the unit is in pixel.
const RULERS_GRADUATION_STEP = 5;
const RULERS_MARKER_STEP = 50;
const RULERS_TEXT_STEP = 100;

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

    this._layoutHelpers = new LayoutHelpers(this._tabActor.window);
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
    this._layoutHelpers = null;
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
  _currentNode: null,

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
      if (!this._currentNode) {
        this._currentNode = this._findAndAttachElement(event);
      }
      events.emit(this._walker, "picker-node-picked", this._currentNode);
    };

    this._onHovered = event => {
      this._preventContentEvent(event);
      this._currentNode = this._findAndAttachElement(event);
      if (this._hoveredNode !== this._currentNode.node) {
        this._highlighter.show( this._currentNode.node.rawNode);
        events.emit(this._walker, "picker-node-hovered", this._currentNode);
        this._hoveredNode = this._currentNode.node;
      }
    };

    this._onKey = event => {
      if (!this._currentNode || !this._isPicking) {
        return;
      }

      this._preventContentEvent(event);
      let currentNode = this._currentNode.node.rawNode;

      /**
       * KEY: Action/scope
       * LEFT_KEY: wider or parent
       * RIGHT_KEY: narrower or child
       * ENTER/CARRIAGE_RETURN: Picks currentNode
       * ESC: Cancels picker, picks currentNode
       */
      switch(event.keyCode) {
        case Ci.nsIDOMKeyEvent.DOM_VK_LEFT: // wider
          if (!currentNode.parentElement) {
            return;
          }
          currentNode = currentNode.parentElement;
          break;

        case Ci.nsIDOMKeyEvent.DOM_VK_RIGHT: // narrower
          if (!currentNode.children.length) {
            return;
          }

          // Set firstElementChild by default
          let child = currentNode.firstElementChild;
          // If currentNode is parent of hoveredNode, then
          // previously selected childNode is set
          let hoveredNode = this._hoveredNode.rawNode;
          for (let sibling of currentNode.children) {
            if (sibling.contains(hoveredNode) || sibling === hoveredNode) {
              child = sibling;
            }
          }

          currentNode = child;
          break;

        case Ci.nsIDOMKeyEvent.DOM_VK_RETURN: // select element
          this._onPick(event);
          return;

        case Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE: // cancel picking
          this.cancelPick();
          events.emit(this._walker, "picker-node-canceled");
          return;

        default: return;
      }

      // Store currently attached element
      this._currentNode = this._walker.attachElement(currentNode);
      this._highlighter.show(this._currentNode.node.rawNode);
      events.emit(this._walker, "picker-node-hovered", this._currentNode);
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
    let target = getPageListenerTarget(this._tabActor);
    target.addEventListener("mousemove", this._onHovered, true);
    target.addEventListener("click", this._onPick, true);
    target.addEventListener("mousedown", this._preventContentEvent, true);
    target.addEventListener("mouseup", this._preventContentEvent, true);
    target.addEventListener("dblclick", this._preventContentEvent, true);
    target.addEventListener("keydown", this._onKey, true);
    target.addEventListener("keyup", this._preventContentEvent, true);
  },

  _stopPickerListeners: function() {
    let target = getPageListenerTarget(this._tabActor);
    target.removeEventListener("mousemove", this._onHovered, true);
    target.removeEventListener("click", this._onPick, true);
    target.removeEventListener("mousedown", this._preventContentEvent, true);
    target.removeEventListener("mouseup", this._preventContentEvent, true);
    target.removeEventListener("dblclick", this._preventContentEvent, true);
    target.removeEventListener("keydown", this._onKey, true);
    target.removeEventListener("keyup", this._preventContentEvent, true);
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

  this.listeners = new Map();
}

exports.CanvasFrameAnonymousContentHelper = CanvasFrameAnonymousContentHelper;

CanvasFrameAnonymousContentHelper.prototype = {
  destroy: function() {
    // If the current window isn't the one the content was inserted into, this
    // will fail, but that's fine.
    try {
      let doc = this.anonymousContentDocument;
      doc.removeAnonymousContent(this._content);
    } catch (e) {}
    events.off(this.tabActor, "navigate", this._onNavigate);
    this.tabActor = this.nodeBuilder = this._content = null;
    this.anonymousContentDocument = null;
    this.anonymousContentGlobal = null;

    this._removeAllListeners();
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
      this._removeAllListeners();
      this._insert();
      this.anonymousContentDocument = this.tabActor.window.document;
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

  /**
   * Add an event listener to one of the elements inserted in the canvasFrame
   * native anonymous container.
   * Like other methods in this helper, this requires the ID of the element to
   * be passed in.
   *
   * Note that if the content page navigates, the event listeners won't be
   * added again.
   *
   * Also note that unlike traditional DOM events, the events handled by
   * listeners added here will propagate through the document only through
   * bubbling phase, so the useCapture parameter isn't supported.
   * It is possible however to call e.stopPropagation() to stop the bubbling.
   *
   * IMPORTANT: the chrome-only canvasFrame insertion API takes great care of
   * not leaking references to inserted elements to chrome JS code. That's
   * because otherwise, chrome JS code could freely modify native anon elements
   * inside the canvasFrame and probably change things that are assumed not to
   * change by the C++ code managing this frame.
   * See https://wiki.mozilla.org/DevTools/Highlighter#The_AnonymousContent_API
   * Unfortunately, the inserted nodes are still available via
   * event.originalTarget, and that's what the event handler here uses to check
   * that the event actually occured on the right element, but that also means
   * consumers of this code would be able to access the inserted elements.
   * Therefore, the originalTarget property will be nullified before the event
   * is passed to your handler.
   *
   * IMPL DETAIL: A single event listener is added per event types only, at
   * browser level and if the event originalTarget is found to have the provided
   * ID, the callback is executed (and then IDs of parent nodes of the
   * originalTarget are checked too).
   *
   * @param {String} id
   * @param {String} type
   * @param {Function} handler
   */
  addEventListenerForElement: function(id, type, handler) {
    if (typeof id !== "string") {
      throw new Error("Expected a string ID in addEventListenerForElement but" +
        " got: " + id);
    }

    // If no one is listening for this type of event yet, add one listener.
    if (!this.listeners.has(type)) {
      let target = getPageListenerTarget(this.tabActor);
      target.addEventListener(type, this, true);
      // Each type entry in the map is a map of ids:handlers.
      this.listeners.set(type, new Map);
    }

    let listeners = this.listeners.get(type);
    listeners.set(id, handler);
  },

  /**
   * Remove an event listener from one of the elements inserted in the
   * canvasFrame native anonymous container.
   * @param {String} id
   * @param {String} type
   * @param {Function} handler
   */
  removeEventListenerForElement: function(id, type, handler) {
    let listeners = this.listeners.get(type);
    if (!listeners) {
      return;
    }
    listeners.delete(id);

    // If no one is listening for event type anymore, remove the listener.
    if (!this.listeners.has(type)) {
      let target = getPageListenerTarget(this.tabActor);
      target.removeEventListener(type, this, true);
    }
  },

  handleEvent: function(event) {
    let listeners = this.listeners.get(event.type);
    if (!listeners) {
      return;
    }

    // Hide the originalTarget property to avoid exposing references to native
    // anonymous elements. See addEventListenerForElement's comment.
    let isPropagationStopped = false;
    let eventProxy = new Proxy(event, {
      get: (obj, name) => {
        if (name === "originalTarget") {
          return null;
        } else if (name === "stopPropagation") {
          return () => {
            isPropagationStopped = true;
          };
        } else {
          return obj[name];
        }
      }
    });

    // Start at originalTarget, bubble through ancestors and call handlers when
    // needed.
    let node = event.originalTarget;
    while (node) {
      let handler = listeners.get(node.id);
      if (handler) {
        handler(eventProxy, node.id);
        if (isPropagationStopped) {
          break;
        }
      }
      node = node.parentNode;
    }
  },

  _removeAllListeners: function() {
    if (this.tabActor) {
      let target = getPageListenerTarget(this.tabActor);
      for (let [type] of this.listeners) {
        target.removeEventListener(type, this, true);
      }
    }
    this.listeners.clear();
  },

  getElement: function(id) {
    let self = this;
    return {
      getTextContent: () => self.getTextContentForElement(id),
      setTextContent: text => self.setTextContentForElement(id, text),
      setAttribute: (name, value) => self.setAttributeForElement(id, name, value),
      getAttribute: name => self.getAttributeForElement(id, name),
      removeAttribute: name => self.removeAttributeForElement(id, name),
      addEventListener: (type, handler) => {
        return self.addEventListenerForElement(id, type, handler);
      },
      removeEventListener: (type, handler) => {
        return self.removeEventListenerForElement(id, type, handler);
      }
    };
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
    let win = getWindow(this.currentNode);
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
    this.currentNode = null;
    this.layoutHelpers = null;
  }
};

/**
 * The BoxModelHighlighter draws the box model regions on top of a node.
 * If the node is a block box, then each region will be displayed as 1 polygon.
 * If the node is an inline box though, each region may be represented by 1 or
 * more polygons, depending on how many line boxes the inline element has.
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
 *         <path class="box-model-margin" points="..." />
 *         <path class="box-model-border" points="..." />
 *         <path class="box-model-padding" points="..." />
 *         <path class="box-model-content" points="..." />
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
        nodeType: "path",
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

  getElement: function(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
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
      let win = getWindow(this.currentNode);
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
    this.getElement("nodeinfobar-container").setAttribute("hidden", "true");
  },

  /**
   * Show the infobar
   */
  _showInfobar: function() {
    this.getElement("nodeinfobar-container").removeAttribute("hidden");
    this._updateInfobar();
  },

  /**
   * Hide the box model
   */
  _hideBoxModel: function() {
    this.getElement("elements").setAttribute("hidden", "true");
  },

  /**
   * Show the box model
   */
  _showBoxModel: function() {
    this.getElement("elements").removeAttribute("hidden");
  },

  /**
   * Calculate an outer quad based on the quads returned by getAdjustedQuads.
   * The BoxModelHighlighter may highlight more than one boxes, so in this case
   * create a new quad that "contains" all of these quads.
   * This is useful to position the guides and nodeinfobar.
   * This may happen if the BoxModelHighlighter is used to highlight an inline
   * element that spans line breaks.
   * @param {String} region The box-model region to get the outer quad for.
   * @return {Object} A quad-like object {p1,p2,p3,p4,bounds}
   */
  _getOuterQuad: function(region) {
    let quads = this.currentQuads[region];
    if (!quads.length) {
      return null;
    }

    let quad = {
      p1: {x: Infinity, y: Infinity},
      p2: {x: -Infinity, y: Infinity},
      p3: {x: -Infinity, y: -Infinity},
      p4: {x: Infinity, y: -Infinity},
      bounds: {
        bottom: -Infinity,
        height: 0,
        left: Infinity,
        right: -Infinity,
        top: Infinity,
        width: 0,
        x: 0,
        y: 0,
      }
    };

    for (let q of quads) {
      quad.p1.x = Math.min(quad.p1.x, q.p1.x);
      quad.p1.y = Math.min(quad.p1.y, q.p1.y);
      quad.p2.x = Math.max(quad.p2.x, q.p2.x);
      quad.p2.y = Math.min(quad.p2.y, q.p2.y);
      quad.p3.x = Math.max(quad.p3.x, q.p3.x);
      quad.p3.y = Math.max(quad.p3.y, q.p3.y);
      quad.p4.x = Math.min(quad.p4.x, q.p4.x);
      quad.p4.y = Math.max(quad.p4.y, q.p4.y);

      quad.bounds.bottom = Math.max(quad.bounds.bottom, q.bounds.bottom);
      quad.bounds.top = Math.min(quad.bounds.top, q.bounds.top);
      quad.bounds.left = Math.min(quad.bounds.left, q.bounds.left);
      quad.bounds.right = Math.max(quad.bounds.right, q.bounds.right);
    }
    quad.bounds.x = quad.bounds.left;
    quad.bounds.y = quad.bounds.top;
    quad.bounds.width = quad.bounds.right - quad.bounds.left;
    quad.bounds.height = quad.bounds.bottom - quad.bounds.top;

    return quad;
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
        let box = this.getElement(boxType);

        if (this.regionFill[boxType]) {
          box.setAttribute("style", "fill:" + this.regionFill[boxType]);
        } else {
          box.setAttribute("style", "");
        }

        if (!this.options.showOnly || this.options.showOnly === boxType) {
          // Highlighting all quads.
          let path = [];
          for (let {p1, p2, p3, p4} of this.currentQuads[boxType]) {
            path.push("M" + p1.x + "," + p1.y + " " +
                      "L" + p2.x + "," + p2.y + " " +
                      "L" + p3.x + "," + p3.y + " " +
                      "L" + p4.x + "," + p4.y);
          }

          box.setAttribute("d", path.join(" "));
        } else {
          box.removeAttribute("d");
        }

        if (boxType === this.options.region && !this.options.hideGuides) {
          this._showGuides(boxType);
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
    let hasNoQuads = !this.currentQuads.margin.length &&
                     !this.currentQuads.border.length &&
                     !this.currentQuads.padding.length &&
                     !this.currentQuads.content.length;
    if (!this.currentNode ||
        Cu.isDeadWrapper(this.currentNode) ||
        this.currentNode.nodeType !== Ci.nsIDOMNode.ELEMENT_NODE ||
        !this.currentNode.ownerDocument ||
        !getWindow(this.currentNode) ||
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
      let quad = this._getOuterQuad(region);

      if (!quad) {
        // Invisible element such as a script tag.
        break;
      }

      let {bottom, height, left, right, top, width, x, y} = quad.bounds;

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
   * @param {String} region The region around which the guides should be shown.
   */
  _showGuides: function(region) {
    let {p1, p2, p3, p4} = this._getOuterQuad(region);

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
      this.getElement("guide-" + side).setAttribute("hidden", "true");
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
    let guide = this.getElement("guide-" + side);

    if (point <= 0) {
      guide.setAttribute("hidden", "true");
      return false;
    }

    if (side === "top" || side === "bottom") {
      guide.setAttribute("x1", "0");
      guide.setAttribute("y1", point + "");
      guide.setAttribute("x2", "100%");
      guide.setAttribute("y2", point + "");
    } else {
      guide.setAttribute("x1", point + "");
      guide.setAttribute("y1", "0");
      guide.setAttribute("x2", point + "");
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

    let rect = this._getOuterQuad("border").bounds;
    let dim = parseFloat(rect.width.toPrecision(6)) + " \u00D7 " + parseFloat(rect.height.toPrecision(6));

    this.getElement("nodeinfobar-tagname").setTextContent(tagName);
    this.getElement("nodeinfobar-id").setTextContent(id);
    this.getElement("nodeinfobar-classes").setTextContent(classList);
    this.getElement("nodeinfobar-pseudo-classes").setTextContent(pseudos);
    this.getElement("nodeinfobar-dimensions").setTextContent(dim);

    this._moveInfobar();
  },

  /**
   * Move the Infobar to the right place in the highlighter.
   */
  _moveInfobar: function() {
    let bounds = this._getOuterBounds();
    let winHeight = this.win.innerHeight * LayoutHelpers.getCurrentZoom(this.win);
    let winWidth = this.win.innerWidth * LayoutHelpers.getCurrentZoom(this.win);

    // Ensure that containerBottom and containerTop are at least zero to avoid
    // showing tooltips outside the viewport.
    let containerBottom = Math.max(0, bounds.bottom) + NODE_INFOBAR_ARROW_SIZE;
    let containerTop = Math.min(winHeight, bounds.top);
    let container = this.getElement("nodeinfobar-container");

    // Can the bar be above the node?
    let top;
    if (containerTop < NODE_INFOBAR_HEIGHT) {
      // No. Can we move the bar under the node?
      if (containerBottom + NODE_INFOBAR_HEIGHT > winHeight) {
        // No. Let's move it inside.
        top = containerTop;
        container.setAttribute("position", "overlap");
      } else {
        // Yes. Let's move it under the node.
        top = containerBottom;
        container.setAttribute("position", "bottom");
      }
    } else {
      // Yes. Let's move it on top of the node.
      top = containerTop - NODE_INFOBAR_HEIGHT;
      container.setAttribute("position", "top");
    }

    // Align the bar with the box's center if possible.
    let left = bounds.right - bounds.width / 2;
    // Make sure the while infobar is visible.
    let buffer = 100;
    if (left < buffer) {
      left = buffer;
      container.setAttribute("hide-arrow", "true");
    } else if (left > winWidth - buffer) {
      left = winWidth - buffer;
      container.setAttribute("hide-arrow", "true");
    } else {
      container.removeAttribute("hide-arrow");
    }

    let style = "top:" + top + "px;left:" + left + "px;";
    container.setAttribute("style", style);
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

  getElement: function(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
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
    this.getElement(id).setAttribute("points", points.join(" "));
  },

  _setLinePoints: function(p1, p2, id) {
    let line = this.getElement(id);
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
    setIgnoreLayoutChanges(true);

    // Getting the points for the transformed shape
    let quads = this.currentQuads.border;
    if (!quads.length ||
        quads[0].bounds.width <= 0 || quads[0].bounds.height <= 0) {
      this._hideShapes();
      return null;
    }

    let [quad] = quads;

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
    this.getElement("elements").setAttribute("hidden", "true");
  },

  _showShapes: function() {
    this.getElement("elements").removeAttribute("hidden");
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

  getElement: function(id) {
    return this.markup.getElement(id);
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
    let quads = this.layoutHelpers.getAdjustedQuads(contextNode);
    if (!quads.length) {
      this.hide();
      return;
    }

    let {bounds} = quads[0];
    let x = "left:" + (bounds.x + options.rect.x) + "px;";
    let y = "top:" + (bounds.y + options.rect.y) + "px;";
    let width = "width:" + options.rect.width + "px;";
    let height = "height:" + options.rect.height + "px;";

    let style = x + y + width + height;
    if (options.fill) {
      style += "background:" + options.fill + ";";
    }

    // Set the coordinates of the highlighter and show it
    let rect = this.getElement("highlighted-rect");
    rect.setAttribute("style", style);
    rect.removeAttribute("hidden");
  },

  hide: function() {
    this.getElement("highlighted-rect").setAttribute("hidden", "true");
  }
};
register(RectHighlighter);
exports.RectHighlighter = RectHighlighter;

/**
 * Element geometry properties helper that gives names of position and size
 * properties.
 */
let GeoProp = {
  SIDES: ["top", "right", "bottom", "left"],
  SIZES: ["width", "height"],

  allProps: function() {
    return [...this.SIDES, ...this.SIZES];
  },

  isSide: function(name) {
    return this.SIDES.indexOf(name) !== -1;
  },

  isSize: function(name) {
    return this.SIZES.indexOf(name) !== -1;
  },

  containsSide: function(names) {
    return names.some(name => this.SIDES.indexOf(name) !== -1);
  },

  containsSize: function(names) {
    return names.some(name => this.SIZES.indexOf(name) !== -1);
  },

  isHorizontal: function(name) {
    return name === "left" || name === "right" || name === "width";
  },

  isInverted: function(name) {
    return name === "right" || name === "bottom";
  },

  mainAxisStart: function(name) {
    return this.isHorizontal(name) ? "left" : "top";
  },

  crossAxisStart: function(name) {
    return this.isHorizontal(name) ? "top" : "left";
  },

  mainAxisSize: function(name) {
    return this.isHorizontal(name) ? "width" : "height";
  },

  crossAxisSize: function(name) {
    return this.isHorizontal(name) ? "height" : "width";
  },

  axis: function(name) {
    return this.isHorizontal(name) ? "x" : "y";
  },

  crossAxis: function(name) {
    return this.isHorizontal(name) ? "y" : "x";
  }
};

/**
 * The GeometryEditor highlights an elements's top, left, bottom, right, width
 * and height dimensions, when they are set.
 *
 * To determine if an element has a set size and position, the highlighter lists
 * the CSS rules that apply to the element and checks for the top, left, bottom,
 * right, width and height properties.
 * The highlighter won't be shown if the element doesn't have any of these
 * properties set, but will be shown when at least 1 property is defined.
 *
 * The highlighter displays lines and labels for each of the defined properties
 * in and around the element (relative to the offset parent when one exists).
 * The highlighter also highlights the element itself and its offset parent if
 * there is one.
 *
 * Note that the class name contains the word Editor because the aim is for the
 * handles to be draggable in content to make the geometry editable.
 */
function GeometryEditorHighlighter(tabActor) {
  AutoRefreshHighlighter.call(this, tabActor);

  // The list of element geometry properties that can be set.
  this.definedProperties = new Map();

  this.markup = new CanvasFrameAnonymousContentHelper(tabActor,
    this._buildMarkup.bind(this));
}

GeometryEditorHighlighter.prototype = Heritage.extend(AutoRefreshHighlighter.prototype, {
  typeName: "GeometryEditorHighlighter",

  ID_CLASS_PREFIX: "geometry-editor-",

  _buildMarkup: function() {
    let container = createNode(this.win, {
      attributes: {"class": "highlighter-container"}
    });

    let root = createNode(this.win, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let svg = createSVGNode(this.win, {
      nodeType: "svg",
      parent: root,
      attributes: {
        "id": "elements",
        "width": "100%",
        "height": "100%"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Offset parent node highlighter.
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: svg,
      attributes: {
        "class": "offset-parent",
        "id": "offset-parent",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Current node highlighter (margin box).
    createSVGNode(this.win, {
      nodeType: "polygon",
      parent: svg,
      attributes: {
        "class": "current-node",
        "id": "current-node",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    // Build the 4 side arrows and labels.
    for (let name of GeoProp.SIDES) {
      createSVGNode(this.win, {
        nodeType: "line",
        parent: svg,
        attributes: {
          "class": "arrow " + name,
          "id": "arrow-" + name,
          "hidden": "true"
        },
        prefix: this.ID_CLASS_PREFIX
      });

      // Labels are positioned by using a translated <g>. This group contains
      // a path and text that are themselves positioned using another translated
      // <g>. This is so that the label arrow points at the 0,0 coordinates of
      // parent <g>.
      let labelG = createSVGNode(this.win, {
        nodeType: "g",
        parent: svg,
        attributes: {
          "id": "label-" + name,
          "hidden": "true"
        },
        prefix: this.ID_CLASS_PREFIX
      });

      let subG = createSVGNode(this.win, {
        nodeType: "g",
        parent: labelG,
        attributes: {
          "transform": GeoProp.isHorizontal(name)
                       ? "translate(-30 -30)"
                       : "translate(5 -10)"
        }
      });

      createSVGNode(this.win, {
        nodeType: "path",
        parent: subG,
        attributes: {
          "class": "label-bubble",
          "d": GeoProp.isHorizontal(name)
               ? "M0 0 L60 0 L60 20 L35 20 L30 25 L25 20 L0 20z"
               : "M5 0 L65 0 L65 20 L5 20 L5 15 L0 10 L5 5z"
        },
        prefix: this.ID_CLASS_PREFIX
      });

      createSVGNode(this.win, {
        nodeType: "text",
        parent: subG,
        attributes: {
          "class": "label-text",
          "id": "label-text-" + name,
          "x": GeoProp.isHorizontal(name) ? "30" : "35",
          "y": "10"
        },
        prefix: this.ID_CLASS_PREFIX
      });
    }

    // Build the width/height label and resize handle.
    let labelSizeG = createSVGNode(this.win, {
      nodeType: "g",
      parent: svg,
      attributes: {
        "id": "label-size",
        "hidden": "true"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    let subSizeG = createSVGNode(this.win, {
      nodeType: "g",
      parent: labelSizeG,
      attributes: {
        "transform": "translate(-50 -10)"
      }
    });

    createSVGNode(this.win, {
      nodeType: "path",
      parent: subSizeG,
      attributes: {
        "class": "label-bubble",
        "d": "M0 0 L100 0 L100 20 L0 20z"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    createSVGNode(this.win, {
      nodeType: "text",
      parent: subSizeG,
      attributes: {
        "class": "label-text",
        "id": "label-text-size",
        "x": "50",
        "y": "10"
      },
      prefix: this.ID_CLASS_PREFIX
    });

    return container;
  },

  destroy: function() {
    AutoRefreshHighlighter.prototype.destroy.call(this);

    this.markup.destroy();
    this.definedProperties.clear();
    this.definedProperties = null;
    this.offsetParent = null;
  },

  getElement: function(id) {
    return this.markup.getElement(this.ID_CLASS_PREFIX + id);
  },

  /**
   * Get the list of geometry properties that are actually set on the current
   * node.
   * @return {Map} A map indexed by property name and where the value is an
   * object having the cssRule property.
   */
  getDefinedGeometryProperties: function() {
    let props = new Map();
    if (!this.currentNode) {
      return props;
    }

    // Get the list of css rules applying to the current node.
    let cssRules = DOMUtils.getCSSStyleRules(this.currentNode);
    for (let i = 0; i < cssRules.Count(); i++) {
      let rule = cssRules.GetElementAt(i);
      for (let name of GeoProp.allProps()) {
        let value = rule.style.getPropertyValue(name);
        if (value && value !== "auto") {
          // getCSSStyleRules returns rules ordered from least-specific to
          // most-specific, so just override any previous properties we have set.
          props.set(name, {
            cssRule: rule
          });
        }
      }
    }

    // Go through the inline styles last.
    for (let name of GeoProp.allProps()) {
      let value = this.currentNode.style.getPropertyValue(name);
      if (value && value !== "auto") {
        props.set(name, {
          // There's no cssRule to store here, so store the node instead since
          // node.style exists.
          cssRule: this.currentNode
        });
      }
    }

    // Post-process the list for invalid properties. This is done after the fact
    // because of cases like relative positioning with both top and bottom where
    // only top will actually be used, but both exists in css rules and computed
    // styles.
    for (let [name] of props) {
      let pos = this.computedStyle.position;

      // Top/left/bottom/right on static positioned elements have no effect.
      if (pos === "static" && GeoProp.SIDES.indexOf(name) !== -1) {
        props.delete(name);
      }

      // Bottom/right on relative positioned elements are only used if top/left
      // are not defined.
      let hasRightAndLeft = name === "right" && props.has("left");
      let hasBottomAndTop = name === "bottom" && props.has("top");
      if (pos === "relative" && (hasRightAndLeft || hasBottomAndTop)) {
        props.delete(name);
      }
    }

    return props;
  },

  _show: function() {
    this.computedStyle = CssLogic.getComputedStyle(this.currentNode);
    let pos = this.computedStyle.position;
    // XXX: sticky positioning is ignored for now. To be implemented next.
    if (pos === "sticky") {
      this.hide();
      return;
    }

    let hasUpdated = this._update();
    if (!hasUpdated) {
      this.hide();
    }
  },

  _update: function() {
    // At each update, the position or/and size may have changed, so get the
    // list of defined properties, and re-position the arrows and highlighters.
    this.definedProperties = this.getDefinedGeometryProperties();

    let isStatic = this.computedStyle.position === "static";
    let hasSizes = GeoProp.containsSize([...this.definedProperties.keys()]);

    if (!this.definedProperties.size) {
      console.warn("The element does not have editable geometry properties");
      return false;
    }

    setIgnoreLayoutChanges(true);

    // Update the highlighters and arrows.
    this.updateOffsetParent();
    this.updateCurrentNode();
    this.updateArrows();
    this.updateSize();

    // Avoid zooming the arrows when content is zoomed.
    this.markup.scaleRootElement(this.currentNode, this.ID_CLASS_PREFIX + "root");

    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
    return true;
  },

  /**
   * Update the offset parent rectangle.
   * There are 3 different cases covered here:
   * - the node is absolutely/fixed positioned, and an offsetParent is defined
   *   (i.e. it's not just positioned in the viewport): the offsetParent node
   *   is highlighted (i.e. the rectangle is shown),
   * - the node is relatively positioned: the rectangle is shown where the node
   *   would originally have been (because that's where the relative positioning
   *   is calculated from),
   * - the node has no offset parent at all: the offsetParent rectangle is
   *   hidden.
   */
  updateOffsetParent: function() {
    // Get the offsetParent, if any.
    this.offsetParent = getOffsetParent(this.currentNode);
    // And the offsetParent quads.
    this.parentQuads = this.layoutHelpers
                      .getAdjustedQuads(this.offsetParent.element, "padding");

    let el = this.getElement("offset-parent");

    let isPositioned = this.computedStyle.position === "absolute" ||
                       this.computedStyle.position === "fixed";
    let isRelative = this.computedStyle.position === "relative";
    let isHighlighted = false;

    if (this.offsetParent.element && isPositioned) {
      let {p1, p2, p3, p4} = this.parentQuads[0];
      let points = p1.x + "," + p1.y + " " +
                   p2.x + "," + p2.y + " " +
                   p3.x + "," + p3.y + " " +
                   p4.x + "," + p4.y;
      el.setAttribute("points", points);
      isHighlighted = true;
    } else if (isRelative) {
      let xDelta = parseFloat(this.computedStyle.left);
      let yDelta = parseFloat(this.computedStyle.top);
      if (xDelta || yDelta) {
        let {p1, p2, p3, p4} = this.currentQuads.margin[0];
        let points = (p1.x - xDelta) + "," + (p1.y - yDelta) + " " +
                     (p2.x - xDelta) + "," + (p2.y - yDelta) + " " +
                     (p3.x - xDelta) + "," + (p3.y - yDelta) + " " +
                     (p4.x - xDelta) + "," + (p4.y - yDelta);
        el.setAttribute("points", points);
        isHighlighted = true;
      }
    }

    if (isHighlighted) {
      el.removeAttribute("hidden");
    } else {
      el.setAttribute("hidden", "true");
    }
  },

  updateCurrentNode: function() {
    let box = this.getElement("current-node");
    let {p1, p2, p3, p4} = this.currentQuads.margin[0];
    let attr = p1.x + "," + p1.y + " " +
               p2.x + "," + p2.y + " " +
               p3.x + "," + p3.y + " " +
               p4.x + "," + p4.y;
    box.setAttribute("points", attr);
    box.removeAttribute("hidden");
  },

  _hide: function() {
    setIgnoreLayoutChanges(true);

    this.getElement("current-node").setAttribute("hidden", "true");
    this.getElement("offset-parent").setAttribute("hidden", "true");
    this.hideArrows();
    this.hideSize();

    this.definedProperties.clear();

    setIgnoreLayoutChanges(false, this.currentNode.ownerDocument.documentElement);
  },

  hideArrows: function() {
    for (let side of GeoProp.SIDES) {
      this.getElement("arrow-" + side).setAttribute("hidden", "true");
      this.getElement("label-" + side).setAttribute("hidden", "true");
    }
  },

  hideSize: function() {
    this.getElement("label-size").setAttribute("hidden", "true");
  },

  updateSize: function() {
    this.hideSize();

    let labels = [];
    let width = this.definedProperties.get("width");
    let height = this.definedProperties.get("height");

    if (width) {
      labels.push("↔ " + width.cssRule.style.getPropertyValue("width"));
    }
    if (height) {
      labels.push("↕ " + height.cssRule.style.getPropertyValue("height"));
    }

    if (labels.length) {
      let labelEl = this.getElement("label-size");
      let labelTextEl = this.getElement("label-text-size");

      let {bounds} = this.currentQuads.margin[0];

      labelEl.setAttribute("transform", "translate(" +
        (bounds.left + bounds.width/2) + " " +
        (bounds.top + bounds.height/2) + ")");
      labelEl.removeAttribute("hidden");
      labelTextEl.setTextContent(labels.join(" "));
    }
  },

  updateArrows: function() {
    this.hideArrows();

    // Position arrows always end at the node's margin box.
    let marginBox = this.currentQuads.margin[0].bounds;
    // But size arrows are displayed in the box that corresponds to the current
    // box-sizing.
    let boxSizing = this.computedStyle.boxSizing.split("-")[0];
    let box = this.currentQuads[boxSizing][0].bounds;

    // Position the side arrows which need to be visible.
    // Arrows always start at the offsetParent edge, and end at the middle
    // position of the node's margin edge.
    // Note that for relative positioning, the offsetParent is considered to be
    // the node itself, where it would have been originally.
    // +------------------+----------------+
    // | offsetparent     | top            |
    // | or viewport      |                |
    // |         +--------+--------+       |
    // |         | node            |       |
    // +---------+                 +-------+
    // | left    |                 | right |
    // |         +--------+--------+       |
    // |                  | bottom         |
    // +------------------+----------------+
    let getSideArrowStartPos = side => {
      // In case an offsetParent exists and is highlighted.
      if (this.parentQuads && this.parentQuads.length) {
        return this.parentQuads[0].bounds[side];
      }

      // In case of relative positioning.
      if (this.computedStyle.position === "relative") {
        if (GeoProp.isInverted(side)) {
          return marginBox[side] + parseFloat(this.computedStyle[side]);
        } else {
          return marginBox[side] - parseFloat(this.computedStyle[side]);
        }
      }

      // In case the element is positioned in the viewport.
      if (GeoProp.isInverted(side)) {
        return this.offsetParent.dimension[GeoProp.mainAxisSize(side)];
      } else {
        return -1 * getWindow(this.currentNode)["scroll" +
                                                GeoProp.axis(side).toUpperCase()];
      }
    };

    for (let side of GeoProp.SIDES) {
      let sideProp = this.definedProperties.get(side);
      if (!sideProp) {
        continue;
      }

      let mainAxisStartPos = getSideArrowStartPos(side);
      let mainAxisEndPos = marginBox[side];
      let crossAxisPos = marginBox[GeoProp.crossAxisStart(side)] +
                         marginBox[GeoProp.crossAxisSize(side)] / 2;

      this.updateArrow(side, mainAxisStartPos, mainAxisEndPos, crossAxisPos,
                       sideProp.cssRule.style.getPropertyValue(side));
    }
  },

  updateArrow: function(side, mainStart, mainEnd, crossPos, labelValue) {
    let arrowEl = this.getElement("arrow-" + side);
    let labelEl = this.getElement("label-" + side);
    let labelTextEl = this.getElement("label-text-" + side);

    // Position the arrow <line>.
    arrowEl.setAttribute(GeoProp.axis(side) + "1", mainStart);
    arrowEl.setAttribute(GeoProp.crossAxis(side) + "1", crossPos);
    arrowEl.setAttribute(GeoProp.axis(side) + "2", mainEnd);
    arrowEl.setAttribute(GeoProp.crossAxis(side) + "2", crossPos);
    arrowEl.removeAttribute("hidden");

    // Position the label <text> in the middle of the arrow (making sure it's
    // not hidden below the fold).
    let capitalize = str => str.substring(0, 1).toUpperCase() + str.substring(1);
    let winMain = this.win["inner" + capitalize(GeoProp.mainAxisSize(side))]
    let labelMain = mainStart + (mainEnd - mainStart) / 2;
    if ((mainStart > 0 && mainStart < winMain) ||
        (mainEnd > 0 && mainEnd < winMain)) {
      if (labelMain < GEOMETRY_LABEL_SIZE) {
        labelMain = GEOMETRY_LABEL_SIZE;
      } else if (labelMain > winMain - GEOMETRY_LABEL_SIZE) {
        labelMain = winMain - GEOMETRY_LABEL_SIZE;
      }
    }
    let labelCross = crossPos;
    labelEl.setAttribute("transform", GeoProp.isHorizontal(side)
                         ? "translate(" + labelMain + " " + labelCross + ")"
                         : "translate(" + labelCross + " " + labelMain + ")");
    labelEl.removeAttribute("hidden");
    labelTextEl.setTextContent(labelValue);
  }
});
register(GeometryEditorHighlighter);
exports.GeometryEditorHighlighter = GeometryEditorHighlighter;

/**
 * The RulersHighlighter is a class that displays both horizontal and
 * vertical rules on the page, along the top and left edges, with pixel
 * graduations, useful for users to quickly check distances
 */
function RulersHighlighter(tabActor) {
  this.win = tabActor.window;
  this.markup = new CanvasFrameAnonymousContentHelper(tabActor,
    this._buildMarkup.bind(this));

  this.win.addEventListener("scroll", this, true);
  this.win.addEventListener("pagehide", this, true);
}

RulersHighlighter.prototype =  {
  typeName: "RulersHighlighter",

  ID_CLASS_PREFIX: "rulers-highlighter-",

  _buildMarkup: function() {
    let prefix = this.ID_CLASS_PREFIX;
    let window = this.win;

    function createRuler(axis, size) {
      let width, height;
      let isHorizontal = true;

      if (axis === "x") {
        width = size;
        height = 16;
      } else if (axis === "y") {
        width = 16;
        height = size;
        isHorizontal = false;
      } else {
        throw new Error(`Invalid type of axis given; expected "x" or "y" but got "${axis}"`);
      }

      let g = createSVGNode(window, {
        nodeType: "g",
        attributes: {
          id: `${axis}-axis`
        },
        parent: svg,
        prefix
      });

      createSVGNode(window, {
        nodeType: "rect",
        attributes: {
          y: isHorizontal ? 0 : 16,
          width,
          height
        },
        parent: g
      });

      let gRule = createSVGNode(window, {
        nodeType: "g",
        attributes: {
          id: `${axis}-axis-ruler`
        },
        parent: g,
        prefix
      });

      let pathGraduations = createSVGNode(window, {
        nodeType: "path",
        attributes: {
          "class": "ruler-graduations",
          width,
          height
        },
        parent: gRule,
        prefix
      });

      let pathMarkers = createSVGNode(window, {
        nodeType: "path",
        attributes: {
          "class": "ruler-markers",
          width,
          height
        },
        parent: gRule,
        prefix
      });

      let gText = createSVGNode(window, {
        nodeType: "g",
        attributes: {
          id: `${axis}-axis-text`,
          "class": (isHorizontal ? "horizontal" : "vertical") + "-labels"
        },
        parent: g,
        prefix
      });

      let dGraduations = "";
      let dMarkers = "";
      let graduationLength;

      for (let i = 0; i < size; i+=RULERS_GRADUATION_STEP) {
        if (i === 0) continue;

        graduationLength = (i % 2 === 0) ? 6 : 4;

        if (i % RULERS_TEXT_STEP === 0) {
          graduationLength = 8;
          createSVGNode(window, {
            nodeType: "text",
            parent: gText,
            attributes: {
              x: isHorizontal ? 2 + i : -i - 1,
              y: 5
            }
          }).textContent = i;
        }

        if (isHorizontal) {
          if (i % RULERS_MARKER_STEP === 0)
            dMarkers += `M${i} 0 L${i} ${graduationLength}`;
          else
            dGraduations += `M${i} 0 L${i} ${graduationLength} `;

        } else {
          if (i % 50 === 0)
            dMarkers += `M0 ${i} L${graduationLength} ${i}`;
          else
            dGraduations += `M0 ${i} L${graduationLength} ${i}`;
        }
      }

      pathGraduations.setAttribute("d", dGraduations);
      pathMarkers.setAttribute("d", dMarkers);

      return g;
    }

    let container = createNode(window, {
      attributes: {"class": "highlighter-container"}
    });

    let root = createNode(window, {
      parent: container,
      attributes: {
        "id": "root",
        "class": "root"
      },
      prefix
    });

    let svg = createSVGNode(window, {
      nodeType: "svg",
      parent: root,
      attributes: {
        id: "elements",
        "class": "elements",
        width: "100%",
        height: "100%",
        hidden: "true"
      },
      prefix
    });

    createRuler("x", RULERS_MAX_X_AXIS);
    createRuler("y", RULERS_MAX_Y_AXIS);

    return container;
  },

  handleEvent: function(event) {
    switch (event.type) {
      case "scroll":
        this._onScroll(event);
        break;
      case "pagehide":
        this.destroy();
        break;
    }
  },

  _onScroll: function(event) {
    let prefix = this.ID_CLASS_PREFIX;
    let { scrollX, scrollY } = event.view;

    this.markup.getElement(`${prefix}x-axis-ruler`)
                        .setAttribute("transform", `translate(${-scrollX})`);
    this.markup.getElement(`${prefix}x-axis-text`)
                        .setAttribute("transform", `translate(${-scrollX})`);
    this.markup.getElement(`${prefix}y-axis-ruler`)
                        .setAttribute("transform", `translate(0, ${-scrollY})`);
    this.markup.getElement(`${prefix}y-axis-text`)
                        .setAttribute("transform", `translate(0, ${-scrollY})`);
  },

  destroy: function() {
    this.hide();

    this.win.removeEventListener("scroll", this, true);
    this.win.removeEventListener("pagehide", this, true);

    this.markup.destroy();

    events.emit(this, "destroy");
  },

  show: function() {
    this.markup.removeAttributeForElement(this.ID_CLASS_PREFIX + "elements",
      "hidden");
  },

  hide: function() {
    this.markup.setAttributeForElement(this.ID_CLASS_PREFIX + "elements",
      "hidden", "true");
  }
};

register(RulersHighlighter);
exports.RulersHighlighter = RulersHighlighter;

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
      installHelperSheet(getWindow(node), SIMPLE_OUTLINE_SHEET);
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

/**
 * Get the right target for listening to events on the page (while picking an
 * element for instance).
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
function getPageListenerTarget(tabActor) {
  return tabActor.isRootActor ? tabActor.window : tabActor.chromeEventHandler;
}

/**
 * Get a node's owner window.
 */
function getWindow(node) {
  return node.ownerDocument.defaultView;
}

/**
 * Get the provided node's offsetParent dimensions.
 * Returns an object with the {parent, dimension} properties.
 * Note that the returned parent will be null if the offsetParent is the
 * default, non-positioned, body or html node.
 *
 * node.offsetParent returns the nearest positioned ancestor but if it is
 * non-positioned itself, we just return null to let consumers know the node is
 * actually positioned relative to the viewport.
 *
 * @return {Object}
 */
function getOffsetParent(node) {
  let offsetParent = node.offsetParent;
  if (offsetParent &&
      CssLogic.getComputedStyle(offsetParent).position === "static") {
    offsetParent = null;
  }

  let width, height;
  if (!offsetParent) {
    height = getWindow(node).innerHeight;
    width = getWindow(node).innerWidth;
  } else {
    height = offsetParent.offsetHeight;
    width = offsetParent.offsetWidth;
  }

  return {
    element: offsetParent,
    dimension: {width, height}
  };
}

XPCOMUtils.defineLazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils)
});
