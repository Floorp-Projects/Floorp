/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global XPCNativeWrapper */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("chrome://marionette/content/assert.js");
Cu.import("chrome://marionette/content/atom.js");
const {
  InvalidSelectorError,
  NoSuchElementError,
  StaleElementReferenceError,
} = Cu.import("chrome://marionette/content/error.js", {});
const {pprint} = Cu.import("chrome://marionette/content/format.js", {});
const {PollPromise} = Cu.import("chrome://marionette/content/sync.js", {});

this.EXPORTED_SYMBOLS = ["element"];

const XMLNS = "http://www.w3.org/1999/xhtml";
const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/** XUL elements that support checked property. */
const XUL_CHECKED_ELS = new Set([
  "button",
  "checkbox",
  "listitem",
  "toolbarbutton",
]);

/** XUL elements that support selected property. */
const XUL_SELECTED_ELS = new Set([
  "listitem",
  "menu",
  "menuitem",
  "menuseparator",
  "radio",
  "richlistitem",
  "tab",
]);

const uuidGen = Cc["@mozilla.org/uuid-generator;1"]
    .getService(Ci.nsIUUIDGenerator);

/**
 * This module provides shared functionality for dealing with DOM-
 * and web elements in Marionette.
 *
 * A web element is an abstraction used to identify an element when it
 * is transported across the protocol, between remote- and local ends.
 *
 * Each element has an associated web element reference (a UUID) that
 * uniquely identifies the the element across all browsing contexts. The
 * web element reference for every element representing the same element
 * is the same.
 *
 * The {@link element.Store} provides a mapping between web element
 * references and DOM elements for each browsing context.  It also provides
 * functionality for looking up and retrieving elements.
 *
 * @namespace
 */
this.element = {};

element.Key = "element-6066-11e4-a52e-4f735466cecf";
element.LegacyKey = "ELEMENT";

element.Strategy = {
  ClassName: "class name",
  Selector: "css selector",
  ID: "id",
  Name: "name",
  LinkText: "link text",
  PartialLinkText: "partial link text",
  TagName: "tag name",
  XPath: "xpath",
  Anon: "anon",
  AnonAttribute: "anon attribute",
};

/**
 * Stores known/seen elements and their associated web element
 * references.
 *
 * Elements are added by calling |add(el)| or |addAll(elements)|, and
 * may be queried by their web element reference using |get(element)|.
 *
 * @class
 * @memberof element
 */
element.Store = class {
  constructor() {
    this.els = {};
    this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  }

  clear() {
    this.els = {};
  }

  /**
   * Make a collection of elements seen.
   *
   * The oder of the returned web element references is guaranteed to
   * match that of the collection passed in.
   *
   * @param {NodeList} els
   *     Sequence of elements to add to set of seen elements.
   *
   * @return {Array.<WebElement>}
   *     List of the web element references associated with each element
   *     from |els|.
   */
  addAll(els) {
    let add = this.add.bind(this);
    return [...els].map(add);
  }

  /**
   * Make an element seen.
   *
   * @param {Element} el
   *    Element to add to set of seen elements.
   *
   * @return {string}
   *     Web element reference associated with element.
   */
  add(el) {
    for (let i in this.els) {
      let foundEl;
      try {
        foundEl = this.els[i].get();
      } catch (e) {}

      if (foundEl) {
        if (new XPCNativeWrapper(foundEl) == new XPCNativeWrapper(el)) {
          return i;
        }

      // cleanup reference to gc'd element
      } else {
        delete this.els[i];
      }
    }

    let id = element.generateUUID();
    this.els[id] = Cu.getWeakReference(el);
    return id;
  }

  /**
   * Determine if the provided web element reference has been seen
   * before/is in the element store.
   *
   * Unlike when getting the element, a staleness check is not
   * performed.
   *
   * @param {string} uuid
   *     Element's associated web element reference.
   *
   * @return {boolean}
   *     True if element is in the store, false otherwise.
   */
  has(uuid) {
    return Object.keys(this.els).includes(uuid);
  }

  /**
   * Retrieve a DOM element by its unique web element reference/UUID.
   *
   * Upon retrieving the element, an element staleness check is
   * performed.
   *
   * @param {string} uuid
   *     Web element reference, or UUID.
   * @param {WindowProxy} window
   *     Current browsing context, which may differ from the associate
   *     browsing context of <var>el</var>.
   *
   * @returns {Element}
   *     Element associated with reference.
   *
   * @throws {NoSuchElementError}
   *     If the web element reference <var>uuid</var> has not been
   *     seen before.
   * @throws {StaleElementReferenceError}
   *     If the element has gone stale, indicating it is no longer
   *     attached to the DOM, or its node document is no longer the
   *     active document.
   */
  get(uuid, window) {
    if (!this.has(uuid)) {
      throw new NoSuchElementError(
          "Web element reference not seen before: " + uuid);
    }

    let el;
    let ref = this.els[uuid];
    try {
      el = ref.get();
    } catch (e) {
      delete this.els[uuid];
    }

    if (element.isStale(el, window)) {
      throw new StaleElementReferenceError(
          pprint`The element reference of ${el || uuid} stale; ` +
              "either the element is no longer attached to the DOM, " +
              "it is not in the current frame context, " +
              "or the document has been refreshed");
    }

    return el;
  }
};

