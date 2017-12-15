/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";
/* global XPCNativeWrapper */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("chrome://marionette/content/assert.js");
Cu.import("chrome://marionette/content/atom.js");
const {
  InvalidArgumentError,
  InvalidSelectorError,
  NoSuchElementError,
  StaleElementReferenceError,
} = Cu.import("chrome://marionette/content/error.js", {});
const {pprint} = Cu.import("chrome://marionette/content/format.js", {});
const {PollPromise} = Cu.import("chrome://marionette/content/sync.js", {});

this.EXPORTED_SYMBOLS = [
  "ChromeWebElement",
  "ContentWebElement",
  "ContentWebFrame",
  "ContentWebWindow",
  "element",
  "WebElement",
];

const {
  FIRST_ORDERED_NODE_TYPE,
  ORDERED_NODE_ITERATOR_TYPE,
} = Ci.nsIDOMXPathResult;
const ELEMENT_NODE = 1;
const DOCUMENT_NODE = 9;

const XBLNS = "http://www.mozilla.org/xbl";
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
 * Elements are added by calling {@link #add()} or {@link addAll()},
 * and may be queried by their web element reference using {@link get()}.
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
   *     from <var>els</var>.
   */
  addAll(els) {
    let add = this.add.bind(this);
    return [...els].map(add);
  }

  /**
   * Make an element seen.
   *
   * @param {(Element|WindowProxy|XULElement)} el
   *    Element to add to set of seen elements.
   *
   * @return {WebElement}
   *     Web element reference associated with element.
   *
   * @throws {TypeError}
   *     If <var>el</var> is not an {@link Element} or a {@link XULElement}.
   */
  add(el) {
    const isDOMElement = element.isDOMElement(el);
    const isDOMWindow = element.isDOMWindow(el);
    const isXULElement = element.isXULElement(el);
    const context = isXULElement ? "chrome" : "content";

    if (!(isDOMElement || isDOMWindow || isXULElement)) {
      throw new TypeError(
          "Expected an element or WindowProxy, " +
          pprint`got: ${el}`);
    }

    for (let i in this.els) {
      let foundEl;
      try {
        foundEl = this.els[i].get();
      } catch (e) {}

      if (foundEl) {
        if (new XPCNativeWrapper(foundEl) == new XPCNativeWrapper(el)) {
          return WebElement.fromUUID(i, context);
        }

      // cleanup reference to gc'd element
      } else {
        delete this.els[i];
      }
    }

    let webEl = WebElement.from(el);
    this.els[webEl.uuid] = Cu.getWeakReference(el);
    return webEl;
  }

  /**
   * Determine if the provided web element reference has been seen
   * before/is in the element store.
   *
   * Unlike when getting the element, a staleness check is not
   * performed.
   *
   * @param {WebElement} webEl
   *     Element's associated web element reference.
   *
   * @return {boolean}
   *     True if element is in the store, false otherwise.
   *
   * @throws {TypeError}
   *     If <var>webEl</var> is not a {@link WebElement}.
   */
  has(webEl) {
    if (!(webEl instanceof WebElement)) {
      throw new TypeError(
          pprint`Expected web element, got: ${webEl}`);
    }
    return Object.keys(this.els).includes(webEl.uuid);
  }

  /**
   * Retrieve a DOM {@link Element} or a {@link XULElement} by its
   * unique {@link WebElement} reference.
   *
   * @param {WebElement} webEl
   *     Web element reference to find the associated {@link Element}
   *     of.
   * @param {WindowProxy} window
   *     Current browsing context, which may differ from the associate
   *     browsing context of <var>el</var>.
   *
   * @returns {(Element|XULElement)}
   *     Element associated with reference.
   *
   * @throws {TypeError}
   *     If <var>webEl</var> is not a {@link WebElement}.
   * @throws {NoSuchElementError}
   *     If the web element reference <var>uuid</var> has not been
   *     seen before.
   * @throws {StaleElementReferenceError}
   *     If the element has gone stale, indicating it is no longer
   *     attached to the DOM, or its node document is no longer the
   *     active document.
   */
  get(webEl, window) {
    if (!(webEl instanceof WebElement)) {
      throw new TypeError(
          pprint`Expected web element, got: ${webEl}`);
    }
    if (!this.has(webEl)) {
      throw new NoSuchElementError(
          "Web element reference not seen before: " + webEl.uuid);
    }

    let el;
    let ref = this.els[webEl.uuid];
    try {
      el = ref.get();
    } catch (e) {
      delete this.els[webEl.uuid];
    }

    if (element.isStale(el, window)) {
      throw new StaleElementReferenceError(
          pprint`The element reference of ${el || webEl.uuid} is stale; ` +
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
 * This will wait for the duration of <var>timeout</var> for the
 * element to appear in the DOM.
 *
 * See the {@link element.Strategy} enum for a full list of supported
 * search strategies that can be passed to <var>strategy</var>.
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
 * @param {HTMLDocument} document
 *     Document root.
 * @param {Element} startNode
 *     Where in the DOM hiearchy to begin searching.
 * @param {string} expression
 *     XPath search expression.
 *
 * @return {Node}
 *     First element matching <var>expression</var>.
 */
element.findByXPath = function(document, startNode, expression) {
  let iter = document.evaluate(
      expression, startNode, null, FIRST_ORDERED_NODE_TYPE, null);
  return iter.singleNodeValue;
};

/**
 * Find elements by XPath expression.
 *
 * @param {HTMLDocument} document
 *     Document root.
 * @param {Element} startNode
 *     Where in the DOM hierarchy to begin searching.
 * @param {string} expression
 *     XPath search expression.
 *
 * @return {Iterable.<Node>}
 *     Iterator over elements matching <var>expression</var>.
 */
element.findByXPathAll = function* (document, startNode, expression) {
  let iter = document.evaluate(
      expression, startNode, null, ORDERED_NODE_ITERATOR_TYPE, null);
  let el = iter.iterateNext();
  while (el) {
    yield el;
    el = iter.iterateNext();
  }
};

/**
 * Find all hyperlinks descendant of <var>startNode</var> which
 * link text is <var>linkText</var>.
 *
 * @param {Element} startNode
 *     Where in the DOM hierarchy to begin searching.
 * @param {string} linkText
 *     Link text to search for.
 *
 * @return {Iterable.<HTMLAnchorElement>}
 *     Sequence of link elements which text is <var>s</var>.
 */
element.findByLinkText = function(startNode, linkText) {
  return filterLinks(startNode, link => link.text.trim() === linkText);
};

/**
 * Find all hyperlinks descendant of <var>startNode</var> which
 * link text contains <var>linkText</var>.
 *
 * @param {Element} startNode
 *     Where in the DOM hierachy to begin searching.
 * @param {string} linkText
 *     Link text to search for.
 *
 * @return {Iterable.<HTMLAnchorElement>}
 *     Iterator of link elements which text containins
 *     <var>linkText</var>.
 */
element.findByPartialLinkText = function(startNode, linkText) {
  return filterLinks(startNode, link => link.text.includes(linkText));
};

/**
 * Find anonymous nodes of <var>node</var>.
 *
 * @param {XULDocument} document
 *     Root node of the document.
 * @param {XULElement} node
 *     Where in the DOM hierarchy to begin searching.
 *
 * @return {Iterable.<XULElement>}
 *     Iterator over anonymous elements.
 */
element.findAnonymousNodes = function* (document, node) {
  let anons = document.getAnonymousNodes(node) || [];
  for (let node of anons) {
    yield node;
  }
};

/**
 * Filters all hyperlinks that are descendant of <var>startNode</var>
 * by <var>predicate</var>.
 *
 * @param {Element} startNode
 *     Where in the DOM hierarchy to begin searching.
 * @param {function(HTMLAnchorElement): boolean} predicate
 *     Function that determines if given link should be included in
 *     return value or filtered away.
 *
 * @return {Iterable.<HTMLAnchorElement>}
 *     Iterator of link elements matching <var>predicate</var>.
 */
function* filterLinks(startNode, predicate) {
  for (let link of startNode.getElementsByTagName("a")) {
    if (predicate(link)) {
      yield link;
    }
  }
}

/**
 * Finds a single element.
 *
 * @param {element.Strategy} strategy
 *     Selector strategy to use.
 * @param {string} selector
 *     Selector expression.
 * @param {HTMLDocument} document
 *     Document root.
 * @param {Element=} startNode
 *     Optional node from which to start searching.
 *
 * @return {Element}
 *     Found elements.
 *
 * @throws {InvalidSelectorError}
 *     If strategy <var>using</var> is not recognised.
 * @throws {Error}
 *     If selector expression <var>selector</var> is malformed.
 */
function findElement(strategy, selector, document, startNode = undefined) {
  switch (strategy) {
    case element.Strategy.ID:
      {
        if (startNode.getElementById) {
          return startNode.getElementById(selector);
        }
        let expr = `.//*[@id="${selector}"]`;
        return element.findByXPath(document, startNode, expr);
      }

    case element.Strategy.Name:
      {
        if (startNode.getElementsByName) {
          return startNode.getElementsByName(selector)[0];
        }
        let expr = `.//*[@name="${selector}"]`;
        return element.findByXPath(document, startNode, expr);
      }

    case element.Strategy.ClassName:
      return startNode.getElementsByClassName(selector)[0];

    case element.Strategy.TagName:
      return startNode.getElementsByTagName(selector)[0];

    case element.Strategy.XPath:
      return element.findByXPath(document, startNode, selector);

    case element.Strategy.LinkText:
      for (let link of startNode.getElementsByTagName("a")) {
        if (link.text.trim() === selector) {
          return link;
        }
      }
      return undefined;

    case element.Strategy.PartialLinkText:
      for (let link of startNode.getElementsByTagName("a")) {
        if (link.text.includes(selector)) {
          return link;
        }
      }
      return undefined;

    case element.Strategy.Selector:
      try {
        return startNode.querySelector(selector);
      } catch (e) {
        throw new InvalidSelectorError(`${e.message}: "${selector}"`);
      }

    case element.Strategy.Anon:
      return element.findAnonymousNodes(document, startNode).next().value;

    case element.Strategy.AnonAttribute:
      let attr = Object.keys(selector)[0];
      return document.getAnonymousElementByAttribute(
          startNode, attr, selector[attr]);
  }

  throw new InvalidSelectorError(`No such strategy: ${strategy}`);
}

/**
 * Find multiple elements.
 *
 * @param {element.Strategy} strategy
 *     Selector strategy to use.
 * @param {string} selector
 *     Selector expression.
 * @param {HTMLDocument} document
 *     Document root.
 * @param {Element=} startNode
 *     Optional node from which to start searching.
 *
 * @return {Array.<Element>}
 *     Found elements.
 *
 * @throws {InvalidSelectorError}
 *     If strategy <var>strategy</var> is not recognised.
 * @throws {Error}
 *     If selector expression <var>selector</var> is malformed.
 */
function findElements(strategy, selector, document, startNode = undefined) {
  switch (strategy) {
    case element.Strategy.ID:
      selector = `.//*[@id="${selector}"]`;

    // fall through
    case element.Strategy.XPath:
      return [...element.findByXPathAll(document, startNode, selector)];

    case element.Strategy.Name:
      if (startNode.getElementsByName) {
        return startNode.getElementsByName(selector);
      }
      return [...element.findByXPathAll(
          document, startNode, `.//*[@name="${selector}"]`)];

    case element.Strategy.ClassName:
      return startNode.getElementsByClassName(selector);

    case element.Strategy.TagName:
      return startNode.getElementsByTagName(selector);

    case element.Strategy.LinkText:
      return [...element.findByLinkText(startNode, selector)];

    case element.Strategy.PartialLinkText:
      return [...element.findByPartialLinkText(startNode, selector)];

    case element.Strategy.Selector:
      return startNode.querySelectorAll(selector);

    case element.Strategy.Anon:
      return [...element.findAnonymousNodes(document, startNode)];

    case element.Strategy.AnonAttribute:
      let attr = Object.keys(selector)[0];
      let el = document.getAnonymousElementByAttribute(
          startNode, attr, selector[attr]);
      if (el) {
        return [el];
      }
      return [];

    default:
      throw new InvalidSelectorError(`No such strategy: ${strategy}`);
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

  } else if (element.isDOMElement(el)) {
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
 *     If <var>xOffset</var> or <var>yOffset</var> are not numbers.
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
 *     True if if <var>el</var> is in viewport, false otherwise.
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

  function findAncestralElement(startNode, validAncestors) {
    let node = startNode;
    while (node.parentNode) {
      node = node.parentNode;
      if (validAncestors.includes(node.localName)) {
        return node;
      }
    }

    return startNode;
  }

  // Does <option> have a valid context,
  // meaning is it a child of <datalist> or <select>?
  if (el.localName === "option") {
    return findAncestralElement(el, ["datalist", "select"]);
  }

  // Child nodes of button will not be part of the element tree for
  // elementsFromPoint until bug 1089326 is fixed.
  return findAncestralElement(el, ["button"]);
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

    // Bug 1413493 - <tr> is not part of the returned paint tree yet. As
    // workaround check the visibility based on the first contained cell.
    if (el.localName === "tr" && el.cells && el.cells.length > 0) {
      return tree.includes(el.cells[0]);
    }

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

  if (!atom.isElementDisplayed(el, win)) {
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

/**
 * Ascertains whether <var>node</var> is a DOM-, SVG-, or XUL element.
 *
 * @param {*} node
 *     Element thought to be an <code>Element</code> or
 *     <code>XULElement</code>.
 *
 * @return {boolean}
 *     True if <var>node</var> is an element, false otherwise.
 */
element.isElement = function(node) {
  return element.isDOMElement(node) || element.isXULElement(node);
};

/**
 * Ascertains whether <var>node</var> is a DOM element.
 *
 * @param {*} node
 *     Element thought to be an <code>Element</code>.
 *
 * @return {boolean}
 *     True if <var>node</var> is a DOM element, false otherwise.
 */
element.isDOMElement = function(node) {
  return typeof node == "object" &&
      node !== null &&
      "nodeType" in node &&
      [ELEMENT_NODE, DOCUMENT_NODE].includes(node.nodeType) &&
      !element.isXULElement(node);
};

/**
 * Ascertains whether <var>el</var> is a XUL- or XBL element.
 *
 * @param {*} node
 *     Element thought to be a XUL- or XBL element.
 *
 * @return {boolean}
 *     True if <var>node</var> is a XULElement or XBLElement,
 *     false otherwise.
 */
element.isXULElement = function(node) {
  return typeof node == "object" &&
      node !== null &&
      "nodeType" in node &&
      node.nodeType === node.ELEMENT_NODE &&
      [XBLNS, XULNS].includes(node.namespaceURI);
};

/**
 * Ascertains whether <var>node</var> is a <code>WindowProxy</code>.
 *
 * @param {*} node
 *     Node thought to be a <code>WindowProxy</code>.
 *
 * @return {boolean}
 *     True if <var>node</var> is a DOM window.
 */
element.isDOMWindow = function(node) {
  // TODO(ato): This should use Object.prototype.toString.call(node)
  // but it's not clear how to write a good xpcshell test for that,
  // seeing as we stub out a WindowProxy.
  return typeof node == "object" &&
      node !== null &&
      typeof node.toString == "function" &&
      node.toString() == "[object Window]" &&
      node.self === node;
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
  if (!element.isDOMElement(el)) {
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

/**
 * A web element is an abstraction used to identify an element when
 * it is transported via the protocol, between remote- and local ends.
 *
 * In Marionette this abstraction can represent DOM elements,
 * WindowProxies, and XUL elements.
 */
class WebElement {
  /**
   * @param {string} uuid
   *     Identifier that must be unique across all browsing contexts
   *     for the contract to be upheld.
   */
  constructor(uuid) {
    this.uuid = assert.string(uuid);
  }

  /**
   * Performs an equality check between this web element and
   * <var>other</var>.
   *
   * @param {WebElement} other
   *     Web element to compare with this.
   *
   * @return {boolean}
   *     True if this and <var>other</var> are the same.  False
   *     otherwise.
   */
  is(other) {
    return other instanceof WebElement && this.uuid === other.uuid;
  }

  toString() {
    return `[object ${this.constructor.name} uuid=${this.uuid}]`;
  }

  /**
   * Returns a new {@link WebElement} reference for a DOM element,
   * <code>WindowProxy</code>, or XUL element.
   *
   * @param {(Element|WindowProxy|XULElement)} node
   *     Node to construct a web element reference for.
   *
   * @return {(ContentWebElement|ChromeWebElement)}
   *     Web element reference for <var>el</var>.
   *
   * @throws {InvalidArgumentError}
   *     If <var>node</var> is neither a <code>WindowProxy</code>,
   *     DOM element, or a XUL element.
   */
  static from(node) {
    const uuid = WebElement.generateUUID();

    if (element.isDOMElement(node)) {
      return new ContentWebElement(uuid);
    } else if (element.isDOMWindow(node)) {
      if (node.parent === node) {
        return new ContentWebWindow(uuid);
      }
      return new ContentWebFrame(uuid);
    } else if (element.isXULElement(node)) {
      return new ChromeWebElement(uuid);
    }

    throw new InvalidArgumentError("Expected DOM window/element " +
        pprint`or XUL element, got: ${node}`);
  }

  /**
   * Unmarshals a JSON Object to one of {@link ContentWebElement},
   * {@link ContentWebWindow}, {@link ContentWebFrame}, or
   * {@link ChromeWebElement}.
   *
   * @param {Object.<string, string>} json
   *     Web element reference, which is supposed to be a JSON Object
   *     where the key is one of the {@link WebElement} concrete
   *     classes' UUID identifiers.
   *
   * @return {WebElement}
   *     Representation of the web element.
   *
   * @throws {InvalidArgumentError}
   *     If <var>json</var> is not a web element reference.
   */
  static fromJSON(json) {
    assert.object(json);
    let keys = Object.keys(json);

    for (let key of keys) {
      switch (key) {
        case ContentWebElement.Identifier:
        case ContentWebElement.LegacyIdentifier:
          return ContentWebElement.fromJSON(json);

        case ContentWebWindow.Identifier:
          return ContentWebWindow.fromJSON(json);

        case ContentWebFrame.Identifier:
          return ContentWebFrame.fromJSON(json);

        case ChromeWebElement.Identifier:
          return ChromeWebElement.fromJSON(json);
      }
    }

    throw new InvalidArgumentError(
        pprint`Expected web element reference, got: ${json}`);
  }

  /**
   * Constructs a {@link ContentWebElement} or {@link ChromeWebElement}
   * from a a string <var>uuid</var>.
   *
   * This whole function is a workaround for the fact that clients
   * to Marionette occasionally pass <code>{id: <uuid>}</code> JSON
   * Objects instead of web element representations.  For that reason
   * we need the <var>context</var> argument to determine what kind of
   * {@link WebElement} to return.
   *
   * @param {string} uuid
   *     UUID to be associated with the web element.
   * @param {Context} context
   *     Context, which is used to determine if the returned type
   *     should be a content web element or a chrome web element.
   *
   * @return {WebElement}
   *     One of {@link ContentWebElement} or {@link ChromeWebElement},
   *     based on <var>context</var>.
   *
   * @throws {InvalidArgumentError}
   *     If <var>uuid</var> is not a string or <var>context</var>
   *     is an invalid context.
   */
  static fromUUID(uuid, context) {
    assert.string(uuid);

    switch (context) {
      case "chrome":
        return new ChromeWebElement(uuid);

      case "content":
        return new ContentWebElement(uuid);

      default:
        throw new InvalidArgumentError("Unknown context: " + context);
    }
  }

  /**
   * Checks if <var>ref<var> is a {@link WebElement} reference,
   * i.e. if it has {@link ContentWebElement.Identifier},
   * {@link ContentWebElement.LegacyIdentifier}, or
   * {@link ChromeWebElement.Identifier} as properties.
   *
   * @param {Object.<string, string>} obj
   *     Object that represents a reference to a {@link WebElement}.
   * @return {boolean}
   *     True if <var>obj</var> is a {@link WebElement}, false otherwise.
   */
  static isReference(obj) {
    if (Object.prototype.toString.call(obj) != "[object Object]") {
      return false;
    }

    if ((ContentWebElement.Identifier in obj) ||
        (ContentWebElement.LegacyIdentifier in obj) ||
        (ContentWebWindow.Identifier in obj) ||
        (ContentWebFrame.Identifier in obj) ||
        (ChromeWebElement.Identifier in obj)) {
      return true;
    }
    return false;
  }

  /**
   * Generates a unique identifier.
   *
   * @return {string}
   *     UUID.
   */
  static generateUUID() {
    let uuid = uuidGen.generateUUID().toString();
    return uuid.substring(1, uuid.length - 1);
  }
}
this.WebElement = WebElement;

/**
 * DOM elements are represented as web elements when they are
 * transported over the wire protocol.
 */
class ContentWebElement extends WebElement {
  toJSON() {
    return {
      [ContentWebElement.Identifier]: this.uuid,
      [ContentWebElement.LegacyIdentifier]: this.uuid,
    };
  }

  static fromJSON(json) {
    const {Identifier, LegacyIdentifier} = ContentWebElement;

    if (!(Identifier in json) && !(LegacyIdentifier in json)) {
      throw new InvalidArgumentError(
          pprint`Expected web element reference, got: ${json}`);
    }

    let uuid = json[Identifier] || json[LegacyIdentifier];
    return new ContentWebElement(uuid);
  }
}
ContentWebElement.Identifier = "element-6066-11e4-a52e-4f735466cecf";
ContentWebElement.LegacyIdentifier = "ELEMENT";
this.ContentWebElement = ContentWebElement;

/**
 * Top-level browsing contexts, such as <code>WindowProxy</code>
 * whose <code>opener</code> is null, are represented as web windows
 * over the wire protocol.
 */
class ContentWebWindow extends WebElement {
  toJSON() {
    return {
      [ContentWebWindow.Identifier]: this.uuid,
      [ContentWebElement.LegacyIdentifier]: this.uuid,
    };
  }

  static fromJSON(json) {
    if (!(ContentWebWindow.Identifier in json)) {
      throw new InvalidArgumentError(
          pprint`Expected web window reference, got: ${json}`);
    }
    let uuid = json[ContentWebWindow.Identifier];
    return new ContentWebWindow(uuid);
  }
}
ContentWebWindow.Identifier = "window-fcc6-11e5-b4f8-330a88ab9d7f";
this.ContentWebWindow = ContentWebWindow;

/**
 * Nested browsing contexts, such as the <code>WindowProxy</code>
 * associated with <tt>&lt;frame&gt;</tt> and <tt>&lt;iframe&gt;</tt>,
 * are represented as web frames over the wire protocol.
 */
class ContentWebFrame extends WebElement {
  toJSON() {
    return {
      [ContentWebFrame.Identifier]: this.uuid,
      [ContentWebElement.LegacyIdentifier]: this.uuid,
    };
  }

  static fromJSON(json) {
    if (!(ContentWebFrame.Identifier in json)) {
      throw new InvalidArgumentError(
          pprint`Expected web frame reference, got: ${json}`);
    }
    let uuid = json[ContentWebFrame.Identifier];
    return new ContentWebFrame(uuid);
  }
}
ContentWebFrame.Identifier = "frame-075b-4da1-b6ba-e579c2d3230a";
this.ContentWebFrame = ContentWebFrame;

/**
 * XUL elements in chrome space are represented as chrome web elements
 * over the wire protocol.
 */
class ChromeWebElement extends WebElement {
  toJSON() {
    return {
      [ChromeWebElement.Identifier]: this.uuid,
      [ContentWebElement.LegacyIdentifier]: this.uuid,
    };
  }

  static fromJSON(json) {
    if (!(ChromeWebElement.Identifier in json)) {
      throw new InvalidArgumentError("Expected chrome element reference " +
          pprint`for XUL/XBL element, got: ${json}`);
    }
    let uuid = json[ChromeWebElement.Identifier];
    return new ChromeWebElement(uuid);
  }
}
ChromeWebElement.Identifier = "chromeelement-9fc5-4b51-a3c8-01716eedeb04";
this.ChromeWebElement = ChromeWebElement;
