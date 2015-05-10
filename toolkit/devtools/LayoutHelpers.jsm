/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
const Ci = Components.interfaces;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["LayoutHelpers"];

let LayoutHelpers = function(aTopLevelWindow) {
  this._topDocShell = aTopLevelWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                     .getInterface(Ci.nsIWebNavigation)
                                     .QueryInterface(Ci.nsIDocShell);
};

this.LayoutHelpers = LayoutHelpers;

LayoutHelpers.prototype = {

  /**
   * Get box quads adjusted for iframes and zoom level.
   * @param {DOMNode} node The node for which we are to get the box model region
   * quads.
   * @param {String} region The box model region to return: "content",
   * "padding", "border" or "margin".
   * @return {Array} An array of objects that have the same structure as quads
   * returned by getBoxQuads. An empty array if the node has no quads or is
   * invalid.
   */
  getAdjustedQuads: function(node, region) {
    if (!node || !node.getBoxQuads) {
      return [];
    }

    let quads = node.getBoxQuads({
      box: region
    });

    if (!quads.length) {
      return [];
    }

    let [xOffset, yOffset] = this.getFrameOffsets(node);
    let scale = LayoutHelpers.getCurrentZoom(node);

    let adjustedQuads = [];
    for (let quad of quads) {
      adjustedQuads.push({
        p1: {
          w: quad.p1.w * scale,
          x: quad.p1.x * scale + xOffset,
          y: quad.p1.y * scale + yOffset,
          z: quad.p1.z * scale
        },
        p2: {
          w: quad.p2.w * scale,
          x: quad.p2.x * scale + xOffset,
          y: quad.p2.y * scale + yOffset,
          z: quad.p2.z * scale
        },
        p3: {
          w: quad.p3.w * scale,
          x: quad.p3.x * scale + xOffset,
          y: quad.p3.y * scale + yOffset,
          z: quad.p3.z * scale
        },
        p4: {
          w: quad.p4.w * scale,
          x: quad.p4.x * scale + xOffset,
          y: quad.p4.y * scale + yOffset,
          z: quad.p4.z * scale
        },
        bounds: {
          bottom: quad.bounds.bottom * scale + yOffset,
          height: quad.bounds.height * scale,
          left: quad.bounds.left * scale + xOffset,
          right: quad.bounds.right * scale + xOffset,
          top: quad.bounds.top * scale + yOffset,
          width: quad.bounds.width * scale,
          x: quad.bounds.x * scale + xOffset,
          y: quad.bounds.y * scale + yOffset
        }
      });
    }

    return adjustedQuads;
  },

  /**
   * Compute the absolute position and the dimensions of a node, relativalely
   * to the root window.
   *
   * @param {DOMNode} aNode
   *        a DOM element to get the bounds for
   * @param {DOMWindow} aContentWindow
   *        the content window holding the node
   * @return {Object}
   *         A rect object with the {top, left, width, height} properties
   */
  getRect: function(aNode, aContentWindow) {
    let frameWin = aNode.ownerDocument.defaultView;
    let clientRect = aNode.getBoundingClientRect();

    // Go up in the tree of frames to determine the correct rectangle.
    // clientRect is read-only, we need to be able to change properties.
    let rect = {top: clientRect.top + aContentWindow.pageYOffset,
            left: clientRect.left + aContentWindow.pageXOffset,
            width: clientRect.width,
            height: clientRect.height};

    // We iterate through all the parent windows.
    while (true) {
      // Are we in the top-level window?
      if (this.isTopLevelWindow(frameWin)) {
        break;
      }

      let frameElement = this.getFrameElement(frameWin);
      if (!frameElement) {
        break;
      }

      // We are in an iframe.
      // We take into account the parent iframe position and its
      // offset (borders and padding).
      let frameRect = frameElement.getBoundingClientRect();

      let [offsetTop, offsetLeft] =
        this.getIframeContentOffset(frameElement);

      rect.top += frameRect.top + offsetTop;
      rect.left += frameRect.left + offsetLeft;

      frameWin = this.getParentWindow(frameWin);
    }

    return rect;
  },

  /**
   * Returns iframe content offset (iframe border + padding).
   * Note: this function shouldn't need to exist, had the platform provided a
   * suitable API for determining the offset between the iframe's content and
   * its bounding client rect. Bug 626359 should provide us with such an API.
   *
   * @param {DOMNode} aIframe
   *        The iframe.
   * @return {Array} [offsetTop, offsetLeft]
   *         offsetTop is the distance from the top of the iframe and the top of
   *         the content document.
   *         offsetLeft is the distance from the left of the iframe and the left
   *         of the content document.
   */
  getIframeContentOffset: function(aIframe) {
    let style = aIframe.contentWindow.getComputedStyle(aIframe, null);

    // In some cases, the computed style is null
    if (!style) {
      return [0, 0];
    }

    let paddingTop = parseInt(style.getPropertyValue("padding-top"));
    let paddingLeft = parseInt(style.getPropertyValue("padding-left"));

    let borderTop = parseInt(style.getPropertyValue("border-top-width"));
    let borderLeft = parseInt(style.getPropertyValue("border-left-width"));

    return [borderTop + paddingTop, borderLeft + paddingLeft];
  },

  /**
   * Find an element from the given coordinates. This method descends through
   * frames to find the element the user clicked inside frames.
   *
   * @param {DOMDocument} aDocument the document to look into.
   * @param {Number} aX
   * @param {Number} aY
   * @return {DOMNode}
   *         the element node found at the given coordinates, or null if no node
   *         was found
   */
  getElementFromPoint: function(aDocument, aX, aY) {
    let node = aDocument.elementFromPoint(aX, aY);
    if (node && node.contentDocument) {
      if (node instanceof Ci.nsIDOMHTMLIFrameElement) {
        let rect = node.getBoundingClientRect();

        // Gap between the iframe and its content window.
        let [offsetTop, offsetLeft] = this.getIframeContentOffset(node);

        aX -= rect.left + offsetLeft;
        aY -= rect.top + offsetTop;

        if (aX < 0 || aY < 0) {
          // Didn't reach the content document, still over the iframe.
          return node;
        }
      }
      if (node instanceof Ci.nsIDOMHTMLIFrameElement ||
          node instanceof Ci.nsIDOMHTMLFrameElement) {
        let subnode = this.getElementFromPoint(node.contentDocument, aX, aY);
        if (subnode) {
          node = subnode;
        }
      }
    }
    return node;
  },

  /**
   * Scroll the document so that the element "elem" appears vertically in
   * the viewport.
   *
   * @param {DOMNode} elem
   *        The element that needs to appear in the viewport.
   * @param {Boolean} centered
   *        true if you want it centered, false if you want it to appear on the
   *        top of the viewport. True by default, and that is usually what
   *        you want.
   */
  scrollIntoViewIfNeeded: function(elem, centered) {
    // We want to default to centering the element in the page,
    // so as to keep the context of the element.
    centered = centered === undefined ? true: !!centered;

    let win = elem.ownerDocument.defaultView;
    let clientRect = elem.getBoundingClientRect();

    // The following are always from the {top, bottom}
    // of the viewport, to the {top, …} of the box.
    // Think of them as geometrical vectors, it helps.
    // The origin is at the top left.

    let topToBottom = clientRect.bottom;
    let bottomToTop = clientRect.top - win.innerHeight;
    let yAllowed = true;  // We allow one translation on the y axis.

    // Whatever `centered` is, the behavior is the same if the box is
    // (even partially) visible.
    if ((topToBottom > 0 || !centered) && topToBottom <= elem.offsetHeight) {
      win.scrollBy(0, topToBottom - elem.offsetHeight);
      yAllowed = false;
    } else
    if ((bottomToTop < 0 || !centered) && bottomToTop >= -elem.offsetHeight) {
      win.scrollBy(0, bottomToTop + elem.offsetHeight);
      yAllowed = false;
    }

    // If we want it centered, and the box is completely hidden,
    // then we center it explicitly.
    if (centered) {
      if (yAllowed && (topToBottom <= 0 || bottomToTop >= 0)) {
        win.scroll(win.scrollX,
                   win.scrollY + clientRect.top
                   - (win.innerHeight - elem.offsetHeight) / 2);
      }
    }
  },

  /**
   * Check if a node and its document are still alive
   * and attached to the window.
   *
   * @param {DOMNode} aNode
   * @return {Boolean}
   */
  isNodeConnected: function(aNode) {
    try {
      let connected = (aNode.ownerDocument && aNode.ownerDocument.defaultView &&
                      !(aNode.compareDocumentPosition(aNode.ownerDocument.documentElement) &
                      aNode.DOCUMENT_POSITION_DISCONNECTED));
      return connected;
    } catch (e) {
      // "can't access dead object" error
      return false;
    }
  },

  /**
   * like win.parent === win, but goes through mozbrowsers and mozapps iframes.
   *
   * @param {DOMWindow} win
   * @return {Boolean}
   */
  isTopLevelWindow: function(win) {
    let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShell);

    return docShell === this._topDocShell;
  },

  /**
   * Check a window is part of the top level window.
   *
   * @param {DOMWindow} win
   * @return {Boolean}
   */
  isIncludedInTopLevelWindow: function LH_isIncludedInTopLevelWindow(win) {
    if (this.isTopLevelWindow(win)) {
      return true;
    }

    let parent = this.getParentWindow(win);
    if (!parent || parent === win) {
      return false;
    }

    return this.isIncludedInTopLevelWindow(parent);
  },

  /**
   * like win.parent, but goes through mozbrowsers and mozapps iframes.
   *
   * @param {DOMWindow} win
   * @return {DOMWindow}
   */
  getParentWindow: function(win) {
    if (this.isTopLevelWindow(win)) {
      return null;
    }

    let docShell = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIWebNavigation)
                   .QueryInterface(Ci.nsIDocShell);

    if (docShell.isBrowserOrApp) {
      let parentDocShell = docShell.getSameTypeParentIgnoreBrowserAndAppBoundaries();
      return parentDocShell ? parentDocShell.contentViewer.DOMDocument.defaultView : null;
    } else {
      return win.parent;
    }
  },

  /**
   * like win.frameElement, but goes through mozbrowsers and mozapps iframes.
   *
   * @param {DOMWindow} win
   *        The window to get the frame for
   * @return {DOMNode}
   *         The element in which the window is embedded.
   */
  getFrameElement: function(win) {
    if (this.isTopLevelWindow(win)) {
      return null;
    }

    let winUtils = win.
      QueryInterface(Components.interfaces.nsIInterfaceRequestor).
      getInterface(Components.interfaces.nsIDOMWindowUtils);

    return winUtils.containerElement;
  },

  /**
   * Get the x/y offsets for of all the parent frames of a given node
   *
   * @param {DOMNode} node
   *        The node for which we are to get the offset
   * @return {Array}
   *         The frame offset [x, y]
   */
  getFrameOffsets: function(node) {
    let xOffset = 0;
    let yOffset = 0;
    let frameWin = node.ownerDocument.defaultView;
    let scale = LayoutHelpers.getCurrentZoom(node);

    while (true) {
      // Are we in the top-level window?
      if (this.isTopLevelWindow(frameWin)) {
        break;
      }

      let frameElement = this.getFrameElement(frameWin);
      if (!frameElement) {
        break;
      }

      // We are in an iframe.
      // We take into account the parent iframe position and its
      // offset (borders and padding).
      let frameRect = frameElement.getBoundingClientRect();

      let [offsetTop, offsetLeft] =
        this.getIframeContentOffset(frameElement);

      xOffset += frameRect.left + offsetLeft;
      yOffset += frameRect.top + offsetTop;

      frameWin = this.getParentWindow(frameWin);
    }

    return [xOffset * scale, yOffset * scale];
  },

  /**
   * Get the 4 bounding points for a node taking iframes into account.
   * Note that for transformed nodes, this will return the untransformed bound.
   *
   * @param {DOMNode} node
   * @return {Object}
   *         An object with p1,p2,p3,p4 properties being {x,y} objects
   */
  getNodeBounds: function(node) {
    if (!node) {
      return;
    }

    let scale = LayoutHelpers.getCurrentZoom(node);

    // Find out the offset of the node in its current frame
    let offsetLeft = 0;
    let offsetTop = 0;
    let el = node;
    while (el && el.parentNode) {
      offsetLeft += el.offsetLeft;
      offsetTop += el.offsetTop;
      el = el.offsetParent;
    }

    // Also take scrolled containers into account
    el = node;
    while (el && el.parentNode) {
      if (el.scrollTop) {
        offsetTop -= el.scrollTop;
      }
      if (el.scrollLeft) {
        offsetLeft -= el.scrollLeft;
      }
      el = el.parentNode;
    }

    // And add the potential frame offset if the node is nested
    let [xOffset, yOffset] = this.getFrameOffsets(node);
    xOffset += offsetLeft;
    yOffset += offsetTop;

    xOffset *= scale;
    yOffset *= scale;

    // Get the width and height
    let width = node.offsetWidth * scale;
    let height = node.offsetHeight * scale;

    return {
      p1: {x: xOffset, y: yOffset},
      p2: {x: xOffset + width, y: yOffset},
      p3: {x: xOffset + width, y: yOffset + height},
      p4: {x: xOffset, y: yOffset + height}
    };
  }
};

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
LayoutHelpers.getRootBindingParent = function(node) {
  let parent;
  let doc = node.ownerDocument;
  if (!doc) {
    return node;
  }
  while ((parent = doc.getBindingParent(node))) {
    node = parent;
  }
  return node;
};