/**
 * Find a single element or a collection of elements starting at the
 * document root or a given node.
 *
 * If |timeout| is above 0, an implicit search technique is used.
 * This will wait for the duration of |timeout| for the element
 * to appear in the DOM.
 *
 * See the |element.Strategy| enum for a full list of supported
 * search strategies that can be passed to |strategy|.
 *
 * Available flags for <var>opts</var>:
 *
 * <dl>
 *   <dt><code>all</code>
 *   <dd>
 *     If true, a multi-element search selector is used and a sequence
 *     of elements will be returned.  Otherwise a single element.
 *
 *   <dt><code>timeout</code>
 *   <dd>
 *     Duration to wait before timing out the search.  If <code>all</code>
 *     is false, a {@link NoSuchElementError} is thrown if unable to
 *     find the element within the timeout duration.
 *
 *   <dt><code>startNode</code>
 *   <dd>Element to use as the root of the search.
 *
 * @param {Object.<string, WindowProxy>} container
 *     Window object and an optional shadow root that contains the
 *     root shadow DOM element.
 * @param {string} strategy
 *     Search strategy whereby to locate the element(s).
 * @param {string} selector
 *     Selector search pattern.  The selector must be compatible with
 *     the chosen search <var>strategy</var>.
 * @param {Object.<string, ?>} opts
 *     Options.
 *
 * @return {Promise.<(Element|Array.<Element>)>}
 *     Single element or a sequence of elements.
 *
 * @throws InvalidSelectorError
 *     If <var>strategy</var> is unknown.
 * @throws InvalidSelectorError
 *     If <var>selector</var> is malformed.
 * @throws NoSuchElementError
 *     If a single element is requested, this error will throw if the
 *     element is not found.
 */
element.find = function(container, strategy, selector, opts = {}) {
  let all = !!opts.all;
  let timeout = opts.timeout || 0;
  let startNode = opts.startNode;

  let searchFn;
  if (opts.all) {
    searchFn = findElements.bind(this);
  } else {
    searchFn = findElement.bind(this);
  }

  return new Promise((resolve, reject) => {
    let findElements = new PollPromise((resolve, reject) => {
      let res = find_(container, strategy, selector, searchFn,
          {all, startNode});
      if (res.length > 0) {
        resolve(Array.from(res));
      } else {
        reject([]);
      }
    }, {timeout});

    findElements.then(foundEls => {
      // the following code ought to be moved into findElement
      // and findElements when bug 1254486 is addressed
      if (!opts.all && (!foundEls || foundEls.length == 0)) {
        let msg;
        switch (strategy) {
          case element.Strategy.AnonAttribute:
            msg = "Unable to locate anonymous element: " +
                JSON.stringify(selector);
            break;

          default:
            msg = "Unable to locate element: " + selector;
        }

        reject(new NoSuchElementError(msg));
      }

      if (opts.all) {
        resolve(foundEls);
      }
      resolve(foundEls[0]);
    }, reject);
  });
};

function find_(container, strategy, selector, searchFn,
    {startNode = null, all = false} = {}) {
  let rootNode = container.shadowRoot || container.frame.document;

  if (!startNode) {
    switch (strategy) {
      // For anonymous nodes the start node needs to be of type
      // DOMElement, which will refer to :root in case of a DOMDocument.
      case element.Strategy.Anon:
      case element.Strategy.AnonAttribute:
        if (rootNode instanceof Ci.nsIDOMDocument) {
          startNode = rootNode.documentElement;
        }
        break;

      default:
        startNode = rootNode;
    }
  }

  let res;
  try {
    res = searchFn(strategy, selector, rootNode, startNode);
  } catch (e) {
    throw new InvalidSelectorError(
        `Given ${strategy} expression "${selector}" is invalid: ${e}`);
  }

  if (res) {
    if (all) {
      return res;
    }
    return [res];
  }
  return [];
}

