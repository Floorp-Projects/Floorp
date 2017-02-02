/**
 * Provides infrastructure for tests that would require mock document.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["MockDocument"]

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.importGlobalProperties(["URL"]);

const MockDocument = {
  /**
   * Create a document for the given URL containing the given HTML with the ownerDocument of all <form>s having a mocked location.
   */
  createTestDocument(aDocumentURL, aContent = "<form>", aType = "text/html") {
    let parser = Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(Ci.nsIDOMParser);
    parser.init();
    let parsedDoc = parser.parseFromString(aContent, aType);

    for (let element of parsedDoc.forms) {
      this.mockOwnerDocumentProperty(element, parsedDoc, aDocumentURL);
    }
    return parsedDoc;
  },

  mockOwnerDocumentProperty(aElement, aDoc, aURL) {
    // Mock the document.location object so we can unit test without a frame. We use a proxy
    // instead of just assigning to the property since it's not configurable or writable.
    let document = new Proxy(aDoc, {
      get(target, property, receiver) {
        // document.location is normally null when a document is outside of a "browsing context".
        // See https://html.spec.whatwg.org/#the-location-interface
        if (property == "location") {
          return new URL(aURL);
        }
        return target[property];
      },
    });

    // Assign element.ownerDocument to the proxy so document.location works.
    Object.defineProperty(aElement, "ownerDocument", {
      value: document,
    });
  },

};

