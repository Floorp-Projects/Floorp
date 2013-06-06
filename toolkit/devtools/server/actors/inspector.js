/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Here's the server side of the remote inspector.
 *
 * The WalkerActor is the client's view of the debuggee's DOM.  It's gives
 * the client a tree of NodeActor objects.
 *
 * The walker presents the DOM tree mostly unmodified from the source DOM
 * tree, but with a few key differences:
 *
 *  - Empty text nodes are ignored.  This is pretty typical of developer
 *    tools, but maybe we should reconsider that on the server side.
 *  - iframes with documents loaded have the loaded document as the child,
 *    the walker provides one big tree for the whole document tree.
 *
 * There are a few ways to get references to NodeActors:
 *
 *   - When you first get a WalkerActor reference, it comes with a free
 *     reference to the root document's node.
 *   - Given a node, you can ask for children, siblings, and parents.
 *   - You can issue querySelector and querySelectorAll requests to find
 *     other elements.
 *
 * Once you have a NodeFront, you should be able to answer a few questions
 * without further round trips, like the node's name, namespace/tagName,
 * attributes, etc.  Other questions (like a text node's full nodeValue)
 * might require another round trip.
 */

const {Cc, Ci, Cu} = require("chrome");

const protocol = require("devtools/server/protocol");
const {Arg, Option, method, RetVal, types} = protocol;
const {LongStringActor, ShortLongString} = require("devtools/server/actors/string");
const promise = require("sdk/core/promise");
const object = require("sdk/util/object");

Cu.import("resource://gre/modules/Services.jsm");

exports.register = function(handle) {
  handle.addTabActor(InspectorActor, "inspectorActor");
};

exports.unregister = function(handle) {
  handle.removeTabActor(InspectorActor);
};

// XXX: A poor man's makeInfallible until we move it out of transport.js
// Which should be very soon.
function makeInfallible(handler) {
  return function(...args) {
    try {
      return handler.apply(this, args);
    } catch(ex) {
      console.error(ex);
    }
    return undefined;
  }
}

// A resolve that hits the main loop first.
function delayedResolve(value) {
  let deferred = promise.defer();
  Services.tm.mainThread.dispatch(makeInfallible(function delayedResolveHandler() {
    deferred.resolve(value);
  }), 0);
  return deferred.promise;
}

/**
 * We only send nodeValue up to a certain size by default.  This stuff
 * controls that size.
 */
exports.DEFAULT_VALUE_SUMMARY_LENGTH = 50;
var gValueSummaryLength = exports.DEFAULT_VALUE_SUMMARY_LENGTH;

exports.getValueSummaryLength = function() {
  return gValueSummaryLength;
};

exports.setValueSummaryLength = function(val) {
  gValueSummaryLength = val;
};

/**
 * Server side of the node actor.
 */
var NodeActor = protocol.ActorClass({
  typeName: "domnode",

  initialize: function(walker, node) {
    protocol.Actor.prototype.initialize.call(this, null);
    this.walker = walker;
    this.rawNode = node;
  },

  toString: function() {
    return "[NodeActor " + this.actorID + " for " + this.rawNode.toString() + "]";
  },

  /**
   * Instead of storing a connection object, the NodeActor gets its connection
   * from its associated walker.
   */
  get conn() this.walker.conn,

  // Returns the JSON representation of this object over the wire.
  form: function(detail) {
    let form = {
      actor: this.actorID,
      nodeType: this.rawNode.nodeType,
      namespaceURI: this.namespaceURI,
      nodeName: this.rawNode.nodeName,
      numChildren: this.rawNode.childNodes.length,

      // doctype attributes
      name: this.rawNode.name,
      publicId: this.rawNode.publicId,
      systemId: this.rawNode.systemId,

      attrs: this.writeAttrs()
    };

    if (this.rawNode.nodeValue) {
      // We only include a short version of the value if it's longer than
      // gValueSummaryLength
      if (this.rawNode.nodeValue.length > gValueSummaryLength) {
        form.shortValue = this.rawNode.nodeValue.substring(0, gValueSummaryLength);
        form.incompleteValue = true;
      } else {
        form.shortValue = this.rawNode.nodeValue;
      }
    }

    return form;
  },

  writeAttrs: function() {
    if (!this.rawNode.attributes) {
      return undefined;
    }
    return [{namespace: attr.namespace, name: attr.name, value: attr.value }
            for (attr of this.rawNode.attributes)];
  },

  /**
   * Returns a LongStringActor with the node's value.
   */
  getNodeValue: method(function() {
    return new LongStringActor(this.conn, this.rawNode.nodeValue || "");
  }, {
    request: {},
    response: {
      value: RetVal("longstring")
    }
  }),

  /**
   * Set the node's value to a given string.
   */
  setNodeValue: method(function(value) {
    this.rawNode.nodeValue = value;
  }, {
    request: { value: Arg(0) },
    response: {}
  }),
});

/**
 * Client side of the node actor.
 */
let NodeFront = protocol.FrontClass(NodeActor, {
  initialize: function(conn, form) {
    protocol.Front.prototype.initialize.call(this, conn, form);
  },

  // Update the object given a form representation off the wire.
  form: function(form) {
    // Shallow copy of the form.  We could just store a reference, but
    // eventually we'll want to update some of the data.
    this._form = object.merge(form);
  },

  // Some accessors to make NodeFront feel more like an nsIDOMNode

  get id() this.getAttribute("id"),

  get nodeType() this._form.nodeType,
  get namespaceURI() this._form.namespaceURI,
  get nodeName() this._form.nodeName,

  get className() {
    return this.getAttribute("class") || '';
  },

  get hasChildren() this._form.numChildren > 0,
  get numChildren() this._form.numChildren,

  get tagName() this.nodeType === Ci.nsIDOMNode.ELEMENT_NODE ? this.nodeName : null,
  get shortValue() this._form.shortValue,
  get incompleteValue() !!this._form.incompleteValue,

  // doctype properties
  get name() this._form.name,
  get publicId() this._form.publicId,
  get systemId() this._form.systemId,

  getAttribute: function(name) {
    let attr = this._getAttribute(name);
    return attr ? attr.value : null;
  },
  hasAttribute: function(name) {
    this._cacheAttributes();
    return (name in this._attrMap);
  },

  get attributes() this._form.attrs || [],

  getNodeValue: protocol.custom(function() {
    if (!this.incompleteValue) {
      return delayedResolve(new ShortLongString(this.shortValue));
    } else {
      return this._getNodeValue();
    }
  }, {
    impl: "_getNodeValue"
  }),

  _cacheAttributes: function() {
    if (typeof(this._attrMap) != "undefined") {
      return;
    }
    this._attrMap = {};
    for (let attr of this.attributes) {
      this._attrMap[attr.name] = attr;
    }
  },

  _getAttribute: function(name) {
    this._cacheAttributes();
    return this._attrMap[name] || undefined;
  },

  /**
   * Get an nsIDOMNode for the given node front.  This only works locally,
   * and is only intended as a stopgap during the transition to the remote
   * protocol.  If you depend on this you're likely to break soon.
   */
  rawNode: function(rawNode) {
    if (!this.conn._transport._serverConnection) {
      throw new Error("Tried to use rawNode on a remote connection.");
    }
    let actor = this.conn._transport._serverConnection.getActor(this.actorID);
    if (!actor) {
      throw new Error("Could not find client side for actor " + this.actorID);
    }
    return actor.rawNode;
  }
});

/**
 * Server side of a node list as returned by querySelectorAll()
 */
var NodeListActor = exports.NodeListActor = protocol.ActorClass({
  typeName: "domnodelist",

  initialize: function(walker, nodeList) {
    protocol.Actor.prototype.initialize.call(this);
    this.walker = walker;
    this.nodeList = nodeList;
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
  },

  /**
   * Instead of storing a connection object, the NodeActor gets its connection
   * from its associated walker.
   */
  get conn() {
    return this.walker.conn;
  },

  /**
   * Items returned by this actor should belong to the parent walker.
   */
  marshallPool: function() {
    return this.walker;
  },

  // Returns the JSON representation of this object over the wire.
  form: function() {
    return {
      actor: this.actorID,
      length: this.nodeList.length
    }
  },

  /**
   * Get a single node from the node list.
   */
  item: method(function(index) {
    return this.walker._ref(this.nodeList[index]);
  }, {
    request: { item: Arg(0) },
    response: { node: RetVal("domnode") }
  }),

  /**
   * Get a range of the items from the node list.
   */
  items: method(function(start=0, end=this.nodeList.length) {
    return [this.walker._ref(item)
            for (item of Array.prototype.slice.call(this.nodeList, start, end))];
  }, {
    request: {
      start: Arg(0, "number", { optional: true }),
      end: Arg(1, "number", { optional: true })
    },
    response: { nodes: RetVal("array:domnode") }
  }),

  release: method(function() {}, { release: true })
});

/**
 * Client side of a node list as returned by querySelectorAll()
 */
var NodeListFront = exports.NodeLIstFront = protocol.FrontClass(NodeListActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  },

  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },

  marshallPool: function() {
    return this.parent();
  },

  // Update the object given a form representation off the wire.
  form: function(json) {
    this.length = json.length;
  }
});