/**
 * Find a single element by XPath expression.
 *
 * @param {DOMElement} root
 *     Document root
 * @param {DOMElement} startNode
 *     Where in the DOM hiearchy to begin searching.
 * @param {string} expr
 *     XPath search expression.
 *
 * @return {DOMElement}
 *     First element matching expression.
 */
element.findByXPath = function(root, startNode, expr) {
  let iter = root.evaluate(expr, startNode, null,
      Ci.nsIDOMXPathResult.FIRST_ORDERED_NODE_TYPE, null);
  return iter.singleNodeValue;
};

/**
 * Find elements by XPath expression.
 *
 * @param {DOMElement} root
 *     Document root.
 * @param {DOMElement} startNode
 *     Where in the DOM hierarchy to begin searching.
 * @param {string} expr
 *     XPath search expression.
 *
 * @return {Array.<DOMElement>}
 *     Sequence of found elements matching expression.
 */
element.findByXPathAll = function(root, startNode, expr) {
  let rv = [];
  let iter = root.evaluate(expr, startNode, null,
      Ci.nsIDOMXPathResult.ORDERED_NODE_ITERATOR_TYPE, null);
  let el = iter.iterateNext();
  while (el) {
    rv.push(el);
    el = iter.iterateNext();
  }
  return rv;
};

/**
 * Find all hyperlinks descendant of <var>node</var> which link text
 * is <var>s</var>.
 *
 * @param {DOMElement} node
 *     Where in the DOM hierarchy to begin searching.
 * @param {string} s
 *     Link text to search for.
 *
 * @return {Array.<DOMAnchorElement>}
 *     Sequence of link elements which text is <var>s</var>.
 */
element.findByLinkText = function(node, s) {
  return filterLinks(node, link => link.text.trim() === s);
};

/**
 * Find all hyperlinks descendant of |node| which link text contains |s|.
 *
 * @param {DOMElement} node
 *     Where in the DOM hierachy to begin searching.
 * @param {string} s
 *     Link text to search for.
 *
 * @return {Array.<DOMAnchorElement>}
 *     Sequence of link elements which text containins |s|.
 */
element.findByPartialLinkText = function(node, s) {
  return filterLinks(node, link => link.text.indexOf(s) != -1);
};

/**
 * Filters all hyperlinks that are descendant of |node| by |predicate|.
 *
 * @param {DOMElement} node
 *     Where in the DOM hierarchy to begin searching.
 * @param {function(DOMAnchorElement): boolean} predicate
 *     Function that determines if given link should be included in
 *     return value or filtered away.
 *
 * @return {Array.<DOMAnchorElement>}
 *     Sequence of link elements matching |predicate|.
 */
function filterLinks(node, predicate) {
  let rv = [];
  for (let link of node.getElementsByTagName("a")) {
    if (predicate(link)) {
      rv.push(link);
    }
  }
  return rv;
}

/**
 * Finds a single element.
 *
 * @param {element.Strategy} using
 *     Selector strategy to use.
 * @param {string} value
 *     Selector expression.
 * @param {DOMElement} rootNode
 *     Document root.
 * @param {DOMElement=} startNode
 *     Optional node from which to start searching.
 *
 * @return {DOMElement}
 *     Found elements.
 *
 * @throws {InvalidSelectorError}
 *     If strategy |using| is not recognised.
 * @throws {Error}
 *     If selector expression |value| is malformed.
 */