LayoutHelpers.getBindingParent = function(node) {
  let doc = node.ownerDocument;
  if (!doc) {
    return false;
  }

  // If there is no binding parent then it is not anonymous.
  let parent = doc.getBindingParent(node);
  if (!parent) {
    return false;
  }

  return parent;
}
/**
 * Determine whether a node is anonymous by determining if there
 * is a bindingParent.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 *
 */
LayoutHelpers.isAnonymous = function(node) {
  return LayoutHelpers.getRootBindingParent(node) !== node;
};

/**
 * Determine whether a node is native anonymous content (as opposed
 * to XBL anonymous or shadow DOM).
 * Native anonymous content includes elements like internals to form
 * controls and ::before/::after.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 *
 */
LayoutHelpers.isNativeAnonymous = function(node) {
  if (!LayoutHelpers.getBindingParent(node)) {
    return false;
  }
  return !LayoutHelpers.isXBLAnonymous(node) &&
         !LayoutHelpers.isShadowAnonymous(node);
};

/**
 * Determine whether a node is XBL anonymous content (as opposed
 * to native anonymous or shadow DOM).
 * See https://developer.mozilla.org/en-US/docs/XBL/XBL_1.0_Reference/Anonymous_Content.
 *
 * @param {DOMNode} node
 * @return {Boolean}
 *
 */