// Some common request/response templates for the dom walker

let nodeArrayMethod = {
  request: {
    node: Arg(0, "domnode"),
    maxNodes: Option(1),
    center: Option(1, "domnode"),
    start: Option(1, "domnode"),
    whatToShow: Option(1)
  },
  response: RetVal(types.addDictType("domtraversalarray", {
    nodes: "array:domnode"
  }))
};

let traversalMethod = {
  request: {
    node: Arg(0, "domnode"),
    whatToShow: Option(1)
  },
  response: {
    node: RetVal("domnode")
  }
}

/**
 * Server side of the DOM walker.
 */
var WalkerActor = protocol.ActorClass({
  typeName: "domwalker",

  /**
   * Create the WalkerActor
   * @param DebuggerServerConnection conn
   *    The server connection.
   */
  initialize: function(conn, document, options) {
    protocol.Actor.prototype.initialize.call(this, conn);

    this._doc = document;
    this._refMap = new Map();
  },

  // Returns the JSON representation of this object over the wire.
  form: function() {
    return {
      actor: this.actorID,
      root: this.document().form()
    }
  },

  toString: function() {
    return "[WalkerActor " + this.actorID + "]";
  },

  destroy: function() {
    protocol.Actor.prototype.destroy.call(this);
    this._doc = null;
  },

  release: method(function() {}, { release: true }),

  _ref: function(node) {
    let actor = this._refMap.get(node);
    if (actor) return actor;

    actor = new NodeActor(this, node);

    // Add the node actor as a child of this walker actor, assigning
    // it an actorID.
    this.manage(actor);
    this._refMap.set(node, actor);
    return actor;
  },

  /**
   * Return the document node that contains the given node,
   * or the root node if no node is specified.
   * @param NodeActor node
   *        The node whose document is needed, or null to
   *        return the root.
   */
  document: method(function(node) {
    let doc = node ? nodeDocument(node.rawNode) : this._doc;
    return this._ref(doc);
  }, {
    request: { node: Arg(0, "domnode", {optional: true}) },
    response: { node: RetVal("domnode") },
  }),

  /**
   * Return the documentElement for the document containing the
   * given node.
   * @param NodeActor node
   *        The node whose documentElement is requested, or null
   *        to use the root document.
   */
  documentElement: method(function(node) {
    let elt = node ? nodeDocument(node.rawNode).documentElement : this._doc.documentElement;
    return this._ref(elt);
  }, {
    request: { node: Arg(0, "domnode", {optional: true}) },
    response: { node: RetVal("domnode") },
  }),

  /**
   * Return all parents of the given node, ordered from immediate parent
   * to root.
   * @param NodeActor node
   *    The node whose parents are requested.
   * @param object options
   *    Named options, including:
   *    `sameDocument`: If true, parents will be restricted to the same
   *      document as the node.
   */
  parents: method(function(node, options={}) {
    let walker = documentWalker(node.rawNode);
    let parents = [];
    let cur;
    while((cur = walker.parentNode())) {
      if (options.sameDocument && cur.ownerDocument != node.rawNode.ownerDocument) {
        break;
      }
      parents.push(this._ref(cur));
    }
    return parents;
  }, {
    request: {
      node: Arg(0, "domnode"),
      sameDocument: Option(1)
    },
    response: {
      nodes: RetVal("array:domnode")
    },
  }),

  /**
   * Return children of the given node.  By default this method will return
   * all children of the node, but there are options that can restrict this
   * to a more manageable subset.
   *
   * @param NodeActor node
   *    The node whose children you're curious about.
   * @param object options
   *    Named options:
   *    `maxNodes`: The set of nodes returned by the method will be no longer
   *       than maxNodes.
   *    `start`: If a node is specified, the list of nodes will start
   *       with the given child.  Mutally exclusive with `center`.
   *    `center`: If a node is specified, the given node will be as centered
   *       as possible in the list, given how close to the ends of the child
   *       list it is.  Mutually exclusive with `start`.
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   *
   * @returns an object with three items:
   *    hasFirst: true if the first child of the node is included in the list.
   *    hasLast: true if the last child of the node is included in the list.
   *    nodes: Child nodes returned by the request.
   */
  children: method(function(node, options={}) {
    if (options.center && options.start) {
      throw Error("Can't specify both 'center' and 'start' options.");
    }
    let maxNodes = options.maxNodes || -1;
    if (maxNodes == -1) {
      maxNodes = Number.MAX_VALUE;
    }

    // We're going to create a few document walkers with the same filter,
    // make it easier.
    let filteredWalker = function(node) {
      return documentWalker(node, options.whatToShow);
    }

    // Need to know the first and last child.
    let rawNode = node.rawNode;
    let firstChild = filteredWalker(rawNode).firstChild();
    let lastChild = filteredWalker(rawNode).lastChild();

    if (!firstChild) {
      // No children, we're done.
      return { hasFirst: true, hasLast: true, nodes: [] };
    }

    let start;
    if (options.center) {
      start = options.center.rawNode;
    } else if (options.start) {
      start = options.start.rawNode;
    } else {
      start = firstChild;
    }

    let nodes = [];

    // Start by reading backward from the starting point if we're centering...
    let backwardWalker = filteredWalker(start);
    if (start != firstChild && options.center) {
      backwardWalker.previousSibling();
      let backwardCount = Math.floor(maxNodes / 2);
      let backwardNodes = this._readBackward(backwardWalker, backwardCount);
      nodes = backwardNodes;
    }

    // Then read forward by any slack left in the max children...
    let forwardWalker = filteredWalker(start);
    let forwardCount = maxNodes - nodes.length;
    nodes = nodes.concat(this._readForward(forwardWalker, forwardCount));

    // If there's any room left, it means we've run all the way to the end.
    // If we're centering, check if there are more items to read at the front.
    let remaining = maxNodes - nodes.length;
    if (options.center && remaining > 0 && nodes[0].rawNode != firstChild) {
      let firstNodes = this._readBackward(backwardWalker, remaining);

      // Then put it all back together.
      nodes = firstNodes.concat(nodes);
    }

    return {
      hasFirst: nodes[0].rawNode == firstChild,
      hasLast: nodes[nodes.length - 1].rawNode == lastChild,
      nodes: nodes
    };
  }, nodeArrayMethod),

  /**
   * Return siblings of the given node.  By default this method will return
   * all siblings of the node, but there are options that can restrict this
   * to a more manageable subset.
   *
   * If `start` or `center` are not specified, this method will center on the
   * node whose siblings are requested.
   *
   * @param NodeActor node
   *    The node whose children you're curious about.
   * @param object options
   *    Named options:
   *    `maxNodes`: The set of nodes returned by the method will be no longer
   *       than maxNodes.
   *    `start`: If a node is specified, the list of nodes will start
   *       with the given child.  Mutally exclusive with `center`.
   *    `center`: If a node is specified, the given node will be as centered
   *       as possible in the list, given how close to the ends of the child
   *       list it is.  Mutually exclusive with `start`.
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   *
   * @returns an object with three items:
   *    hasFirst: true if the first child of the node is included in the list.
   *    hasLast: true if the last child of the node is included in the list.
   *    nodes: Child nodes returned by the request.
   */
  siblings: method(function(node, options={}) {
    let parentNode = documentWalker(node.rawNode).parentNode();
    if (!parentNode) {
      return {
        hasFirst: true,
        hasLast: true,
        nodes: [node]
      };
    }

    if (!(options.start || options.center)) {
      options.center = node;
    }

    return this.children(this._ref(parentNode), options);
  }, nodeArrayMethod),

  /**
   * Get the next sibling of a given node.  Getting nodes one at a time
   * might be inefficient, be careful.
   *
   * @param object options
   *    Named options:
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   */
  nextSibling: method(function(node, options={}) {
    let walker = documentWalker(node.rawNode, options.whatToShow || Ci.nsIDOMNodeFilter.SHOW_ALL);
    return this._ref(walker.nextSibling());
  }, traversalMethod),

  /**
   * Get the previous sibling of a given node.  Getting nodes one at a time
   * might be inefficient, be careful.
   *
   * @param object options
   *    Named options:
   *    `whatToShow`: A bitmask of node types that should be included.  See
   *       https://developer.mozilla.org/en-US/docs/Web/API/NodeFilter.
   */
  previousSibling: method(function(node, options={}) {
    let walker = documentWalker(node.rawNode, options.whatToShow || Ci.nsIDOMNodeFilter.SHOW_ALL);
    return this._ref(walker.previousSibling());
  }, traversalMethod),

  /**
   * Helper function for the `children` method: Read forward in the sibling
   * list into an array with `count` items, including the current node.
   */
  _readForward: function(walker, count)
  {
    let ret = [];
    let node = walker.currentNode;
    do {
      ret.push(this._ref(node));
      node = walker.nextSibling();
    } while (node && --count);
    return ret;
  },

  /**
   * Helper function for the `children` method: Read backward in the sibling
   * list into an array with `count` items, including the current node.
   */
  _readBackward: function(walker, count)
  {
    let ret = [];
    let node = walker.currentNode;
    do {
      ret.push(this._ref(node));
      node = walker.previousSibling();
    } while(node && --count);
    ret.reverse();
    return ret;
  },

  /**
   * Return the first node in the document that matches the given selector.
   * See https://developer.mozilla.org/en-US/docs/Web/API/Element.querySelector
   *
   * @param NodeActor baseNode
   * @param string selector
   */
  querySelector: method(function(baseNode, selector) {
    let node = baseNode.rawNode.querySelector(selector);
    return node ? this._ref(node) : null;
  }, {
    request: {
      node: Arg(0, "domnode"),
      selector: Arg(1)
    },
    response: {
      node: RetVal("domnode", { optional: true })
    }
  }),

  /**
   * Return a NodeListActor with all nodes that match the given selector.
   * See https://developer.mozilla.org/en-US/docs/Web/API/Element.querySelectorAll
   *
   * @param NodeActor baseNode
   * @param string selector
   */
  querySelectorAll: method(function(baseNode, selector) {
    return new NodeListActor(this, baseNode.rawNode.querySelectorAll(selector));
  }, {
    request: {
      node: Arg(0, "domnode"),
      selector: Arg(1)
    },
    response: {
      list: RetVal("domnodelist")
    }
  })
});

