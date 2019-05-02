/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This JS module wraps the nsIXULStore XPCOM service with useful abstractions.
// In particular, it wraps the enumerators returned by getIDsEnumerator()
// and getAttributeEnumerator() in JS objects that implement the iterable
// protocol.  It also implements the persist() method.  JS consumers should use
// this module rather than accessing nsIXULStore directly.

const EXPORTED_SYMBOLS = ["XULStore"];

// Services.xulStore loads this module and returns its `XULStore` symbol
// when this implementation of XULStore is enabled, so using it here
// would loop infinitely.  But the mozilla/use-services rule is a good
// requiremnt for every other consumer of XULStore.
// eslint-disable-next-line mozilla/use-services
const xulStore = Cc["@mozilla.org/xul/xulstore;1"].getService(Ci.nsIXULStore);

// Enables logging.
const debugMode = false;

// Internal function for logging debug messages to the Error Console window
function log(message) {
  if (!debugMode)
    return;
  console.log("XULStore: " + message);
}

const XULStore = {
  setValue: xulStore.setValue,
  hasValue: xulStore.hasValue,
  getValue: xulStore.getValue,
  removeValue: xulStore.removeValue,
  removeDocument: xulStore.removeDocument,

  /**
   * Sets a value for a specified node's attribute, except in
   * the case below (following the original XULDocument::persist):
   * If the value is empty and if calling `hasValue` with the node's
   * document and ID and `attr` would return true, then the
   * value instead gets removed from the store (see Bug 1476680).
   *
   * @param node - DOM node
   * @param attr - attribute to store
   */
  persist(node, attr) {
    if (!node.id) {
      throw new Error("Node without ID passed into persist()");
    }

    const uri = node.ownerDocument.documentURI;
    const value = node.getAttribute(attr);

    if (node.localName == "window") {
      log("Persisting attributes to windows is handled by nsXULWindow.");
      return;
    }

    // See Bug 1476680 - we could drop the `hasValue` check so that
    // any time there's an empty attribute it gets removed from the
    // store. Since this is copying behavior from document.persist,
    // callers would need to be updated with that change.
    if (!value && xulStore.hasValue(uri, node.id, attr)) {
      xulStore.removeValue(uri, node.id, attr);
    } else {
      xulStore.setValue(uri, node.id, attr, value);
    }
  },

  getIDsEnumerator(docURI) {
    return new XULStoreEnumerator(xulStore.getIDsEnumerator(docURI));
  },

  getAttributeEnumerator(docURI, id) {
    return new XULStoreEnumerator(xulStore.getAttributeEnumerator(docURI, id));
  },
};

class XULStoreEnumerator {
  constructor(enumerator) {
    this.enumerator = enumerator;
  }

  hasMore() {
    return this.enumerator.hasMore();
  }

  getNext() {
    return this.enumerator.getNext();
  }

  * [Symbol.iterator]() {
    while (this.enumerator.hasMore()) {
      yield (this.enumerator.getNext());
    }
  }
}
