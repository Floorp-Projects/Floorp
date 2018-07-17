/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["findAllCssSelectors", "findCssSelector"];

/**
 * Traverse getBindingParent until arriving upon the bound element
 * responsible for the generation of the specified node.
 * See https://developer.mozilla.org/en-US/docs/XBL/XBL_1.0_Reference/DOM_Interfaces#getBindingParent.
 *
 * @param {DOMNode} node
 * @return {DOMNode}
 *         If node is not anonymous, this will return node. Otherwise,
 *         it will return the bound element
 *
 */
function getRootBindingParent(node) {
  let doc = node.ownerDocument;
  if (!doc) {
    return node;
  }

  let parent;
  while ((parent = doc.getBindingParent(node))) {
    node = parent;
  }
  return node;
}

/**
 * Return the node's parent shadow root if the node in shadow DOM, null
 * otherwise.
 */
function getShadowRoot(node) {
  let doc = node.ownerDocument;
  if (!doc) {
    return null;
  }

  const parent = doc.getBindingParent(node);
  const shadowRoot = parent && parent.openOrClosedShadowRoot;
  if (shadowRoot) {
    return shadowRoot;
  }

  return null;
}

/**
 * Find the position of [element] in [nodeList].
 * @returns an index of the match, or -1 if there is no match
 */
function positionInNodeList(element, nodeList) {
  for (let i = 0; i < nodeList.length; i++) {
    if (element === nodeList[i]) {
      return i;
    }
  }
  return -1;
}

/**
 * For a provided node, find the appropriate container/node couple so that
 * container.contains(node) and a CSS selector can be created from the
 * container to the node.
 */
function findNodeAndContainer(node) {
  const shadowRoot = getShadowRoot(node);
  if (shadowRoot) {
    // If the node is under a shadow root, the shadowRoot contains the node and
    // we can find the node via shadowRoot.querySelector(path).
    return {
      containingDocOrShadow: shadowRoot,
      node
    };
  }

  // Otherwise, get the root binding parent to get a non anonymous element that
  // will be accessible from the ownerDocument.
  const bindingParent = getRootBindingParent(node);
  return {
    containingDocOrShadow: bindingParent.ownerDocument,
    node: bindingParent
  };
}

/**
 * Find a unique CSS selector for a given element
 * @returns a string such that:
 *   - ele.containingDocOrShadow.querySelector(reply) === ele
 *   - ele.containingDocOrShadow.querySelectorAll(reply).length === 1
 */
const findCssSelector = function(ele) {
  const { node, containingDocOrShadow } = findNodeAndContainer(ele);
  ele = node;

  if (!containingDocOrShadow || !containingDocOrShadow.contains(ele)) {
    // findCssSelector received element not inside document.
    return "";
  }

  let cssEscape = ele.ownerGlobal.CSS.escape;

  // document.querySelectorAll("#id") returns multiple if elements share an ID
  if (ele.id &&
      containingDocOrShadow.querySelectorAll("#" + cssEscape(ele.id)).length === 1) {
    return "#" + cssEscape(ele.id);
  }

  // Inherently unique by tag name
  let tagName = ele.localName;
  if (tagName === "html") {
    return "html";
  }
  if (tagName === "head") {
    return "head";
  }
  if (tagName === "body") {
    return "body";
  }

  // We might be able to find a unique class name
  let selector, index, matches;
  for (let i = 0; i < ele.classList.length; i++) {
    // Is this className unique by itself?
    selector = "." + cssEscape(ele.classList.item(i));
    matches = containingDocOrShadow.querySelectorAll(selector);
    if (matches.length === 1) {
      return selector;
    }
    // Maybe it's unique with a tag name?
    selector = cssEscape(tagName) + selector;
    matches = containingDocOrShadow.querySelectorAll(selector);
    if (matches.length === 1) {
      return selector;
    }
    // Maybe it's unique using a tag name and nth-child
    index = positionInNodeList(ele, ele.parentNode.children) + 1;
    selector = selector + ":nth-child(" + index + ")";
    matches = containingDocOrShadow.querySelectorAll(selector);
    if (matches.length === 1) {
      return selector;
    }
  }

  // Not unique enough yet.
  index = positionInNodeList(ele, ele.parentNode.children) + 1;
  selector = cssEscape(tagName) + ":nth-child(" + index + ")";
  if (ele.parentNode !== containingDocOrShadow) {
    selector = findCssSelector(ele.parentNode) + " > " + selector;
  }
  return selector;
};

/**
 * If the element is in a frame or under a shadowRoot, return the corresponding
 * element.
 */
function getSelectorParent(node) {
  const shadowRoot = getShadowRoot(node);
  if (shadowRoot) {
    // The element is in a shadowRoot, return the host component.
    return shadowRoot.host;
  }

  // Otherwise return the parent frameElement.
  return node.ownerGlobal.frameElement;
}

/**
 * Retrieve the array of CSS selectors corresponding to the provided node.
 *
 * The selectors are ordered starting with the root document and ending with the deepest
 * nested frame. Additional items are used if the node is inside a frame or a shadow root,
 * each representing the CSS selector for finding the frame or root element in its parent
 * document.
 *
 * This format is expected by DevTools in order to handle the Inspect Node context menu
 * item.
 *
 * @param  {node}
 *         The node for which the CSS selectors should be computed
 * @return {Array}
 *         An array of CSS selectors to find the target node. Several selectors can be
 *         needed if the element is nested in frames and not directly in the root
 *         document. The selectors are ordered starting with the root document and
 *         ending with the deepest nested frame or shadow root.
 */
const findAllCssSelectors = function(node) {
  let selectors = [];
  while (node) {
    selectors.unshift(findCssSelector(node));
    node = getSelectorParent(node);
  }

  return selectors;
};
