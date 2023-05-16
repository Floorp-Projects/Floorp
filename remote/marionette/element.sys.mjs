/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  atom: "chrome://remote/content/marionette/atom.sys.mjs",
  error: "chrome://remote/content/shared/webdriver/Errors.sys.mjs",
  PollPromise: "chrome://remote/content/marionette/sync.sys.mjs",
  pprint: "chrome://remote/content/shared/Format.sys.mjs",
});

const ORDERED_NODE_ITERATOR_TYPE = 5;
const FIRST_ORDERED_NODE_TYPE = 9;

const DOCUMENT_FRAGMENT_NODE = 11;
const ELEMENT_NODE = 1;

const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

/** XUL elements that support checked property. */
const XUL_CHECKED_ELS = new Set(["button", "checkbox", "toolbarbutton"]);

/** XUL elements that support selected property. */
const XUL_SELECTED_ELS = new Set([
  "menu",
  "menuitem",
  "menuseparator",
  "radio",
  "richlistitem",
  "tab",
]);

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
 * @namespace
 */
export const element = {};

element.Strategy = {
  ClassName: "class name",
  Selector: "css selector",
  ID: "id",
  Name: "name",
  LinkText: "link text",
  PartialLinkText: "partial link text",
  TagName: "tag name",
  XPath: "xpath",
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
 * @param {Object<string, WindowProxy>} container
 *     Window object.
 * @param {string} strategy
 *     Search strategy whereby to locate the element(s).
 * @param {string} selector
 *     Selector search pattern.  The selector must be compatible with
 *     the chosen search <var>strategy</var>.
 * @param {object=} options
 * @param {boolean=} options.all
 *     If true, a multi-element search selector is used and a sequence of
 *     elements will be returned, otherwise a single element. Defaults to false.
 * @param {Element=} options.startNode
 *     Element to use as the root of the search.
 * @param {number=} options.timeout
 *     Duration to wait before timing out the search.  If <code>all</code>
 *     is false, a {@link NoSuchElementError} is thrown if unable to
 *     find the element within the timeout duration.
 *
 * @returns {Promise.<(Element|Array.<Element>)>}
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
element.find = function(container, strategy, selector, options = {}) {
  const { all = false, startNode, timeout = 0 } = options;

  let searchFn;
  if (all) {
    searchFn = findElements.bind(this);
  } else {
    searchFn = findElement.bind(this);
  }

  return new Promise((resolve, reject) => {
    let findElements = new lazy.PollPromise(
      (resolve, reject) => {
        let res = find_(container, strategy, selector, searchFn, {
          all,
          startNode,
        });
        if (res.length) {
          resolve(Array.from(res));
        } else {
          reject([]);
        }
      },
      { timeout }
    );

    findElements.then(foundEls => {
      // the following code ought to be moved into findElement
      // and findElements when bug 1254486 is addressed
      if (!all && (!foundEls || !foundEls.length)) {
        let msg = `Unable to locate element: ${selector}`;
        reject(new lazy.error.NoSuchElementError(msg));
      }

      if (all) {
        resolve(foundEls);
      }
      resolve(foundEls[0]);
    }, reject);
  });
};

function find_(
  container,
  strategy,
  selector,
  searchFn,
  { startNode = null, all = false } = {}
) {
  let rootNode;

  if (element.isShadowRoot(startNode)) {
    rootNode = startNode.ownerDocument;
  } else {
    rootNode = container.frame.document;
  }

  if (!startNode) {
    startNode = rootNode;
  }

  let res;
  try {
    res = searchFn(strategy, selector, rootNode, startNode);
  } catch (e) {
    throw new lazy.error.InvalidSelectorError(
      `Given ${strategy} expression "${selector}" is invalid: ${e}`
    );
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
 * @param {Document} document
 *     Document root.
 * @param {Element} startNode
 *     Where in the DOM hiearchy to begin searching.
 * @param {string} expression
 *     XPath search expression.
 *
 * @returns {Node}
 *     First element matching <var>expression</var>.
 */
element.findByXPath = function(document, startNode, expression) {
  let iter = document.evaluate(
    expression,
    startNode,
    null,
    FIRST_ORDERED_NODE_TYPE,
    null
  );
  return iter.singleNodeValue;
};

/**
 * Find elements by XPath expression.
 *
 * @param {Document} document
 *     Document root.
 * @param {Element} startNode
 *     Where in the DOM hierarchy to begin searching.
 * @param {string} expression
 *     XPath search expression.
 *
 * @returns {Iterable.<Node>}
 *     Iterator over nodes matching <var>expression</var>.
 */
element.findByXPathAll = function*(document, startNode, expression) {
  let iter = document.evaluate(
    expression,
    startNode,
    null,
    ORDERED_NODE_ITERATOR_TYPE,
    null
  );
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
 * @returns {Iterable.<HTMLAnchorElement>}
 *     Sequence of link elements which text is <var>s</var>.
 */
element.findByLinkText = function(startNode, linkText) {
  return filterLinks(
    startNode,
    link => lazy.atom.getElementText(link).trim() === linkText
  );
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
 * @returns {Iterable.<HTMLAnchorElement>}
 *     Iterator of link elements which text containins
 *     <var>linkText</var>.
 */
element.findByPartialLinkText = function(startNode, linkText) {
  return filterLinks(startNode, link =>
    lazy.atom.getElementText(link).includes(linkText)
  );
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
 * @returns {Iterable.<HTMLAnchorElement>}
 *     Iterator of link elements matching <var>predicate</var>.
 */
function* filterLinks(startNode, predicate) {
  const links = getLinks(startNode);

  for (const link of links) {
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
 * @param {Document} document
 *     Document root.
 * @param {Element=} startNode
 *     Optional Element from which to start searching.
 *
 * @returns {Element}
 *     Found element.
 *
 * @throws {InvalidSelectorError}
 *     If strategy <var>using</var> is not recognised.
 * @throws {Error}
 *     If selector expression <var>selector</var> is malformed.
 */
function findElement(strategy, selector, document, startNode = undefined) {
  switch (strategy) {
    case element.Strategy.ID: {
      if (startNode.getElementById) {
        return startNode.getElementById(selector);
      }
      let expr = `.//*[@id="${selector}"]`;
      return element.findByXPath(document, startNode, expr);
    }

    case element.Strategy.Name: {
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

    case element.Strategy.LinkText: {
      const links = getLinks(startNode);
      for (const link of links) {
        if (lazy.atom.getElementText(link).trim() === selector) {
          return link;
        }
      }
      return undefined;
    }

    case element.Strategy.PartialLinkText: {
      const links = getLinks(startNode);
      for (const link of links) {
        if (lazy.atom.getElementText(link).includes(selector)) {
          return link;
        }
      }
      return undefined;
    }

    case element.Strategy.Selector:
      try {
        return startNode.querySelector(selector);
      } catch (e) {
        throw new lazy.error.InvalidSelectorError(
          `${e.message}: "${selector}"`
        );
      }
  }

  throw new lazy.error.InvalidSelectorError(`No such strategy: ${strategy}`);
}

/**
 * Find multiple elements.
 *
 * @param {element.Strategy} strategy
 *     Selector strategy to use.
 * @param {string} selector
 *     Selector expression.
 * @param {Document} document
 *     Document root.
 * @param {Element=} startNode
 *     Optional Element from which to start searching.
 *
 * @returns {Array.<Element>}
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
      return [
        ...element.findByXPathAll(
          document,
          startNode,
          `.//*[@name="${selector}"]`
        ),
      ];

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

    default:
      throw new lazy.error.InvalidSelectorError(
        `No such strategy: ${strategy}`
      );
  }
}

function getLinks(startNode) {
  // DocumentFragment doesn't have `getElementsByTagName` so using `querySelectorAll`.
  if (element.isShadowRoot(startNode)) {
    return startNode.querySelectorAll("a");
  }
  return startNode.getElementsByTagName("a");
}

/**
 * Finds the closest parent node of <var>startNode</var> matching a CSS
 * <var>selector</var> expression.
 *
 * @param {Node} startNode
 *     Cycle through <var>startNode</var>'s parent nodes in tree-order
 *     and return the first match to <var>selector</var>.
 * @param {string} selector
 *     CSS selector expression.
 *
 * @returns {Node=}
 *     First match to <var>selector</var>, or null if no match was found.
 */
element.findClosest = function(startNode, selector) {
  let node = startNode;
  while (node.parentNode && node.parentNode.nodeType == ELEMENT_NODE) {
    node = node.parentNode;
    if (node.matches(selector)) {
      return node;
    }
  }
  return null;
};

/**
 * Resolve element from specified web reference identifier.
 *
 * @param {BrowsingContext} browsingContext
 *     The browsing context to retrieve the element from.
 * @param {string} nodeId
 *     The WebReference uuid for a DOM element.
 * @param {NodeCache} nodeCache
 *     Node cache that holds already seen WebElement and ShadowRoot references.
 *
 * @returns {Element}
 *     The DOM element that the identifier was generated for.
 *
 * @throws {NoSuchElementError}
 *     If the element doesn't exist in the current browsing context.
 * @throws {StaleElementReferenceError}
 *     If the element has gone stale, indicating its node document is no
 *     longer the active document or it is no longer attached to the DOM.
 */
element.getKnownElement = function(browsingContext, nodeId, nodeCache) {
  if (!element.isNodeReferenceKnown(browsingContext, nodeId, nodeCache)) {
    throw new lazy.error.NoSuchElementError(
      `The element with the reference ${nodeId} is not known in the current browsing context`
    );
  }

  const node = nodeCache.getNode(browsingContext, nodeId);

  // Ensure the node is of the correct Node type.
  if (node !== null && !element.isElement(node)) {
    throw new lazy.error.NoSuchElementError(
      `The element with the reference ${nodeId} is not of type HTMLElement`
    );
  }

  // If null, which may be the case if the element has been unwrapped from a
  // weak reference, it is always considered stale.
  if (node === null || element.isStale(node)) {
    throw new lazy.error.StaleElementReferenceError(
      `The element with the reference ${nodeId} ` +
        "is stale; either its node document is not the active document, " +
        "or it is no longer connected to the DOM"
    );
  }

  return node;
};

/**
 * Resolve ShadowRoot from specified web reference identifier.
 *
 * @param {BrowsingContext} browsingContext
 *     The browsing context to retrieve the shadow root from.
 * @param {string} nodeId
 *     The WebReference uuid for a ShadowRoot.
 * @param {NodeCache} nodeCache
 *     Node cache that holds already seen WebElement and ShadowRoot references.
 *
 * @returns {ShadowRoot}
 *     The ShadowRoot that the identifier was generated for.
 *
 * @throws {NoSuchShadowRootError}
 *     If the ShadowRoot doesn't exist in the current browsing context.
 * @throws {DetachedShadowRootError}
 *     If the ShadowRoot is detached, indicating its node document is no
 *     longer the active document or it is no longer attached to the DOM.
 */
element.getKnownShadowRoot = function(browsingContext, nodeId, nodeCache) {
  if (!element.isNodeReferenceKnown(browsingContext, nodeId, nodeCache)) {
    throw new lazy.error.NoSuchShadowRootError(
      `The shadow root with the reference ${nodeId} is not known in the current browsing context`
    );
  }

  const node = nodeCache.getNode(browsingContext, nodeId);

  // Ensure the node is of the correct Node type.
  if (node !== null && !element.isShadowRoot(node)) {
    throw new lazy.error.NoSuchShadowRootError(
      `The shadow root with the reference ${nodeId} is not of type ShadowRoot`
    );
  }

  // If null, which may be the case if the element has been unwrapped from a
  // weak reference, it is always considered stale.
  if (node === null || element.isDetached(node)) {
    throw new lazy.error.DetachedShadowRootError(
      `The shadow root with the reference ${nodeId} ` +
        "is detached; either its node document is not the active document, " +
        "or it is no longer connected to the DOM"
    );
  }

  return node;
};

/**
 * Determines if <var>obj<var> is an HTML or JS collection.
 *
 * @param {object} seq
 *     Type to determine.
 *
 * @returns {boolean}
 *     True if <var>seq</va> is a collection.
 */
element.isCollection = function(seq) {
  switch (Object.prototype.toString.call(seq)) {
    case "[object Arguments]":
    case "[object Array]":
    case "[object DOMTokenList]":
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
 * Determines if <var>shadowRoot</var> is detached.
 *
 * A ShadowRoot is detached if its node document is not the active document
 * or if the element node referred to as its host is stale.
 *
 * @param {ShadowRoot} shadowRoot
 *     ShadowRoot to check for detached state.
 *
 * @returns {boolean}
 *     True if <var>shadowRoot</var> is detached, false otherwise.
 */
element.isDetached = function(shadowRoot) {
  return (
    !shadowRoot.ownerDocument.isActive() || element.isStale(shadowRoot.host)
  );
};

/**
 * Determines if the node reference is known for the given browsing context.
 *
 * For WebDriver classic only nodes from the same browsing context are
 * allowed to be accessed.
 *
 * @param {BrowsingContext} browsingContext
 *     The browsing context the element has to be part of.
 * @param {ElementIdentifier} nodeId
 *     The WebElement reference identifier for a DOM element.
 * @param {NodeCache} nodeCache
 *     Node cache that holds already seen node references.
 *
 * @returns {boolean}
 *     True if the element is known in the given browsing context.
 */
element.isNodeReferenceKnown = function(browsingContext, nodeId, nodeCache) {
  const nodeDetails = nodeCache.getReferenceDetails(nodeId);
  if (nodeDetails === null) {
    return false;
  }

  if (nodeDetails.isTopBrowsingContext) {
    // As long as Navigables are not available any cross-group navigation will
    // cause a swap of the current top-level browsing context. The only unique
    // identifier in such a case is the browser id the top-level browsing
    // context actually lives in.
    return nodeDetails.browserId === browsingContext.browserId;
  }

  return nodeDetails.browsingContextId === browsingContext.id;
};

/**
 * Determines if <var>el</var> is stale.
 *
 * An element is stale if its node document is not the active document
 * or if it is not connected.
 *
 * @param {Element} el
 *     Element to check for staleness.
 *
 * @returns {boolean}
 *     True if <var>el</var> is stale, false otherwise.
 */
element.isStale = function(el) {
  if (!el.ownerGlobal) {
    // Without a valid inner window the document is basically closed.
    return true;
  }

  return !el.ownerDocument.isActive() || !el.isConnected;
};

/**
 * Determine if <var>el</var> is selected or not.
 *
 * This operation only makes sense on
 * <tt>&lt;input type=checkbox&gt;</tt>,
 * <tt>&lt;input type=radio&gt;</tt>,
 * and <tt>&gt;option&gt;</tt> elements.
 *
 * @param {Element} el
 *     Element to test if selected.
 *
 * @returns {boolean}
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
 * An element is considered read only if it is an
 * <code>&lt;input&gt;</code> or <code>&lt;textarea&gt;</code>
 * element whose <code>readOnly</code> content IDL attribute is set.
 *
 * @param {Element} el
 *     Element to test is read only.
 *
 * @returns {boolean}
 *     True if element is read only.
 */
element.isReadOnly = function(el) {
  return (
    element.isDOMElement(el) &&
    ["input", "textarea"].includes(el.localName) &&
    el.readOnly
  );
};

/**
 * An element is considered disabled if it is a an element
 * that can be disabled, or it belongs to a container group which
 * <code>disabled</code> content IDL attribute affects it.
 *
 * @param {Element} el
 *     Element to test for disabledness.
 *
 * @returns {boolean}
 *     True if element, or its container group, is disabled.
 */
element.isDisabled = function(el) {
  if (!element.isDOMElement(el)) {
    return false;
  }

  switch (el.localName) {
    case "option":
    case "optgroup":
      if (el.disabled) {
        return true;
      }
      let parent = element.findClosest(el, "optgroup,select");
      return element.isDisabled(parent);

    case "button":
    case "input":
    case "select":
    case "textarea":
      return el.disabled;

    default:
      return false;
  }
};

/**
 * Denotes elements that can be used for typing and clearing.
 *
 * Elements that are considered WebDriver-editable are non-readonly
 * and non-disabled <code>&lt;input&gt;</code> elements in the Text,
 * Search, URL, Telephone, Email, Password, Date, Month, Date and
 * Time Local, Number, Range, Color, and File Upload states, and
 * <code>&lt;textarea&gt;</code> elements.
 *
 * @param {Element} el
 *     Element to test.
 *
 * @returns {boolean}
 *     True if editable, false otherwise.
 */
element.isMutableFormControl = function(el) {
  if (!element.isDOMElement(el)) {
    return false;
  }
  if (element.isReadOnly(el) || element.isDisabled(el)) {
    return false;
  }

  if (el.localName == "textarea") {
    return true;
  }

  if (el.localName != "input") {
    return false;
  }

  switch (el.type) {
    case "color":
    case "date":
    case "datetime-local":
    case "email":
    case "file":
    case "month":
    case "number":
    case "password":
    case "range":
    case "search":
    case "tel":
    case "text":
    case "time":
    case "url":
    case "week":
      return true;

    default:
      return false;
  }
};

/**
 * An editing host is a node that is either an HTML element with a
 * <code>contenteditable</code> attribute, or the HTML element child
 * of a document whose <code>designMode</code> is enabled.
 *
 * @param {Element} el
 *     Element to determine if is an editing host.
 *
 * @returns {boolean}
 *     True if editing host, false otherwise.
 */
element.isEditingHost = function(el) {
  return (
    element.isDOMElement(el) &&
    (el.isContentEditable || el.ownerDocument.designMode == "on")
  );
};

/**
 * Determines if an element is editable according to WebDriver.
 *
 * An element is considered editable if it is not read-only or
 * disabled, and one of the following conditions are met:
 *
 * <ul>
 * <li>It is a <code>&lt;textarea&gt;</code> element.
 *
 * <li>It is an <code>&lt;input&gt;</code> element that is not of
 * the <code>checkbox</code>, <code>radio</code>, <code>hidden</code>,
 * <code>submit</code>, <code>button</code>, or <code>image</code> types.
 *
 * <li>It is content-editable.
 *
 * <li>It belongs to a document in design mode.
 * </ul>
 *
 * @param {Element} el
 *     Element to test if editable.
 *
 * @returns {boolean}
 *     True if editable, false otherwise.
 */
element.isEditable = function(el) {
  if (!element.isDOMElement(el)) {
    return false;
  }

  if (element.isReadOnly(el) || element.isDisabled(el)) {
    return false;
  }

  return element.isMutableFormControl(el) || element.isEditingHost(el);
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
 * @returns {Object<string, number>}
 *     X- and Y coordinates.
 *
 * @throws TypeError
 *     If <var>xOffset</var> or <var>yOffset</var> are not numbers.
 */
element.coordinates = function(node, xOffset = undefined, yOffset = undefined) {
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
 * @returns {boolean}
 *     True if if <var>el</var> is in viewport, false otherwise.
 */
element.inViewport = function(el, x = undefined, y = undefined) {
  let win = el.ownerGlobal;
  let c = element.coordinates(el, x, y);
  let vp = {
    top: win.pageYOffset,
    left: win.pageXOffset,
    bottom: win.pageYOffset + win.innerHeight,
    right: win.pageXOffset + win.innerWidth,
  };

  return (
    vp.left <= c.x + win.pageXOffset &&
    c.x + win.pageXOffset <= vp.right &&
    vp.top <= c.y + win.pageYOffset &&
    c.y + win.pageYOffset <= vp.bottom
  );
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
 * @returns {Element}
 *     Container element of <var>el</var>.
 */
element.getContainer = function(el) {
  // Does <option> or <optgroup> have a valid context,
  // meaning is it a child of <datalist> or <select>?
  if (["option", "optgroup"].includes(el.localName)) {
    return element.findClosest(el, "datalist,select") || el;
  }

  return el;
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
 * @returns {boolean}
 *     True if <var>el</var> is inside the viewport, or false otherwise.
 */
element.isInView = function(el) {
  let originalPointerEvents = el.style.pointerEvents;

  try {
    el.style.pointerEvents = "auto";
    const tree = element.getPointerInteractablePaintTree(el);

    // Bug 1413493 - <tr> is not part of the returned paint tree yet. As
    // workaround check the visibility based on the first contained cell.
    if (el.localName === "tr" && el.cells && el.cells.length) {
      return tree.includes(el.cells[0]);
    }

    return tree.includes(el);
  } finally {
    el.style.pointerEvents = originalPointerEvents;
  }
};

/**
 * Generates a unique identifier.
 *
 * The generated uuid will not contain the curly braces.
 *
 * @returns {string}
 *     UUID.
 */
element.generateUUID = function() {
  return Services.uuid
    .generateUUID()
    .toString()
    .slice(1, -1);
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
 * @returns {boolean}
 *     True if visible, false otherwise.
 */
element.isVisible = function(el, x = undefined, y = undefined) {
  let win = el.ownerGlobal;

  if (!lazy.atom.isElementDisplayed(el, win)) {
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
 * @returns {boolean}
 *     True if element is obscured, false otherwise.
 */
element.isObscured = function(el) {
  let tree = element.getPointerInteractablePaintTree(el);
  return !el.contains(tree[0]);
};

// TODO(ato): Only used by deprecated action API
// https://bugzil.la/1354578
/**
 * Calculates the in-view centre point of an element's client rect.
 *
 * The portion of an element that is said to be _in view_, is the
 * intersection of two squares: the first square being the initial
 * viewport, and the second a DOM element.  From this square we
 * calculate the in-view _centre point_ and convert it into CSS pixels.
 *
 * Although Gecko's system internals allow click points to be
 * given in floating point precision, the DOM operates in CSS pixels.
 * When the in-view centre point is later used to retrieve a coordinate's
 * paint tree, we need to ensure to operate in the same language.
 *
 * As a word of warning, there appears to be inconsistencies between
 * how `DOMElement.elementsFromPoint` and `DOMWindowUtils.sendMouseEvent`
 * internally rounds (ceils/floors) coordinates.
 *
 * @param {DOMRect} rect
 *     Element off a DOMRect sequence produced by calling
 *     `getClientRects` on an {@link Element}.
 * @param {WindowProxy} win
 *     Current window global.
 *
 * @returns {Map.<string, number>}
 *     X and Y coordinates that denotes the in-view centre point of
 *     `rect`.
 */
element.getInViewCentrePoint = function(rect, win) {
  const { floor, max, min } = Math;

  // calculate the intersection of the rect that is inside the viewport
  let visible = {
    left: max(0, min(rect.x, rect.x + rect.width)),
    right: min(win.innerWidth, max(rect.x, rect.x + rect.width)),
    top: max(0, min(rect.y, rect.y + rect.height)),
    bottom: min(win.innerHeight, max(rect.y, rect.y + rect.height)),
  };

  // arrive at the centre point of the visible rectangle
  let x = (visible.left + visible.right) / 2.0;
  let y = (visible.top + visible.bottom) / 2.0;

  // convert to CSS pixels, as centre point can be float
  x = floor(x);
  y = floor(y);

  return { x, y };
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
 * @returns {Array.<DOMElement>}
 *     Sequence of elements in paint order.
 */
element.getPointerInteractablePaintTree = function(el) {
  const doc = el.ownerDocument;
  const win = doc.defaultView;
  const rootNode = el.getRootNode();

  // pointer-interactable elements tree, step 1
  if (!el.isConnected) {
    return [];
  }

  // steps 2-3
  let rects = el.getClientRects();
  if (!rects.length) {
    return [];
  }

  // step 4
  let centre = element.getInViewCentrePoint(rects[0], win);

  // step 5
  return rootNode.elementsFromPoint(centre.x, centre.y);
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
    el.scrollIntoView({ block: "end", inline: "nearest" });
  }
};

/**
 * Ascertains whether <var>obj</var> is a DOM-, SVG-, or XUL element.
 *
 * @param {object} obj
 *     Object thought to be an <code>Element</code> or
 *     <code>XULElement</code>.
 *
 * @returns {boolean}
 *     True if <var>obj</var> is an element, false otherwise.
 */
element.isElement = function(obj) {
  return element.isDOMElement(obj) || element.isXULElement(obj);
};

/**
 * Returns the shadow root of an element.
 *
 * @param {Element} el
 *     Element thought to have a <code>shadowRoot</code>
 * @returns {ShadowRoot}
 *     Shadow root of the element.
 */
element.getShadowRoot = function(el) {
  const shadowRoot = el.openOrClosedShadowRoot;
  if (!shadowRoot) {
    throw new lazy.error.NoSuchShadowRootError();
  }
  return shadowRoot;
};

/**
 * Ascertains whether <var>node</var> is a shadow root.
 *
 * @param {ShadowRoot} node
 *   The node that will be checked to see if it has a shadow root
 *
 * @returns {boolean}
 *     True if <var>node</var> is a shadow root, false otherwise.
 */
element.isShadowRoot = function(node) {
  return (
    node &&
    node.nodeType === DOCUMENT_FRAGMENT_NODE &&
    node.containingShadowRoot == node
  );
};

/**
 * Ascertains whether <var>obj</var> is a DOM element.
 *
 * @param {object} obj
 *     Object to check.
 *
 * @returns {boolean}
 *     True if <var>obj</var> is a DOM element, false otherwise.
 */
element.isDOMElement = function(obj) {
  return obj && obj.nodeType == ELEMENT_NODE && !element.isXULElement(obj);
};

/**
 * Ascertains whether <var>obj</var> is a XUL element.
 *
 * @param {object} obj
 *     Object to check.
 *
 * @returns {boolean}
 *     True if <var>obj</var> is a XULElement, false otherwise.
 */
element.isXULElement = function(obj) {
  return obj && obj.nodeType === ELEMENT_NODE && obj.namespaceURI === XUL_NS;
};

/**
 * Ascertains whether <var>node</var> is in a privileged document.
 *
 * @param {Node} node
 *     Node to check.
 *
 * @returns {boolean}
 *     True if <var>node</var> is in a privileged document,
 *     false otherwise.
 */
element.isInPrivilegedDocument = function(node) {
  return !!node?.nodePrincipal?.isSystemPrincipal;
};

/**
 * Ascertains whether <var>obj</var> is a <code>WindowProxy</code>.
 *
 * @param {object} obj
 *     Object to check.
 *
 * @returns {boolean}
 *     True if <var>obj</var> is a DOM window.
 */
element.isDOMWindow = function(obj) {
  // TODO(ato): This should use Object.prototype.toString.call(node)
  // but it's not clear how to write a good xpcshell test for that,
  // seeing as we stub out a WindowProxy.
  return (
    typeof obj == "object" &&
    obj !== null &&
    typeof obj.toString == "function" &&
    obj.toString() == "[object Window]" &&
    obj.self === obj
  );
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
 * @param {Element} el
 *     Element to test if <var>attr</var> is a boolean attribute on.
 * @param {string} attr
 *     Attribute to test is a boolean attribute.
 *
 * @returns {boolean}
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
 * A web reference is an abstraction used to identify an element when
 * it is transported via the protocol, between remote- and local ends.
 *
 * In Marionette this abstraction can represent DOM elements,
 * WindowProxies, and XUL elements.
 */
export class WebReference {
  /**
   * @param {string} uuid
   *     Identifier that must be unique across all browsing contexts
   *     for the contract to be upheld.
   */
  constructor(uuid) {
    this.uuid = lazy.assert.string(uuid);
  }

  /**
   * Performs an equality check between this web element and
   * <var>other</var>.
   *
   * @param {WebReference} other
   *     Web element to compare with this.
   *
   * @returns {boolean}
   *     True if this and <var>other</var> are the same.  False
   *     otherwise.
   */
  is(other) {
    return other instanceof WebReference && this.uuid === other.uuid;
  }

  toString() {
    return `[object ${this.constructor.name} uuid=${this.uuid}]`;
  }

  /**
   * Returns a new {@link WebReference} reference for a DOM or XUL element,
   * <code>WindowProxy</code>, or <code>ShadowRoot</code>.
   *
   * @param {(Element|ShadowRoot|WindowProxy|MockXULElement)} node
   *     Node to construct a web element reference for.
   * @param {string=} uuid
   *     Optional unique identifier of the WebReference if already known.
   *     If not defined a new unique identifier will be created.
   *
   * @returns {WebReference}
   *     Web reference for <var>node</var>.
   *
   * @throws {InvalidArgumentError}
   *     If <var>node</var> is neither a <code>WindowProxy</code>,
   *     DOM or XUL element, or <code>ShadowRoot</code>.
   */
  static from(node, uuid) {
    if (uuid === undefined) {
      uuid = element.generateUUID();
    }

    if (element.isShadowRoot(node) && !element.isInPrivilegedDocument(node)) {
      // When we support Chrome Shadowroots we will need to
      // do a check here of shadowroot.host being in a privileged document
      // See Bug 1743541
      return new ShadowRoot(uuid);
    } else if (element.isElement(node)) {
      return new WebElement(uuid);
    } else if (element.isDOMWindow(node)) {
      if (node.parent === node) {
        return new WebWindow(uuid);
      }
      return new WebFrame(uuid);
    }

    throw new lazy.error.InvalidArgumentError(
      "Expected DOM window/element " + lazy.pprint`or XUL element, got: ${node}`
    );
  }

  /**
   * Unmarshals a JSON Object to one of {@link ShadowRoot}, {@link WebElement},
   * {@link WebFrame}, or {@link WebWindow}.
   *
   * @param {Object<string, string>} json
   *     Web reference, which is supposed to be a JSON Object
   *     where the key is one of the {@link WebReference} concrete
   *     classes' UUID identifiers.
   *
   * @returns {WebReference}
   *     Web reference for the JSON object.
   *
   * @throws {InvalidArgumentError}
   *     If <var>json</var> is not a web reference.
   */
  static fromJSON(json) {
    lazy.assert.object(json);
    if (json instanceof WebReference) {
      return json;
    }
    let keys = Object.keys(json);

    for (let key of keys) {
      switch (key) {
        case ShadowRoot.Identifier:
          return ShadowRoot.fromJSON(json);

        case WebElement.Identifier:
          return WebElement.fromJSON(json);

        case WebFrame.Identifier:
          return WebFrame.fromJSON(json);

        case WebWindow.Identifier:
          return WebWindow.fromJSON(json);
      }
    }

    throw new lazy.error.InvalidArgumentError(
      lazy.pprint`Expected web reference, got: ${json}`
    );
  }

  /**
   * Checks if <var>obj<var> is a {@link WebReference} reference.
   *
   * @param {Object<string, string>} obj
   *     Object that represents a {@link WebReference}.
   *
   * @returns {boolean}
   *     True if <var>obj</var> is a {@link WebReference}, false otherwise.
   */
  static isReference(obj) {
    if (Object.prototype.toString.call(obj) != "[object Object]") {
      return false;
    }

    if (
      ShadowRoot.Identifier in obj ||
      WebElement.Identifier in obj ||
      WebFrame.Identifier in obj ||
      WebWindow.Identifier in obj
    ) {
      return true;
    }
    return false;
  }
}

/**
 * DOM elements are represented as web elements when they are
 * transported over the wire protocol.
 */
export class WebElement extends WebReference {
  toJSON() {
    return { [WebElement.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    const { Identifier } = WebElement;

    if (!(Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Expected web element reference, got: ${json}`
      );
    }

    let uuid = json[Identifier];
    return new WebElement(uuid);
  }

  /**
   * Constructs a {@link WebElement} from a string <var>uuid</var>.
   *
   * This whole function is a workaround for the fact that clients
   * to Marionette occasionally pass <code>{id: <uuid>}</code> JSON
   * Objects instead of web element representations.
   *
   * @param {string} uuid
   *     UUID to be associated with the web reference.
   *
   * @returns {WebElement}
   *     The web element reference.
   *
   * @throws {InvalidArgumentError}
   *     If <var>uuid</var> is not a string.
   */
  static fromUUID(uuid) {
    return new WebElement(uuid);
  }
}

WebElement.Identifier = "element-6066-11e4-a52e-4f735466cecf";

/**
 * Shadow Root elements are represented as shadow root references when they are
 * transported over the wire protocol
 */
export class ShadowRoot extends WebReference {
  toJSON() {
    return { [ShadowRoot.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    const { Identifier } = ShadowRoot;

    if (!(Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Expected shadow root reference, got: ${json}`
      );
    }

    let uuid = json[Identifier];
    return new ShadowRoot(uuid);
  }

  /**
   * Constructs a {@link ShadowRoot} from a string <var>uuid</var>.
   *
   * This whole function is a workaround for the fact that clients
   * to Marionette occasionally pass <code>{id: <uuid>}</code> JSON
   * Objects instead of shadow root representations.
   *
   * @param {string} uuid
   *     UUID to be associated with the web reference.
   *
   * @returns {ShadowRoot}
   *     The shadow root reference.
   *
   * @throws {InvalidArgumentError}
   *     If <var>uuid</var> is not a string.
   */
  static fromUUID(uuid) {
    lazy.assert.string(uuid);

    return new ShadowRoot(uuid);
  }
}

ShadowRoot.Identifier = "shadow-6066-11e4-a52e-4f735466cecf";

/**
 * Top-level browsing contexts, such as <code>WindowProxy</code>
 * whose <code>opener</code> is null, are represented as web windows
 * over the wire protocol.
 */
export class WebWindow extends WebReference {
  toJSON() {
    return { [WebWindow.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    if (!(WebWindow.Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Expected web window reference, got: ${json}`
      );
    }
    let uuid = json[WebWindow.Identifier];
    return new WebWindow(uuid);
  }
}

WebWindow.Identifier = "window-fcc6-11e5-b4f8-330a88ab9d7f";

/**
 * Nested browsing contexts, such as the <code>WindowProxy</code>
 * associated with <tt>&lt;frame&gt;</tt> and <tt>&lt;iframe&gt;</tt>,
 * are represented as web frames over the wire protocol.
 */
export class WebFrame extends WebReference {
  toJSON() {
    return { [WebFrame.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    if (!(WebFrame.Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Expected web frame reference, got: ${json}`
      );
    }
    let uuid = json[WebFrame.Identifier];
    return new WebFrame(uuid);
  }
}

WebFrame.Identifier = "frame-075b-4da1-b6ba-e579c2d3230a";