/**
 * Client side of the DOM walker.
 */
var WalkerFront = exports.WalkerFront = protocol.FrontClass(WalkerActor, {
  initialize: function(client, form) {
    protocol.Front.prototype.initialize.call(this, client, form);
  },

  destroy: function() {
    protocol.Front.prototype.destroy.call(this);
  },

  // Update the object given a form representation off the wire.
  form: function(json) {
    this.actorID = json.actorID;
    this.rootNode = types.getType("domnode").read(json.root, this);
  },

  // XXX hack during transition to remote inspector: get a proper NodeFront
  // for a given local node.  Only works locally.
  frontForRawNode: function(rawNode){
    if (!this.conn._transport._serverConnection) {
      throw Error("Tried to use frontForRawNode on a remote connection.");
    }
    let actor = this.conn._transport._serverConnection.getActor(this.actorID);
    if (!actor) {
      throw Error("Could not find client side for actor " + this.actorID);
    }
    let nodeActor = actor._ref(rawNode);

    // Pass the node through a read/write pair to create the client side actor.
    let nodeType = types.getType("domnode");
    return nodeType.read(nodeType.write(nodeActor, actor), this);
  }
});

/**
 * Server side of the inspector actor, which is used to create
 * inspector-related actors, including the walker.
 */
var InspectorActor = protocol.ActorClass({
  typeName: "inspector",
  initialize: function(conn, tabActor) {
    protocol.Actor.prototype.initialize.call(this, conn);
    this.tabActor = tabActor;
    if (tabActor.browser instanceof Ci.nsIDOMWindow) {
      this.window = tabActor.browser;
    } else if (tabActor.browser instanceof Ci.nsIDOMElement) {
      this.window = tabActor.browser.contentWindow;
    }
  },

  getWalker: method(function(options={}) {
    return WalkerActor(this.conn, this.window.document, options);
  }, {
    request: {},
    response: {
      walker: RetVal("domwalker")
    }
  })
});

