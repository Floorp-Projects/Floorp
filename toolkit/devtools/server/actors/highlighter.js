/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cu, Cc, Ci} = require("chrome");
const Services = require("Services");
const protocol = require("devtools/server/protocol");
const {Arg, Option, method} = protocol;
const events = require("sdk/event/core");

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

  initialize: function(inspector, autohide) {
    protocol.Actor.prototype.initialize.call(this, null);

    this._autohide = autohide;
    this._inspector = inspector;
    this._walker = this._inspector.walker;
    this._tabActor = this._inspector.tabActor;

    this._highlighterReady = this._highlighterReady.bind(this);
    this._highlighterHidden = this._highlighterHidden.bind(this);

    if (this._supportsBoxModelHighlighter()) {
      this._boxModelHighlighter =
        new BoxModelHighlighter(this._tabActor, this._inspector);

        this._boxModelHighlighter.on("ready", this._highlighterReady);
        this._boxModelHighlighter.on("hide", this._highlighterHidden);
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
    // Note that <browser>s on Fennec also have a XUL parentNode but the box
    // model highlighter doesn't display correctly on Fennec (bug 993190)
    return this._tabActor.browser &&
           !!this._tabActor.browser.parentNode &&
           Services.appinfo.ID !== "{aa3c5121-dab2-40e2-81ca-7ea25febc110}";
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    if (this._boxModelHighlighter) {
      this._boxModelHighlighter.off("ready", this._highlighterReady);
      this._boxModelHighlighter.off("hide", this._highlighterHidden);
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
    if (node && this._isNodeValidForHighlighting(node.rawNode)) {
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
function BoxModelHighlighter(tabActor, inspector) {
  this.browser = tabActor.browser;
  this.win = tabActor.window;
  this.chromeDoc = this.browser.ownerDocument;
  this.chromeWin = this.chromeDoc.defaultView;
  this._inspector = inspector;

  this.layoutHelpers = new LayoutHelpers(this.win);
  this.chromeLayoutHelper = new LayoutHelpers(this.chromeWin);

  this.transitionDisabler = null;
  this.pageEventsMuter = null;
  this._update = this._update.bind(this);
  this.handleEvent = this.handleEvent.bind(this);
  this.currentNode = null;

  EventEmitter.decorate(this);
  this._initMarkup();
}

BoxModelHighlighter.prototype = {
  get zoom() {
    return this.win.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindowUtils).fullZoom;
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
    this.hide();

    this.chromeWin.clearTimeout(this.transitionDisabler);
    this.chromeWin.clearTimeout(this.pageEventsMuter);

    this.nodeInfo = null;

    this._highlighterContainer.remove();
    this._highlighterContainer = null;

    this.rect = null;
    this.win = null;
    this.browser = null;
    this.chromeDoc = null;
    this.chromeWin = null;
    this.currentNode = null;
  },

  /**
   * Show the highlighter on a given node
   *
   * @param {DOMNode} node
   * @param {Object} options
   *        Object used for passing options
   */
  show: function(node, options={}) {
    this.currentNode = node;

    this._showInfobar();
    this._detachPageListeners();
    this._attachPageListeners();
    this._update();
    this._trackMutations();
  },

  _trackMutations: function() {
    if (this.currentNode) {
      let win = this.currentNode.ownerDocument.defaultView;
      this.currentNodeObserver = new win.MutationObserver(() => {
        this._update();
      });
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
   * @param {Object} options
   *        Object used for passing options. Valid options are:
   *          - box: "content", "padding", "border" or "margin." This specifies
   *            the box that the guides should outline. Default is content.
   */
  _update: function(options={}) {
    if (this.currentNode) {
      if (this._highlightBoxModel(options)) {
        this._showInfobar();
      } else {
        // Nothing to highlight (0px rectangle like a <script> tag for instance)
        this.hide();
      }
      this.emit("ready");
    }
  },

  /**
   * Hide the highlighter, the outline and the infobar.
   */
  hide: function() {
    if (this.currentNode) {
      this._untrackMutations();
      this.currentNode = null;
      this._hideBoxModel();
      this._hideInfobar();
      this._detachPageListeners();
    }
    this.emit("hide");
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
   * Highlight the box model.
   *
   * @param {Object} options
   *        Object used for passing options. Valid options are:
   *          - region: "content", "padding", "border" or "margin." This specifies
   *            the region that the guides should outline. Default is content.
   * @return {boolean}
   *         True if the rectangle was highlighted, false otherwise.
   */
  _highlightBoxModel: function(options) {
    let isShown = false;

    options.region = options.region || "content";

    this.rect = this.layoutHelpers.getAdjustedQuads(this.currentNode, "margin");

    if (!this.rect) {
      return null;
    }

    if (this.rect.bounds.width > 0 && this.rect.bounds.height > 0) {
      for (let boxType in this._boxModelNodes) {
        let {p1, p2, p3, p4} = boxType === "margin" ? this.rect :
          this.layoutHelpers.getAdjustedQuads(this.currentNode, boxType);

        let boxNode = this._boxModelNodes[boxType];
        boxNode.setAttribute("points",
                             p1.x + "," + p1.y + " " +
                             p2.x + "," + p2.y + " " +
                             p3.x + "," + p3.y + " " +
                             p4.x + "," + p4.y);

        if (boxType === options.region) {
          this._showGuides(p1, p2, p3, p4);
        }
      }

      isShown = true;
      this._showBoxModel();
    } else {
      // Only return false if the element really is invisible.
      // A height of 0 and a non-0 width corresponds to a visible element that
      // is below the fold for instance
      if (this.rect.width > 0 || this.rect.height > 0) {
        isShown = true;
        this._hideBoxModel();
      }
    }
    return isShown;
  },

  /**
   * We only want to show guides for horizontal and vertical edges as this helps
   * to line them up. This method finds these edges and displays a guide there.
   *
   * @param  {DOMPoint} p1
   *                    Point 1
   * @param  {DOMPoint} p2
   *                    Point 2
   * @param  {DOMPoint} p3 [description]
   *                    Point 3
   * @param  {DOMPoint} p4 [description]
   *                    Point 4
   */
  _showGuides: function(p1, p2, p3, p4) {
    let allX = [p1.x, p2.x, p3.x, p4.x].sort();
    let allY = [p1.y, p2.y, p3.y, p4.y].sort();
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
    if (point > 0) {
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
    } else {
      guide.setAttribute("hidden", "true");
      return false;
    }
  },

  /**
   * Update node information (tagName#id.class)
   */
  _updateInfobar: function() {
    if (!this.currentNode) {
      return;
    }

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

    // Dimensions
    let dimensionBox = this.nodeInfo.dimensionBox;
    let rect = this.currentNode.getBoundingClientRect();
    dimensionBox.textContent = Math.ceil(rect.width) + " x " +
                               Math.ceil(rect.height);
    this._moveInfobar();
  },

  /**
   * Move the Infobar to the right place in the highlighter.
   */
  _moveInfobar: function() {
    if (this.rect) {
      let bounds = this.rect.bounds;
      let winHeight = this.win.innerHeight * this.zoom;
      let winWidth = this.win.innerWidth * this.zoom;

      this.nodeInfo.positioner.removeAttribute("disabled");
      // Can the bar be above the node?
      if (bounds.top < this.nodeInfo.barHeight) {
        // No. Can we move the toolbar under the node?
        if (bounds.bottom + this.nodeInfo.barHeight > winHeight) {
          // No. Let's move it inside.
          this.nodeInfo.positioner.style.top = bounds.top + "px";
          this.nodeInfo.positioner.setAttribute("position", "overlap");
        } else {
          // Yes. Let's move it under the node.
          this.nodeInfo.positioner.style.top = bounds.bottom - INFO_BAR_OFFSET + "px";
          this.nodeInfo.positioner.setAttribute("position", "bottom");
        }
      } else {
        // Yes. Let's move it on top of the node.
        this.nodeInfo.positioner.style.top =
          bounds.top + INFO_BAR_OFFSET - this.nodeInfo.barHeight + "px";
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
    } else {
      this.nodeInfo.positioner.style.left = "0";
      this.nodeInfo.positioner.style.top = "0";
      this.nodeInfo.positioner.setAttribute("position", "top");
      this.nodeInfo.positioner.setAttribute("hide-arrow", "true");
    }
  },

  _attachPageListeners: function() {
    if (this.currentNode) {
      let win = this.currentNode.ownerGlobal;

      win.addEventListener("scroll", this, false);
      win.addEventListener("resize", this, false);
      win.addEventListener("MozAfterPaint", this, false);
    }
  },

  _detachPageListeners: function() {
    if (this.currentNode) {
      let win = this.currentNode.ownerGlobal;

      win.removeEventListener("scroll", this, false);
      win.removeEventListener("resize", this, false);
      win.removeEventListener("MozAfterPaint", this, false);
    }
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
      case "MozAfterPaint":
      case "scroll":
        this._update();
        break;
    }
  },
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
