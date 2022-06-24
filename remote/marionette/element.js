/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "ChromeWebElement",
  "ContentShadowRoot",
  "element",
  "WebElement",
  "WebFrame",
  "WebReference",
  "WebWindow",
];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ContentDOMReference: "resource://gre/modules/ContentDOMReference.jsm",

  assert: "chrome://remote/content/shared/webdriver/Assert.jsm",
  atom: "chrome://remote/content/marionette/atom.js",
  error: "chrome://remote/content/shared/webdriver/Errors.jsm",
  PollPromise: "chrome://remote/content/marionette/sync.js",
  pprint: "chrome://remote/content/shared/Format.jsm",
});

const ORDERED_NODE_ITERATOR_TYPE = 5;
const FIRST_ORDERED_NODE_TYPE = 9;

const ELEMENT_NODE = 1;
const DOCUMENT_NODE = 9;

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
 * The {@link element.ReferenceStore} provides a mapping between web element
 * references and the ContentDOMReference of DOM elements for each browsing
 * context.  It also provides functionality for looking up and retrieving
 * elements.
 *
 * @namespace
 */
const element = {};

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
 * Stores known/seen web element references and their associated
 * ContentDOMReference ElementIdentifiers.
 *
 * The ContentDOMReference ElementIdentifier is augmented with a WebReference
 * reference, so in Marionette's IPC it looks like the following example:
 *
 * { browsingContextId: 9,
 *   id: 0.123,
 *   webElRef: {element-6066-11e4-a52e-4f735466cecf: <uuid>} }
 *
 * For use in parent process in conjunction with ContentDOMReference in content.
 *
 * @class
 * @memberof element
 */
