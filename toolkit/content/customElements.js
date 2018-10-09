/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals MozQueryInterface */

"use strict";

// This is loaded into chrome windows with the subscript loader. Wrap in
// a block to prevent accidentally leaking globals onto `window`.
{

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

// The listener of DOMContentLoaded must be set on window, rather than
// document, because the window can go away before the event is fired.
// In that case, we don't want to initialize anything, otherwise we
// may be leaking things because they will never be destroyed after.
let gIsDOMContentLoaded = false;
const gElementsPendingConnection = new Set();
window.addEventListener("DOMContentLoaded", () => {
  gIsDOMContentLoaded = true;
  for (let element of gElementsPendingConnection) {
    try {
      if (element.isConnected) {
        element.connectedCallback();
      }
    } catch (ex) { console.error(ex); }
  }
  gElementsPendingConnection.clear();
}, { once: true, capture: true });

const gXULDOMParser = new DOMParser();
gXULDOMParser.forceEnableXULXBL();

class MozXULElement extends XULElement {
  /**
   * Sometimes an element may not want to run connectedCallback logic during
   * parse. This could be because we don't want to initialize the element before
   * the element's contents have been fully parsed, or for performance reasons.
   * If you'd like to opt-in to this, then add this to the beginning of your
   * `connectedCallback` and `disconnectedCallback`:
   *
   *    if (this.delayConnectedCallback()) { return }
   *
   * And this at the beginning of your `attributeChangedCallback`
   *
   *    if (!this.isConnectedAndReady) { return; }
   */
  delayConnectedCallback() {
    if (gIsDOMContentLoaded) {
      return false;
    }
    gElementsPendingConnection.add(this);
    return true;
  }

  get isConnectedAndReady() {
    return gIsDOMContentLoaded && this.isConnected;
  }

  /**
   * Allows eager deterministic construction of XUL elements with XBL attached, by
   * parsing an element tree and returning a DOM fragment to be inserted in the
   * document before any of the inner elements is referenced by JavaScript.
   *
   * This process is required instead of calling the createElement method directly
   * because bindings get attached when:
   *
   * 1. the node gets a layout frame constructed, or
   * 2. the node gets its JavaScript reflector created, if it's in the document,
   *
   * whichever happens first. The createElement method would return a JavaScript
   * reflector, but the element wouldn't be in the document, so the node wouldn't
   * get XBL attached. After that point, even if the node is inserted into a
   * document, it won't get XBL attached until either the frame is constructed or
   * the reflector is garbage collected and the element is touched again.
   *
   * @param {string} str
   *        String with the XML representation of XUL elements.
   * @param {string[]} [entities]
   *        An array of DTD URLs containing entity definitions.
   *
   * @return {DocumentFragment} `DocumentFragment` instance containing
   *         the corresponding element tree, including element nodes
   *         but excluding any text node.
   */
  static parseXULToFragment(str, entities = []) {
    let doc = gXULDOMParser.parseFromString(`
      ${entities.length ? `<!DOCTYPE bindings [
        ${entities.reduce((preamble, url, index) => {
          return preamble + `<!ENTITY % _dtd-${index} SYSTEM "${url}">
            %_dtd-${index};
            `;
        }, "")}
      ]>` : ""}
      <box xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul">
        ${str}
      </box>
    `, "application/xml");
    // The XUL/XBL parser is set to ignore all-whitespace nodes, whereas (X)HTML
    // does not do this. Most XUL code assumes that the whitespace has been
    // stripped out, so we simply remove all text nodes after using the parser.
    let nodeIterator = doc.createNodeIterator(doc, NodeFilter.SHOW_TEXT);
    let currentNode = nodeIterator.nextNode();
    while (currentNode) {
      currentNode.remove();
      currentNode = nodeIterator.nextNode();
    }
    // We use a range here so that we don't access the inner DOM elements from
    // JavaScript before they are imported and inserted into a document.
    let range = doc.createRange();
    range.selectNodeContents(doc.querySelector("box"));
    return range.extractContents();
  }

  /**
   * Insert a localization link to an FTL file. This is used so that
   * a Custom Element can wait to inject the link until it's connected,
   * and so that consuming documents don't require the correct <link>
   * present in the markup.
   *
   * @param path
   *        The path to the FTL file
   */
  static insertFTLIfNeeded(path) {
    let container = document.head || document.querySelector("linkset");
    if (!container) {
      if (document.contentType == "application/vnd.mozilla.xul+xml") {
        container = document.createXULElement("linkset");
        document.documentElement.appendChild(container);
      } else if (document.documentURI == AppConstants.BROWSER_CHROME_URL) {
        // Special case for browser.xhtml. Here `document.head` is null, so
        // just insert the link at the end of the window.
        container = document.documentElement;
      } else {
        throw new Error("Attempt to inject localization link before document.head is available");
      }
    }

    for (let link of container.querySelectorAll("link")) {
      if (link.getAttribute("href") == path) {
        return;
      }
    }

    let link = document.createElement("link");
    link.setAttribute("rel", "localization");
    link.setAttribute("href", path);

    container.appendChild(link);
  }

  /**
   * Indicate that a class defining a XUL element implements one or more
   * XPCOM interfaces by adding a getCustomInterface implementation to it.
   *
   * The supplied class should implement the properties and methods of
   * all of the interfaces that are specified.
   *
   * @param cls
   *        The class that implements the interface.
   * @param names
   *        Array of interface names.
   */
  static implementCustomInterface(cls, ifaces) {
    const numbers = new Set(ifaces.map(i => i.number));
    if (cls.prototype.customInterfaceNumbers) {
      // Base class already implemented some interfaces. Inherit:
      cls.prototype.customInterfaceNumbers.forEach(number => numbers.add(number));
    }

    cls.prototype.customInterfaceNumbers = numbers;
    cls.prototype.getCustomInterfaceCallback = function getCustomInterfaceCallback(iface) {
      if (numbers.has(iface.number)) {
        return getInterfaceProxy(this);
      }
      return null;
    };
  }
}

/**
 * Given an object, add a proxy that reflects interface implementations
 * onto the object itself.
 */
function getInterfaceProxy(obj) {
  if (!obj._customInterfaceProxy) {
    obj._customInterfaceProxy = new Proxy(obj, {
      get(target, prop, receiver) {
        let propOrMethod = target[prop];
        if (typeof propOrMethod == "function") {
          if (propOrMethod instanceof MozQueryInterface) {
            return Reflect.get(target, prop, receiver);
          }
          return function(...args) {
            return propOrMethod.apply(target, args);
          };
        }
        return propOrMethod;
      },
    });
  }

  return obj._customInterfaceProxy;
}

class MozBaseControl extends MozXULElement {
  get disabled() {
    return this.getAttribute("disabled") == "true";
  }

  set disabled(val) {
    if (val) {
      this.setAttribute("disabled", "true");
    } else {
      this.removeAttribute("disabled");
    }
  }

  get tabIndex() {
    return parseInt(this.getAttribute("tabindex")) || 0;
  }

  set tabIndex(val) {
    if (val) {
      this.setAttribute("tabindex", val);
    } else {
      this.removeAttribute("tabindex");
    }
  }
}

MozXULElement.implementCustomInterface(MozBaseControl, [Ci.nsIDOMXULControlElement]);

// Attach the base class to the window so other scripts can use it:
window.MozXULElement = MozXULElement;
window.MozBaseControl = MozBaseControl;

// For now, don't load any elements in the extension dummy document.
// We will want to load <browser> when that's migrated (bug 1441935).
const isDummyDocument = document.documentURI == "chrome://extensions/content/dummy.xul";
if (!isDummyDocument) {
  for (let script of [
    "chrome://global/content/elements/general.js",
    "chrome://global/content/elements/progressmeter.js",
    "chrome://global/content/elements/radio.js",
    "chrome://global/content/elements/textbox.js",
    "chrome://global/content/elements/tabbox.js",
  ]) {
    Services.scriptloader.loadSubScript(script, window);
  }

  for (let [tag, script] of [
    ["findbar", "chrome://global/content/elements/findbar.js"],
    ["stringbundle", "chrome://global/content/elements/stringbundle.js"],
    ["printpreview-toolbar", "chrome://global/content/printPreviewToolbar.js"],
    ["editor", "chrome://global/content/elements/editor.js"],
  ]) {
    customElements.setElementCreationCallback(tag, () => {
      Services.scriptloader.loadSubScript(script, window);
    });
  }
}
}