function findElement(using, value, rootNode, startNode) {
  switch (using) {
    case element.Strategy.ID:
      {
        if (startNode.getElementById) {
          return startNode.getElementById(value);
        }
        let expr = `.//*[@id="${value}"]`;
        return element.findByXPath( rootNode, startNode, expr);
      }

    case element.Strategy.Name:
      {
        if (startNode.getElementsByName) {
          return startNode.getElementsByName(value)[0];
        }
        let expr = `.//*[@name="${value}"]`;
        return element.findByXPath(rootNode, startNode, expr);
      }

    case element.Strategy.ClassName:
      // works for >= Firefox 3
      return startNode.getElementsByClassName(value)[0];

    case element.Strategy.TagName:
      // works for all elements
      return startNode.getElementsByTagName(value)[0];

    case element.Strategy.XPath:
      return element.findByXPath(rootNode, startNode, value);

    case element.Strategy.LinkText:
      for (let link of startNode.getElementsByTagName("a")) {
        if (link.text.trim() === value) {
          return link;
        }
      }
      return undefined;

    case element.Strategy.PartialLinkText:
      for (let link of startNode.getElementsByTagName("a")) {
        if (link.text.indexOf(value) != -1) {
          return link;
        }
      }
      return undefined;

    case element.Strategy.Selector:
      try {
        return startNode.querySelector(value);
      } catch (e) {
        throw new InvalidSelectorError(`${e.message}: "${value}"`);
      }

    case element.Strategy.Anon:
      return rootNode.getAnonymousNodes(startNode);

    case element.Strategy.AnonAttribute:
      let attr = Object.keys(value)[0];
      return rootNode.getAnonymousElementByAttribute(
          startNode, attr, value[attr]);
  }

  throw new InvalidSelectorError(`No such strategy: ${using}`);
}

/**
 * Find multiple elements.
 *
 * @param {element.Strategy} using
 *     Selector strategy to use.
 * @param {string} value
 *     Selector expression.
 * @param {DOMElement} rootNode
 *     Document root.
 * @param {DOMElement=} startNode
 *     Optional node from which to start searching.
 *
 * @return {DOMElement}
 *     Found elements.
 *
 * @throws {InvalidSelectorError}
 *     If strategy |using| is not recognised.
 * @throws {Error}
 *     If selector expression |value| is malformed.
 */
function findElements(using, value, rootNode, startNode) {
  switch (using) {
    case element.Strategy.ID:
      value = `.//*[@id="${value}"]`;

    // fall through
    case element.Strategy.XPath:
      return element.findByXPathAll(rootNode, startNode, value);

    case element.Strategy.Name:
      if (startNode.getElementsByName) {
        return startNode.getElementsByName(value);
      }
      return element.findByXPathAll(
          rootNode, startNode, `.//*[@name="${value}"]`);

    case element.Strategy.ClassName:
      return startNode.getElementsByClassName(value);

    case element.Strategy.TagName:
      return startNode.getElementsByTagName(value);

    case element.Strategy.LinkText:
      return element.findByLinkText(startNode, value);

    case element.Strategy.PartialLinkText:
      return element.findByPartialLinkText(startNode, value);

    case element.Strategy.Selector:
      return startNode.querySelectorAll(value);

    case element.Strategy.Anon:
      return rootNode.getAnonymousNodes(startNode);

    case element.Strategy.AnonAttribute:
      let attr = Object.keys(value)[0];
      let el = rootNode.getAnonymousElementByAttribute(
          startNode, attr, value[attr]);
      if (el) {
        return [el];
      }
      return [];

    default:
      throw new InvalidSelectorError(`No such strategy: ${using}`);
  }
}

/**
 * Determines if <var>obj<var> is an HTML or JS collection.
 *
 * @param {*} seq
 *     Type to determine.
 *
 * @return {boolean}
 *     True if <var>seq</va> is collection.
 */
element.isCollection = function(seq) {
  switch (Object.prototype.toString.call(seq)) {
    case "[object Arguments]":
    case "[object Array]":
    case "[object FileList]":
    case "[object HTMLAllCollection]":
    case "[object HTMLCollection]":
    case "[object HTMLFormControlsCollection]":
    case "[object HTMLOptionsCollection]":
    case "[object NodeList]":
      return true;

    default:
      return false;
  }
};

element.makeWebElement = function(uuid) {
  return {
    [element.Key]: uuid,
    [element.LegacyKey]: uuid,
  };
};

/**
 * Checks if |ref| has either |element.Key| or |element.LegacyKey|
 * as properties.
 *
 * @param {Object.<string, string>} ref
 *     Object that represents a web element reference.
 * @return {boolean}
 *     True if |ref| has either expected property.
 */
element.isWebElementReference = function(ref) {
  let properties = Object.getOwnPropertyNames(ref);
  return properties.includes(element.Key) ||
      properties.includes(element.LegacyKey);
};

