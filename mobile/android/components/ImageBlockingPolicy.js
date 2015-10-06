/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, manager: Cm, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

/**
 * Content policy for blocking images
 */

// SVG placeholder image for blocked image content
var PLACEHOLDER_IMG = "chrome://browser/skin/images/placeholder_image.svg";

function ImageBlockingPolicy() {}

ImageBlockingPolicy.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPolicy]),
  classDescription: "Click-To-Play Image",
  classID: Components.ID("{f55f77f9-d33d-4759-82fc-60db3ee0bb91}"),
  contractID: "@mozilla.org/browser/blockimages-policy;1",
  xpcom_categories: [{category: "content-policy", service: true}],

  // nsIContentPolicy interface implementation
  shouldLoad: function(contentType, contentLocation, requestOrigin, node, mimeTypeGuess, extra) {
    if (!getEnabled()) {
      return Ci.nsIContentPolicy.ACCEPT;
    }

    if (contentType === Ci.nsIContentPolicy.TYPE_IMAGE || contentType === Ci.nsIContentPolicy.TYPE_IMAGESET) {
      // Accept any non-http(s) image URLs
      if (!contentLocation.schemeIs("http") && !contentLocation.schemeIs("https")) {
        return Ci.nsIContentPolicy.ACCEPT;
      }

      if (node instanceof Ci.nsIDOMHTMLImageElement) {
        // Accept if the user has asked to view the image
        if (node.getAttribute("data-ctv-show") == "true") {
          return Ci.nsIContentPolicy.ACCEPT;
        }

        setTimeout(() => {
          // Cache the original image URL and swap in our placeholder
          node.setAttribute("data-ctv-src", contentLocation.spec);
          node.setAttribute("src", PLACEHOLDER_IMG);

          // For imageset (img + srcset) the "srcset" is used even after we reset the "src" causing a loop.
          // We are given the final image URL anyway, so it's OK to just remove the "srcset" value.
          node.removeAttribute("srcset");
        }, 0);
      }

      // Reject any image that is not associated with a DOM element
      return Ci.nsIContentPolicy.REJECT;
    }

    // Accept all other content types
    return Ci.nsIContentPolicy.ACCEPT;
  },

  shouldProcess: function(contentType, contentLocation, requestOrigin, node, mimeTypeGuess, extra) {
    return Ci.nsIContentPolicy.ACCEPT;
  },

};

function getEnabled() {
  return Services.prefs.getBoolPref("browser.image_blocking.enabled");
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ImageBlockingPolicy]);