/**
 * Client side of the inspector actor, which is used to create
 * inspector-related actors, including the walker.
 */
var InspectorFront = exports.InspectorFront = protocol.FrontClass(InspectorActor, {
  initialize: function(client, tabForm) {
    protocol.Front.prototype.initialize.call(this, client);
    this.actorID = tabForm.inspectorActor;

    // XXX: This is the first actor type in its hierarchy to use the protocol
    // library, so we're going to self-own on the client side for now.
    client.addActorPool(this);
    this.manage(this);
  }
});

function documentWalker(node, whatToShow=Ci.nsIDOMNodeFilter.SHOW_ALL) {
  return new DocumentWalker(node, whatToShow, whitespaceTextFilter, false);
}

function nodeDocument(node) {
  return node.ownerDocument || (node.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE ? node : null);
}

/**
 * Similar to a TreeWalker, except will dig in to iframes and it doesn't
 * implement the good methods like previousNode and nextNode.
 *
 * See TreeWalker documentation for explanations of the methods.
 */
function DocumentWalker(aNode, aShow, aFilter, aExpandEntityReferences)
{
  let doc = nodeDocument(aNode);
  this.walker = doc.createTreeWalker(nodeDocument(aNode),
    aShow, aFilter, aExpandEntityReferences);
  this.walker.currentNode = aNode;
  this.filter = aFilter;
}