element.generateUUID = function() {
  let uuid = uuidGen.generateUUID().toString();
  return uuid.substring(1, uuid.length - 1);
};

/**
 * Determines if <var>el</var> is stale.
 *
 * A stale element is an element no longer attached to the DOM or which
 * node document is not the active document of the current browsing
 * context.
 *
 * The currently selected browsing context, specified through
 * <var>window<var>, is a WebDriver concept defining the target
 * against which commands will run.  As the current browsing context
 * may differ from <var>el</var>'s associated context, an element is
 * considered stale even if it is connected to a living (not discarded)
 * browsing context such as an <tt>&lt;iframe&gt;</tt>.
 *
 * @param {Element=} el
 *     DOM element to check for staleness.  If null, which may be
 *     the case if the element has been unwrapped from a weak
 *     reference, it is always considered stale.
 * @param {WindowProxy=} window
 *     Current browsing context, which may differ from the associate
 *     browsing context of <var>el</var>.  When retrieving XUL
 *     elements, this is optional.
 *
 * @return {boolean}
 *     True if <var>el</var> is stale, false otherwise.
 */
element.isStale = function(el, window = undefined) {
  if (typeof window == "undefined") {
    window = el.ownerGlobal;
  }

  if (el === null ||
      !el.ownerGlobal ||
      el.ownerDocument !== window.document) {
    return true;
  }

  return !el.isConnected;
};

/**
 * Determine if <var>el</var> is selected or not.
 *
 * This operation only makes sense on
 * <tt>&lt;input type=checkbox&gt;</tt>,
 * <tt>&lt;input type=radio&gt;</tt>,
 * and <tt>&gt;option&gt;</tt> elements.
 *
 * @param {(DOMElement|XULElement)} el
 *     Element to test if selected.
 *
 * @return {boolean}
 *     True if element is selected, false otherwise.
 */
element.isSelected = function(el) {
  if (!el) {
    return false;
  }

  if (element.isXULElement(el)) {
    if (XUL_CHECKED_ELS.has(el.tagName)) {
      return el.checked;
    } else if (XUL_SELECTED_ELS.has(el.tagName)) {
      return el.selected;
    }

  // TODO(ato): Use element.isDOMElement when bug 1400256 lands
  } else if (typeof el == "object" &&
      "nodeType" in el &&
      el.nodeType == el.ELEMENT_NODE) {
    if (el.localName == "input" && ["checkbox", "radio"].includes(el.type)) {
      return el.checked;
    } else if (el.localName == "option") {
      return el.selected;
    }
  }

  return false;
};

/**
 * This function generates a pair of coordinates relative to the viewport
 * given a target element and coordinates relative to that element's
 * top-left corner.
 *
 * @param {Node} node
 *     Target node.
 * @param {number=} xOffset
 *     Horizontal offset relative to target's top-left corner.
 *     Defaults to the centre of the target's bounding box.
 * @param {number=} yOffset
 *     Vertical offset relative to target's top-left corner.  Defaults to
 *     the centre of the target's bounding box.
 *
 * @return {Object.<string, number>}
 *     X- and Y coordinates.
 *
 * @throws TypeError
 *     If |xOffset| or |yOffset| are not numbers.
 */
element.coordinates = function(
    node, xOffset = undefined, yOffset = undefined) {

  let box = node.getBoundingClientRect();

  if (typeof xOffset == "undefined" || xOffset === null) {
    xOffset = box.width / 2.0;
  }
  if (typeof yOffset == "undefined" || yOffset === null) {
    yOffset = box.height / 2.0;
  }

  if (typeof yOffset != "number" || typeof xOffset != "number") {
    throw new TypeError("Offset must be a number");
  }

  return {
    x: box.left + xOffset,
    y: box.top + yOffset,
  };
};

/**
 * This function returns true if the node is in the viewport.
 *
 * @param {Element} el
 *     Target element.
 * @param {number=} x
 *     Horizontal offset relative to target.  Defaults to the centre of
 *     the target's bounding box.
 * @param {number=} y
 *     Vertical offset relative to target.  Defaults to the centre of
 *     the target's bounding box.
 *
 * @return {boolean}
 *     True if if |el| is in viewport, false otherwise.
 */