element.ReferenceStore = class {
  constructor() {
    // uuid -> { id, browsingContextId, webElRef }
    this.refs = new Map();
    // id -> webElRef
    this.domRefs = new Map();
  }

  clear(browsingContext) {
    if (!browsingContext) {
      this.refs.clear();
      this.domRefs.clear();
      return;
    }
    for (const context of browsingContext.getAllBrowsingContextsInSubtree()) {
      for (const [uuid, elId] of this.refs) {
        if (elId.browsingContextId == context.id) {
          this.refs.delete(uuid);
          this.domRefs.delete(elId.id);
        }
      }
    }
  }

  /**
   * Make a collection of elements seen.
   *
   * The order of the returned web element references is guaranteed to
   * match that of the collection passed in.
   *
   * @param {Array.<ElementIdentifer>} elIds
   *     Sequence of ids to add to set of seen elements.
   *
   * @return {Array.<WebReference>}
   *     List of the web element references associated with each element
   *     from <var>els</var>.
   */
  addAll(elIds) {
    return [...elIds].map(elId => this.add(elId));
  }

  /**
   * Make an element seen.
   *
   * @param {ElementIdentifier} elId
   *    {id, browsingContextId} to add to set of seen elements.
   *
   * @return {WebReference}
   *     Web element reference associated with element.
   *
   */
  add(elId) {
    if (!elId.id || !elId.browsingContextId) {
      throw new TypeError(
        lazy.pprint`Expected ElementIdentifier, got: ${elId}`
      );
    }
    if (this.domRefs.has(elId.id)) {
      return WebReference.fromJSON(this.domRefs.get(elId.id));
    }
    const webEl = WebReference.fromJSON(elId.webElRef);
    this.refs.set(webEl.uuid, elId);
    this.domRefs.set(elId.id, elId.webElRef);
    return webEl;
  }

  /**
   * Determine if the provided web element reference is in the store.
   *
   * Unlike when getting the element, a staleness check is not
   * performed.
   *
   * @param {WebReference} webEl
   *     Element's associated web element reference.
   *
   * @return {boolean}
   *     True if element is in the store, false otherwise.
   *
   * @throws {TypeError}
   *     If <var>webEl</var> is not a {@link WebReference}.
   */
  has(webEl) {
    if (!(webEl instanceof WebReference)) {
      throw new TypeError(lazy.pprint`Expected web element, got: ${webEl}`);
    }
    return this.refs.has(webEl.uuid);
  }

  /**
   * Retrieve a DOM {@link Element} or a {@link XULElement} by its
   * unique {@link WebReference} reference.
   *
   * @param {WebReference} webEl
   *     Web element reference to find the associated {@link Element}
   *     of.
   * @returns {ElementIdentifier}
   *     ContentDOMReference identifier
   *
   * @throws {TypeError}
   *     If <var>webEl</var> is not a {@link WebReference}.
   * @throws {NoSuchElementError}
   *     If the web element reference <var>uuid</var> has not been
   *     seen before.
   */
  get(webEl) {
    if (!(webEl instanceof WebReference)) {
      throw new TypeError(lazy.pprint`Expected web element, got: ${webEl}`);
    }
    const elId = this.refs.get(webEl.uuid);
    if (!elId) {
      throw new lazy.error.NoSuchElementError(
        "Web element reference not seen before: " + webEl.uuid
      );
    }

    return elId;
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
 *     Window object.
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
    let findElements = new lazy.PollPromise(
      (resolve, reject) => {
        let res = find_(container, strategy, selector, searchFn, {
          all,
          startNode,
        });
        if (res.length > 0) {
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
      if (!opts.all && (!foundEls || foundEls.length == 0)) {
        let msg = `Unable to locate element: ${selector}`;
        reject(new lazy.error.NoSuchElementError(msg));
      }

      if (opts.all) {
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
  let rootNode = container.frame.document;

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
 * @return {Iterable.<HTMLAnchorElement>}
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
 * @return {Iterable.<HTMLAnchorElement>}
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

    case element.Strategy.LinkText:
      for (let link of startNode.getElementsByTagName("a")) {
        if (lazy.atom.getElementText(link).trim() === selector) {
          return link;
        }
      }
      return undefined;

    case element.Strategy.PartialLinkText:
      for (let link of startNode.getElementsByTagName("a")) {
        if (lazy.atom.getElementText(link).includes(selector)) {
          return link;
        }
      }
      return undefined;

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

/**
 * Finds the closest parent node of <var>startNode</var> by CSS a
 * <var>selector</var> expression.
 *
 * @param {Node} startNode
 *     Cyce through <var>startNode</var>'s parent nodes in tree-order
 *     and return the first match to <var>selector</var>.
 * @param {string} selector
 *     CSS selector expression.
 *
 * @return {Node=}
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
 * Wrapper around ContentDOMReference.get with additional steps specific to
 * Marionette.
 *
 * @param {Element} el
 *     The DOM element to generate the identifier for.
 *
 * @return {object} The ContentDOMReference ElementIdentifier for the DOM
 *     element augmented with a Marionette WebReference reference, and some
 *     additional properties.
 *
 * @throws {StaleElementReferenceError}
 *     If the element has gone stale, indicating it is no longer
 *     attached to the DOM, or its node document is no longer the
 *     active document.
 */
element.getElementId = function(el) {
  if (element.isStale(el)) {
    throw new lazy.error.StaleElementReferenceError(
      lazy.pprint`The element reference of ${el} ` +
        "is stale; either the element is no longer attached to the DOM, " +
        "it is not in the current frame context, " +
        "or the document has been refreshed"
    );
  }

  const webEl = WebReference.from(el);

  const id = lazy.ContentDOMReference.get(el);
  const browsingContext = BrowsingContext.get(id.browsingContextId);

  id.webElRef = webEl.toJSON();
  id.browserId = browsingContext.browserId;
  id.isTopLevel = !browsingContext.parent;

  return id;
};

/**
 * Wrapper around ContentDOMReference.resolve with additional error handling
 * specific to Marionette.
 *
 * @param {ElementIdentifier} id
 *     The identifier generated via ContentDOMReference.get for a DOM element.
 *
 * @param {WindowProxy} win
 *     Current window, which may differ from the associated
 *     window of <var>el</var>.
 *
 * @return {Element} The DOM element that the identifier was generated for, or
 *     null if the element does not still exist.
 *
 * @throws {NoSuchElementError}
 *     If element represented by reference <var>id</var> doesn't exist
 *     in the current browsing context.
 * @throws {StaleElementReferenceError}
 *     If the element has gone stale, indicating it is no longer
 *     attached to the DOM, or its node document is no longer the
 *     active document.
 */
element.resolveElement = function(id, win) {
  let sameBrowsingContext;
  if (id.isTopLevel) {
    // Cross-group navigations cause a swap of the current top-level browsing
    // context. The only unique identifier is the browser id the browsing
    // context actually lives in. If it's equal also treat the browsing context
    // as the same (bug 1690308).
    // If the element's browsing context is a top-level browsing context,
    sameBrowsingContext = id.browserId == win?.browsingContext.browserId;
  } else {
    // For non top-level browsing contexts check for equality directly.
    sameBrowsingContext = id.browsingContextId == win?.browsingContext.id;
  }

  if (!sameBrowsingContext) {
    throw new lazy.error.NoSuchElementError(
      `Web element reference not seen before: ${JSON.stringify(id.webElRef)}`
    );
  }

  const el = lazy.ContentDOMReference.resolve(id);

  if (element.isStale(el, win)) {
    throw new lazy.error.StaleElementReferenceError(
      lazy.pprint`The element reference of ${el ||
        JSON.stringify(id.webElRef)} ` +
        "is stale; either the element is no longer attached to the DOM, " +
        "it is not in the current frame context, " +
        "or the document has been refreshed"
    );
  }
  return el;
};

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
 * <var>win<var>, is a WebDriver concept defining the target
 * against which commands will run.  As the current browsing context
 * may differ from <var>el</var>'s associated context, an element is
 * considered stale even if it is connected to a living (not discarded)
 * browsing context such as an <tt>&lt;iframe&gt;</tt>.
 *
 * @param {Element=} el
 *     DOM element to check for staleness.  If null, which may be
 *     the case if the element has been unwrapped from a weak
 *     reference, it is always considered stale.
 * @param {WindowProxy=} win
 *     Current window global, which may differ from the associated
 *     window global of <var>el</var>.  When retrieving XUL
 *     elements, this is optional.
 *
 * @return {boolean}
 *     True if <var>el</var> is stale, false otherwise.
 */
element.isStale = function(el, win = undefined) {
  if (typeof win == "undefined") {
    win = el.ownerGlobal;
  }
  if (el === null || !el.ownerGlobal || el.ownerDocument !== win.document) {
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
 * An element is considered read only if it is an
 * <code>&lt;input&gt;</code> or <code>&lt;textarea&gt;</code>
 * element whose <code>readOnly</code> content IDL attribute is set.
 *
 * @param {Element} el
 *     Element to test is read only.
 *
 * @return {boolean}
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
 * @return {boolean}
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
 * @return {boolean}
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
 * @return {boolean}
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
 * @param {Element}
 *     Element to test if editable.
 *
 * @return {boolean}
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
 * @return {Object.<string, number>}
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
 * @return {boolean}
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
 * @return {Element}
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
 * @return {Map.<string, number>}
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
 * @return {Array.<DOMElement>}
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
  if (rects.length == 0) {
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
 * Ascertains whether <var>node</var> is a DOM-, SVG-, or XUL element.
 *
 * @param {Node} node
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
 * Returns the shadow root of an element.
 *
 * @param {Node} node
 *     Element thought to have a <code>shadowRoot</code>
 * @returns {ShadowRoot}
 *     shadow root of the element.
 */
element.getShadowRoot = function(node) {
  const shadowRoot = node.openOrClosedShadowRoot;
  if (!shadowRoot) {
    throw new lazy.error.NoSuchShadowRootError();
  }
  return shadowRoot;
};

/**
 * Checks whether a node has a shadow root.
 * @param {Node} node
 *   The node that will be checked to see if it has a shadow root
 * @returns {boolean}
 *   true, if it has a shadow root
 */
element.isShadowRoot = function(node) {
  return (
    node !== null &&
    typeof node == "object" &&
    node.containingShadowRoot == node
  );
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
  return (
    typeof node == "object" &&
    node !== null &&
    "nodeType" in node &&
    [ELEMENT_NODE, DOCUMENT_NODE].includes(node.nodeType) &&
    !element.isXULElement(node)
  );
};

/**
 * Ascertains whether <var>node</var> is a XUL element.
 *
 * @param {*} node
 *     Element to check
 *
 * @return {boolean}
 *     True if <var>node</var> is a XULElement,
 *     false otherwise.
 */
element.isXULElement = function(node) {
  return (
    typeof node == "object" &&
    node !== null &&
    "nodeType" in node &&
    node.nodeType === node.ELEMENT_NODE &&
    node.namespaceURI === XUL_NS
  );
};

/**
 * Ascertains whether <var>node</var> is in a privileged document.
 *
 * @param {*} node
 *     Node to check.
 *
 * @return {boolean}
 *     True if <var>node</var> is in a privileged document,
 *     false otherwise.
 */
element.isInPrivilegedDocument = function(node) {
  return !!node?.nodePrincipal?.isSystemPrincipal;
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
  return (
    typeof node == "object" &&
    node !== null &&
    typeof node.toString == "function" &&
    node.toString() == "[object Window]" &&
    node.self === node
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
 * A web reference is an abstraction used to identify an element when
 * it is transported via the protocol, between remote- and local ends.
 *
 * In Marionette this abstraction can represent DOM elements,
 * WindowProxies, and XUL elements.
 */
class WebReference {
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
   * @return {boolean}
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
   * Returns a new {@link WebReference} reference for a DOM element,
   * <code>WindowProxy</code>, ShadowRoot, or XUL element.
   *
   * @param {(Element|ShadowRoot|WindowProxy|XULElement)} node
   *     Node to construct a web element reference for.
   *
   * @return {(ContentShadowRoot|WebElement|ChromeWebElement)}
   *     Web element reference for <var>el</var>.
   *
   * @throws {InvalidArgumentError}
   *     If <var>node</var> is neither a <code>WindowProxy</code>,
   *     DOM element, or a XUL element.
   */
  static from(node) {
    const uuid = WebReference.generateUUID();

    if (element.isShadowRoot(node) && !element.isInPrivilegedDocument(node)) {
      // When we support Chrome Shadowroots we will need to
      // do a check here of shadowroot.host being in a privileged document
      // See Bug 1743541
      return new ContentShadowRoot(uuid);
    } else if (element.isElement(node)) {
      if (element.isInPrivilegedDocument(node)) {
        // If the node is in a priviledged document, we are in "chrome" context.
        return new ChromeWebElement(uuid);
      }
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
   * Unmarshals a JSON Object to one of {@link WebElement},
   * {@link WebWindow}, {@link WebFrame},
   * or {@link ChromeWebElement}.
   *
   * @param {Object.<string, string>} json
   *     Web element reference, which is supposed to be a JSON Object
   *     where the key is one of the {@link WebReference} concrete
   *     classes' UUID identifiers.
   *
   * @return {WebReference}
   *     Representation of the web element.
   *
   * @throws {InvalidArgumentError}
   *     If <var>json</var> is not a web element reference.
   */
  static fromJSON(json) {
    lazy.assert.object(json);
    if (json instanceof WebReference) {
      return json;
    }
    let keys = Object.keys(json);

    for (let key of keys) {
      switch (key) {
        case ContentShadowRoot.Identifier:
          return ContentShadowRoot.fromJSON(json);

        case WebElement.Identifier:
          return WebElement.fromJSON(json);

        case WebFrame.Identifier:
          return WebFrame.fromJSON(json);

        case WebWindow.Identifier:
          return WebWindow.fromJSON(json);

        case ChromeWebElement.Identifier:
          return ChromeWebElement.fromJSON(json);
      }
    }

    throw new lazy.error.InvalidArgumentError(
      lazy.pprint`Expected web element reference, got: ${json}`
    );
  }

  /**
   * Constructs a {@link WebElement} or {@link ChromeWebElement}
   * or {@link ContentShadowRoot} from a a string <var>uuid</var>.
   *
   * This whole function is a workaround for the fact that clients
   * to Marionette occasionally pass <code>{id: <uuid>}</code> JSON
   * Objects instead of web element representations.  For that reason
   * we need the <var>context</var> argument to determine what kind of
   * {@link WebReference} to return.
   *
   * @param {string} uuid
   *     UUID to be associated with the web element.
   * @param {Context} context
   *     Context, which is used to determine if the returned type
   *     should be a content web element or a chrome web element.
   *
   * @return {WebReference}
   *     One of {@link WebElement} or {@link ChromeWebElement},
   *     based on <var>context</var>.
   *
   * @throws {InvalidArgumentError}
   *     If <var>uuid</var> is not a string or <var>context</var>
   *     is an invalid context.
   */
  static fromUUID(uuid, context) {
    lazy.assert.string(uuid);

    switch (context) {
      case "chrome":
        return new ChromeWebElement(uuid);

      case "content":
        return new WebElement(uuid);

      default:
        throw new lazy.error.InvalidArgumentError(
          "Unknown context: " + context
        );
    }
  }

  /**
   * Checks if <var>ref<var> is a {@link WebReference} reference,
   * i.e. if it has {@link WebElement.Identifier}, or
   * {@link ChromeWebElement.Identifier} as properties.
   *
   * @param {Object.<string, string>} obj
   *     Object that represents a reference to a {@link WebReference}.
   * @return {boolean}
   *     True if <var>obj</var> is a {@link WebReference}, false otherwise.
   */
  static isReference(obj) {
    if (Object.prototype.toString.call(obj) != "[object Object]") {
      return false;
    }

    if (
      ContentShadowRoot.Identifier in obj ||
      WebElement.Identifier in obj ||
      WebFrame.Identifier in obj ||
      WebWindow.Identifier in obj ||
      ChromeWebElement.Identifier in obj
    ) {
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
    let uuid = Services.uuid.generateUUID().toString();
    return uuid.substring(1, uuid.length - 1);
  }
}

/**
 * DOM elements are represented as web elements when they are
 * transported over the wire protocol.
 */
class WebElement extends WebReference {
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
}
WebElement.Identifier = "element-6066-11e4-a52e-4f735466cecf";

/**
 * Shadow Root elements are represented as web elements when they are
 * transported over the wire protocol
 */
class ContentShadowRoot extends WebReference {
  toJSON() {
    return { [ContentShadowRoot.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    const { Identifier } = ContentShadowRoot;

    if (!(Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        lazy.pprint`Expected shadow root reference, got: ${json}`
      );
    }

    let uuid = json[Identifier];
    return new ContentShadowRoot(uuid);
  }
}
ContentShadowRoot.Identifier = "shadow-6066-11e4-a52e-4f735466cecf";

/**
 * Top-level browsing contexts, such as <code>WindowProxy</code>
 * whose <code>opener</code> is null, are represented as web windows
 * over the wire protocol.
 */
class WebWindow extends WebReference {
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
class WebFrame extends WebReference {
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

/**
 * XUL elements in chrome space are represented as chrome web elements
 * over the wire protocol.
 */
class ChromeWebElement extends WebReference {
  toJSON() {
    return { [ChromeWebElement.Identifier]: this.uuid };
  }

  static fromJSON(json) {
    if (!(ChromeWebElement.Identifier in json)) {
      throw new lazy.error.InvalidArgumentError(
        "Expected chrome element reference " +
          lazy.pprint`for XUL element, got: ${json}`
      );
    }
    let uuid = json[ChromeWebElement.Identifier];
    return new ChromeWebElement(uuid);
  }
}
ChromeWebElement.Identifier = "chromeelement-9fc5-4b51-a3c8-01716eedeb04";