DocumentWalker.prototype = {
  get node() this.walker.node,
  get whatToShow() this.walker.whatToShow,
  get expandEntityReferences() this.walker.expandEntityReferences,
  get currentNode() this.walker.currentNode,
  set currentNode(aVal) this.walker.currentNode = aVal,

  /**
   * Called when the new node is in a different document than
   * the current node, creates a new treewalker for the document we've
   * run in to.
   */
  _reparentWalker: function DW_reparentWalker(aNewNode) {
    if (!aNewNode) {
      return null;
    }
    let doc = nodeDocument(aNewNode);
    let walker = doc.createTreeWalker(doc,
      this.whatToShow, this.filter, this.expandEntityReferences);
    walker.currentNode = aNewNode;
    this.walker = walker;
    return aNewNode;
  },

  parentNode: function DW_parentNode()
  {
    let currentNode = this.walker.currentNode;
    let parentNode = this.walker.parentNode();

    if (!parentNode) {
      if (currentNode && currentNode.nodeType == Ci.nsIDOMNode.DOCUMENT_NODE
          && currentNode.defaultView) {
        let embeddingFrame = currentNode.defaultView.frameElement;
        if (embeddingFrame) {
          return this._reparentWalker(embeddingFrame);
        }
      }
      return null;
    }

    return parentNode;
  },

  firstChild: function DW_firstChild()
  {
    let node = this.walker.currentNode;
    if (!node)
      return null;
    if (node.contentDocument) {
      return this._reparentWalker(node.contentDocument);
    } else if (node.getSVGDocument) {
      return this._reparentWalker(node.getSVGDocument());
    }
    return this.walker.firstChild();
  },

  lastChild: function DW_lastChild()
  {
    let node = this.walker.currentNode;
    if (!node)
      return null;
    if (node.contentDocument) {
      return this._reparentWalker(node.contentDocument);
    } else if (node.getSVGDocument) {
      return this._reparentWalker(node.getSVGDocument());
    }
    return this.walker.lastChild();
  },

  previousSibling: function DW_previousSibling() this.walker.previousSibling(),
  nextSibling: function DW_nextSibling() this.walker.nextSibling()
}

/**
 * A tree walker filter for avoiding empty whitespace text nodes.
 */
function whitespaceTextFilter(aNode)
{
    if (aNode.nodeType == Ci.nsIDOMNode.TEXT_NODE &&
        !/[^\s]/.exec(aNode.nodeValue)) {
      return Ci.nsIDOMNodeFilter.FILTER_SKIP;
    } else {
      return Ci.nsIDOMNodeFilter.FILTER_ACCEPT;
    }
}

loader.lazyGetter(this, "DOMUtils", function () {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});