element.inViewport = function(el, x = undefined, y = undefined) {
  let win = el.ownerGlobal;
  let c = element.coordinates(el, x, y);
  let vp = {
    top: win.pageYOffset,
    left: win.pageXOffset,
    bottom: (win.pageYOffset + win.innerHeight),
    right: (win.pageXOffset + win.innerWidth),
  };

  return (vp.left <= c.x + win.pageXOffset &&
      c.x + win.pageXOffset <= vp.right &&
      vp.top <= c.y + win.pageYOffset &&
      c.y + win.pageYOffset <= vp.bottom);
};

/**
 * Gets the element's container element.
 *
 * An element container is defined by the WebDriver
 * specification to be an <tt>&lt;option&gt;</tt> element in a
 * <a href="https://html.spec.whatwg.org/#concept-element-contexts">valid
 * element context</a>, meaning that it has an ancestral element
 * that is either <tt>&lt;datalist&gt;</tt> or <tt>&lt;select&gt;</tt>.
 *
 * If the element does not have a valid context, its container element
 * is itself.
 *
 * @param {Element} el
 *     Element to get the container of.
 *
 * @return {Element}
 *     Container element of |el|.
 */
element.getContainer = function(el) {
  if (el.localName != "option") {
    return el;
  }

  function validContext(ctx) {
    return ctx.localName == "datalist" || ctx.localName == "select";
  }

  // does <option> have a valid context,
  // meaning is it a child of <datalist> or <select>?
  let parent = el;
  while (parent.parentNode && !validContext(parent)) {
    parent = parent.parentNode;
  }

  if (!validContext(parent)) {
    return el;
  }
  return parent;
};

/**
 * An element is in view if it is a member of its own pointer-interactable
 * paint tree.
 *
 * This means an element is considered to be in view, but not necessarily
 * pointer-interactable, if it is found somewhere in the
 * <code>elementsFromPoint</code> list at <var>el</var>'s in-view
 * centre coordinates.
 *
 * Before running the check, we change <var>el</var>'s pointerEvents
 * style property to "auto", since elements without pointer events
 * enabled do not turn up in the paint tree we get from
 * document.elementsFromPoint.  This is a specialisation that is only
 * relevant when checking if the element is in view.
 *
 * @param {Element} el
 *     Element to check if is in view.
 *
 * @return {boolean}
 *     True if <var>el</var> is inside the viewport, or false otherwise.
 */
element.isInView = function(el) {
  let originalPointerEvents = el.style.pointerEvents;
  try {
    el.style.pointerEvents = "auto";
    const tree = element.getPointerInteractablePaintTree(el);
    return tree.includes(el);
  } finally {
    el.style.pointerEvents = originalPointerEvents;
  }
};

/**
 * This function throws the visibility of the element error if the element is
 * not displayed or the given coordinates are not within the viewport.
 *
 * @param {Element} el
 *     Element to check if visible.
 * @param {number=} x
 *     Horizontal offset relative to target.  Defaults to the centre of
 *     the target's bounding box.
 * @param {number=} y
 *     Vertical offset relative to target.  Defaults to the centre of
 *     the target's bounding box.
 *
 * @return {boolean}
 *     True if visible, false otherwise.
 */
element.isVisible = function(el, x = undefined, y = undefined) {
  let win = el.ownerGlobal;

  // Bug 1094246: webdriver's isShown doesn't work with content xul
  if (!element.isXULElement(el) && !atom.isElementDisplayed(el, win)) {
    return false;
  }

  if (el.tagName.toLowerCase() == "body") {
    return true;
  }

  if (!element.inViewport(el, x, y)) {
    element.scrollIntoView(el);
    if (!element.inViewport(el)) {
      return false;
    }
  }
  return true;
};

/**
 * A pointer-interactable element is defined to be the first
 * non-transparent element, defined by the paint order found at the centre
 * point of its rectangle that is inside the viewport, excluding the size
 * of any rendered scrollbars.
 *
 * An element is obscured if the pointer-interactable paint tree at its
 * centre point is empty, or the first element in this tree is not an
 * inclusive descendant of itself.
 *
 * @param {DOMElement} el
 *     Element determine if is pointer-interactable.
 *
 * @return {boolean}
 *     True if element is obscured, false otherwise.
 */
element.isObscured = function(el) {
  let tree = element.getPointerInteractablePaintTree(el);
  return !el.contains(tree[0]);
};

