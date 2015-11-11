/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, manager: Cm, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

////////////////////////////////////////////////////////////////////////////////
//// Constants

//// SVG placeholder image for blocked image content
const PLACEHOLDER_IMG = "chrome://browser/skin/images/placeholder_image.svg";

//// Telemetry
const TELEMETRY_TAP_TO_LOAD_ENABLED = "TAP_TO_LOAD_ENABLED";
const TELEMETRY_SHOW_IMAGE_SIZE = "TAP_TO_LOAD_IMAGE_SIZE";
const TOPIC_GATHER_TELEMETRY = "gather-telemetry";

//// Gecko preference
const PREF_IMAGEBLOCKING_ENABLED = "browser.image_blocking.enabled";

//// Enabled options
const OPTION_NEVER = 0;
const OPTION_ALWAYS = 1;
const OPTION_WIFI_ONLY = 2;


/**
 * Content policy for blocking images
 */
function ImageBlockingPolicy() {
  Services.obs.addObserver(this, TOPIC_GATHER_TELEMETRY, false);
}

ImageBlockingPolicy.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPolicy, Ci.nsIObserver]),
  classDescription: "Click-To-Play Image",
  classID: Components.ID("{f55f77f9-d33d-4759-82fc-60db3ee0bb91}"),
  contractID: "@mozilla.org/browser/blockimages-policy;1",
  xpcom_categories: [{category: "content-policy", service: true}],

  // nsIContentPolicy interface implementation
  shouldLoad: function(contentType, contentLocation, requestOrigin, node, mimeTypeGuess, extra) {
    // When enabled or when on cellular, and option for cellular-only is selected
    if (this._enabled() == OPTION_NEVER || (this._enabled() == OPTION_WIFI_ONLY && this._usingCellular())) {
      if (contentType === Ci.nsIContentPolicy.TYPE_IMAGE || contentType === Ci.nsIContentPolicy.TYPE_IMAGESET) {
        // Accept any non-http(s) image URLs
        if (!contentLocation.schemeIs("http") && !contentLocation.schemeIs("https")) {
          return Ci.nsIContentPolicy.ACCEPT;
        }

        if (node instanceof Ci.nsIDOMHTMLImageElement) {
          // Accept if the user has asked to view the image
          if (node.getAttribute("data-ctv-show") == "true") {
            sendImageSizeTelemetry(node.getAttribute("data-ctv-src"));
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
    }

    // Accept all other content types
    return Ci.nsIContentPolicy.ACCEPT;
  },

  shouldProcess: function(contentType, contentLocation, requestOrigin, node, mimeTypeGuess, extra) {
    return Ci.nsIContentPolicy.ACCEPT;
  },

  _usingCellular: function() {
    let network = Cc["@mozilla.org/network/network-link-service;1"].getService(Ci.nsINetworkLinkService);
    return !(network.linkType == Ci.nsINetworkLinkService.LINK_TYPE_UNKNOWN ||
        network.linkType == Ci.nsINetworkLinkService.LINK_TYPE_ETHERNET ||
        network.linkType == Ci.nsINetworkLinkService.LINK_TYPE_USB  ||
        network.linkType == Ci.nsINetworkLinkService.LINK_TYPE_WIFI);
  },

  _enabled: function() {
    return Services.prefs.getIntPref("browser.image_blocking");
  },

  observe : function (subject, topic, data) {
    if (topic == TOPIC_GATHER_TELEMETRY) {
      Services.telemetry.getHistogramById(TELEMETRY_TAP_TO_LOAD_ENABLED).add(getEnabled());
    }
  },
};

function getEnabled() {
  return Services.prefs.getBoolPref(PREF_IMAGEBLOCKING_ENABLED);
}

function sendImageSizeTelemetry(imageURL) {
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
  xhr.open("HEAD", imageURL, true);
  xhr.onreadystatechange = function (e) {
    if (xhr.readyState != 4) {
      return;
    }
    if (xhr.status != 200) {
      return;
    }
    let contentLength = xhr.getResponseHeader("Content-Length");
    if (!contentLength) {
      return;
    }
    let imageSize = contentLength / 1024;
    Services.telemetry.getHistogramById(TELEMETRY_SHOW_IMAGE_SIZE).add(imageSize);
  };
  xhr.send(null);
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ImageBlockingPolicy]);
