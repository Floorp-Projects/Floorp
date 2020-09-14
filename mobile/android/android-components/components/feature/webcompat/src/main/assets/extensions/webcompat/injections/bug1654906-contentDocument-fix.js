"use strict";

/**
 * Bug 1654906 - Build site patch for reallygoodemails.com
 * WebCompat issue #16401 - https://webcompat.com/issues/55001
 *
 * The live email preview at reallygoodemails.com presumes that iframes
 * will never be in an uninitialized readyState, and so ends up setting
 * their innerHTML before they are ready. This intervention detects when
 * they call innerHTML for an uninitialized contentDocument on an iframe,
 * and waits for it to actually load before setting the innerHTML.
 */

/* globals exportFunction */

console.info(
  "iframe.contentDocument has been shimmed for compatibility reasons. See https://webcompat.com/issues/55001 for details."
);

var elemProto = window.wrappedJSObject.Element.prototype;
var innerHTML = Object.getOwnPropertyDescriptor(elemProto, "innerHTML");
var iFrameProto = window.wrappedJSObject.HTMLIFrameElement.prototype;
var oldIFrameContentDocumentProp = Object.getOwnPropertyDescriptor(
  iFrameProto,
  "contentDocument"
);
var uninitializedContentDocs = new Set();

Object.defineProperty(iFrameProto, "contentDocument", {
  get: exportFunction(function() {
    const frame = this;
    const contentDoc = oldIFrameContentDocumentProp.get.call(frame);
    let latestHTMLToSet; // in case innerHTML is set multiple times before load
    let waitingForLoad = false;
    if (contentDoc.readyState === "uninitialized") {
      if (!uninitializedContentDocs.has(contentDoc)) {
        uninitializedContentDocs.add(contentDoc);
        Object.defineProperty(contentDoc.documentElement, "innerHTML", {
          get: innerHTML.get,
          set: exportFunction(function(html) {
            latestHTMLToSet = html;
            if (waitingForLoad) {
              return;
            }
            if (contentDoc.readyState !== "uninitialized") {
              innerHTML.set.call(this, latestHTMLToSet);
              return;
            }
            waitingForLoad = true;
            frame.addEventListener(
              "load",
              () => {
                uninitializedContentDocs.delete(contentDoc);
                const { documentElement } = frame.contentDocument;
                innerHTML.set.call(documentElement, latestHTMLToSet);
                waitingForLoad = false;
              },
              { once: true }
            );
          }, window),
        });
      }
    }
    return contentDoc;
  }, window),

  set: oldIFrameContentDocumentProp.set,
});
