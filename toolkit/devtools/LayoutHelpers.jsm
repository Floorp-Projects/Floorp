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

   * @param {DOMNode} node
   *        The node for which we are to get the box model region quads
   * @param  {String} region
   *         The box model region to return:
   *         "content", "padding", "border" or "margin"
   * @return {Object} An object that has the same structure as one quad returned
   *         by getBoxQuads
   */
  getAdjustedQuads: function(node, region) {
    if (!node || !node.getBoxQuads) {
      return null;
    }

    let [quads] = node.getBoxQuads({
      box: region
    });

    if (!quads) {
      return null;
    }

    let [xOffset, yOffset] = this.getFrameOffsets(node);
    let scale = this.calculateScale(node);

    return {
      p1: {
        w: quads.p1.w * scale,
        x: quads.p1.x * scale + xOffset,
        y: quads.p1.y * scale + yOffset,
        z: quads.p1.z * scale
      },
      p2: {
        w: quads.p2.w * scale,
        x: quads.p2.x * scale + xOffset,
        y: quads.p2.y * scale + yOffset,
        z: quads.p2.z * scale
      },
      p3: {
        w: quads.p3.w * scale,
        x: quads.p3.x * scale + xOffset,
        y: quads.p3.y * scale + yOffset,
        z: quads.p3.z * scale
      },
      p4: {
        w: quads.p4.w * scale,
        x: quads.p4.x * scale + xOffset,
        y: quads.p4.y * scale + yOffset,
        z: quads.p4.z * scale
      },
      bounds: {
        bottom: quads.bounds.bottom * scale + yOffset,
        height: quads.bounds.height * scale,
        left: quads.bounds.left * scale + xOffset,
        right: quads.bounds.right * scale + xOffset,
        top: quads.bounds.top * scale + yOffset,
        width: quads.bounds.width * scale,
        x: quads.bounds.x * scale + xOffset,
        y: quads.bounds.y * scale + yOffset
      }
    };
  },

  /**
   * Get the current zoom factor applied to the container window of a given node
   * @param {DOMNode}
   *        The node for which the zoom factor should be calculated
   * @return {Number}
   */
  calculateScale: function(node) {
    let win = node.ownerDocument.defaultView;
    let winUtils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);
    return winUtils.fullZoom;
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
   * Scroll the document so that the element "elem" appears in the viewport.
   *
   * @param {DOMNode} elem
   *        The element that needs to appear in the viewport.
   * @param {Boolean} centered
   *        true if you want it centered, false if you want it to appear on the
   *        top of the viewport. It is true by default, and that is usually what
   *        you want.
   */
  scrollIntoViewIfNeeded: function(elem, centered) {
    // We want to default to centering the element in the page,
    // so as to keep the context of the element.
    centered = centered === undefined? true: !!centered;

    let win = elem.ownerDocument.defaultView;
    let clientRect = elem.getBoundingClientRect();

    // The following are always from the {top, bottom, left, right}
    // of the viewport, to the {top, …} of the box.
    // Think of them as geometrical vectors, it helps.
    // The origin is at the top left.

    let topToBottom = clientRect.bottom;
    let bottomToTop = clientRect.top - win.innerHeight;
    let leftToRight = clientRect.right;
    let rightToLeft = clientRect.left - win.innerWidth;
    let xAllowed = true;  // We allow one translation on the x axis,
    let yAllowed = true;  // and one on the y axis.

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

    if ((leftToRight > 0 || !centered) && leftToRight <= elem.offsetWidth) {
      if (xAllowed) {
        win.scrollBy(leftToRight - elem.offsetWidth, 0);
        xAllowed = false;
      }
    } else
    if ((rightToLeft < 0 || !centered) && rightToLeft >= -elem.offsetWidth) {
      if (xAllowed) {
        win.scrollBy(rightToLeft + elem.offsetWidth, 0);
        xAllowed = false;
      }
    }

    // If we want it centered, and the box is completely hidden,
    // then we center it explicitly.

    if (centered) {

      if (yAllowed && (topToBottom <= 0 || bottomToTop >= 0)) {
        win.scroll(win.scrollX,
                   win.scrollY + clientRect.top
                   - (win.innerHeight - elem.offsetHeight) / 2);
      }

      if (xAllowed && (leftToRight <= 0 || rightToLeft <= 0)) {
        win.scroll(win.scrollX + clientRect.left
                   - (win.innerWidth - elem.offsetWidth) / 2,
                   win.scrollY);
      }
    }

    if (!this.isTopLevelWindow(win)) {
      // We are inside an iframe.
      let frameElement = this.getFrameElement(win);
      this.scrollIntoViewIfNeeded(frameElement, centered);
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
    let scale = this.calculateScale(node);

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

    let scale = this.calculateScale(node);

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