LayoutHelpers.isXBLAnonymous = function(node) {
  let parent = LayoutHelpers.getBindingParent(node);
  if (!parent) {
    return false;
  }

  // Shadow nodes also show up in getAnonymousNodes, so return false.
  if (parent.shadowRoot && parent.shadowRoot.contains(node)) {
    return false;
  }

  let anonNodes = [...node.ownerDocument.getAnonymousNodes(parent) || []];
  return anonNodes.indexOf(node) > -1;
};

/**
 * Determine whether a node is a child of a shadow root.
 * See https://w3c.github.io/webcomponents/spec/shadow/
 *
 * @param {DOMNode} node
 * @return {Boolean}
 */
LayoutHelpers.isShadowAnonymous = function(node) {
  let parent = LayoutHelpers.getBindingParent(node);
  if (!parent) {
    return false;
  }

  // If there is a shadowRoot and this is part of it then this
  // is not native anonymous
  return parent.shadowRoot && parent.shadowRoot.contains(node);
};

/**
 * Get the current zoom factor applied to the container window of a given node.
 * Container windows are used as a weakmap key to store the corresponding
 * nsIDOMWindowUtils instance to avoid querying it every time.
 *
 * @param {DOMNode|DOMWindow} The node for which the zoom factor should be
 * calculated, or its owner window.
 * @return {Number}
 */
let windowUtils = new WeakMap;
LayoutHelpers.getCurrentZoom = function(node) {
  let win = node.self === node ? node : node.ownerDocument.defaultView;
  let utils = windowUtils.get(win);
  if (utils) {
    return utils.fullZoom;
  }

  utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
             .getInterface(Ci.nsIDOMWindowUtils);
  windowUtils.set(win, utils);
  return utils.fullZoom;
};