// TODO(ato): Only used by deprecated action API
// https://bugzil.la/1354578
/**
 * Calculate the in-view centre point of the area of the given DOM client
 * rectangle that is inside the viewport.
 *
 * @param {DOMRect} rect
 *     Element off a DOMRect sequence produced by calling
 *     <code>getClientRects</code> on an {@link Element}.
 * @param {WindowProxy} window
 *     Current window global.
 *
 * @return {Map.<string, number>}
 *     X and Y coordinates that denotes the in-view centre point of
 *     <var>rect</var>.
 */
element.getInViewCentrePoint = function(rect, window) {
  const {max, min} = Math;

  let x = {
    left: max(0, min(rect.x, rect.x + rect.width)),
    right: min(window.innerWidth, max(rect.x, rect.x + rect.width)),
  };
  let y = {
    top: max(0, min(rect.y, rect.y + rect.height)),
    bottom: min(window.innerHeight, max(rect.y, rect.y + rect.height)),
  };

  return {
    x: (x.left + x.right) / 2,
    y: (y.top + y.bottom) / 2,
  };
};

/**
 * Produces a pointer-interactable elements tree from a given element.
 *
 * The tree is defined by the paint order found at the centre point of
 * the element's rectangle that is inside the viewport, excluding the size
 * of any rendered scrollbars.
 *
 * @param {DOMElement} el
 *     Element to determine if is pointer-interactable.
 *
 * @return {Array.<DOMElement>}
 *     Sequence of elements in paint order.
 */
element.getPointerInteractablePaintTree = function(el) {
  const doc = el.ownerDocument;
  const win = doc.defaultView;
  const container = {frame: win};
  const rootNode = el.getRootNode();

  // Include shadow DOM host only if the element's root node is not the
  // owner document.
  if (rootNode !== doc) {
    container.shadowRoot = rootNode;
  }

  // pointer-interactable elements tree, step 1
  if (!el.isConnected) {
    return [];
  }

  // steps 2-3
  let rects = el.getClientRects();
  if (rects.length == 0) {
    return [];
  }

  // step 4
  let centre = element.getInViewCentrePoint(rects[0], win);

  // step 5
  return doc.elementsFromPoint(centre.x, centre.y);
};

// TODO(ato): Not implemented.
// In fact, it's not defined in the spec.
element.isKeyboardInteractable = () => true;

/**
 * Attempts to scroll into view |el|.
 *
 * @param {DOMElement} el
 *     Element to scroll into view.
 */
element.scrollIntoView = function(el) {
  if (el.scrollIntoView) {
    el.scrollIntoView({block: "end", inline: "nearest", behavior: "instant"});
  }
};

element.isXULElement = function(el) {
  return el.namespaceURI === XULNS;
};

const boolEls = {
  audio: ["autoplay", "controls", "loop", "muted"],
  button: ["autofocus", "disabled", "formnovalidate"],
  details: ["open"],
  dialog: ["open"],
  fieldset: ["disabled"],
  form: ["novalidate"],
  iframe: ["allowfullscreen"],
  img: ["ismap"],
  input: [
    "autofocus",
    "checked",
    "disabled",
    "formnovalidate",
    "multiple",
    "readonly",
    "required",
  ],
  keygen: ["autofocus", "disabled"],
  menuitem: ["checked", "default", "disabled"],
  object: ["typemustmatch"],
  ol: ["reversed"],
  optgroup: ["disabled"],
  option: ["disabled", "selected"],
  script: ["async", "defer"],
  select: ["autofocus", "disabled", "multiple", "required"],
  textarea: ["autofocus", "disabled", "readonly", "required"],
  track: ["default"],
  video: ["autoplay", "controls", "loop", "muted"],
};

/**
 * Tests if the attribute is a boolean attribute on element.
 *
 * @param {DOMElement} el
 *     Element to test if <var>attr</var> is a boolean attribute on.
 * @param {string} attr
 *     Attribute to test is a boolean attribute.
 *
 * @return {boolean}
 *     True if the attribute is boolean, false otherwise.
 */
element.isBooleanAttribute = function(el, attr) {
  if (el.namespaceURI !== XMLNS) {
    return false;
  }

  // global boolean attributes that apply to all HTML elements,
  // except for custom elements
  const customElement = !el.localName.includes("-");
  if ((attr == "hidden" || attr == "itemscope") && customElement) {
    return true;
  }

  if (!boolEls.hasOwnProperty(el.localName)) {
    return false;
  }
  return boolEls[el.localName].includes(attr);
};
