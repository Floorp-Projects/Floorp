/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["ContentMetaChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

// Debounce time in milliseconds - this should be long enough to account for
// sync script tags that could appear between desired meta tags
const TIMEOUT_DELAY = 1000;

const ACCEPTED_PROTOCOLS = ["http:", "https:"];

// Possible description tags, listed in order from least favourable to most favourable
const DESCRIPTION_RULES = [
  "twitter:description",
  "description",
  "og:description",
];

// Possible image tags, listed in order from least favourable to most favourable
const PREVIEW_IMAGE_RULES = [
  "thumbnail",
  "twitter:image",
  "og:image",
  "og:image:url",
  "og:image:secure_url",
];

/*
 * Checks if the incoming meta tag has a greater score than the current best
 * score by checking the index of the meta tag in the list of rules provided.
 *
 * @param {Array} aRules
 *          The list of rules for a given type of meta tag
 * @param {String} aTag
 *          The name or property of the incoming meta tag
 * @param {String} aEntry
 *          The current best entry for the given meta tag
 *
 * @returns {Boolean} true if the incoming meta tag is better than the current
 *                    best meta tag of that same kind, false otherwise
 */
function shouldExtractMetadata(aRules, aTag, aEntry) {
  return aRules.indexOf(aTag) > aEntry.currMaxScore;
}

/*
 * Ensure that the preview image URL is safe and valid before storing
 *
 * @param {URL} aURL
 *          A URL object that needs to be checked for valid principal and protocol
 *
 * @returns {Boolean} true if the preview URL is safe and can be stored, false otherwise
 */
function checkLoadURIStr(aURL) {
  if (!ACCEPTED_PROTOCOLS.includes(aURL.protocol)) {
    return false;
  }
  try {
    let ssm = Services.scriptSecurityManager;
    let principal = ssm.createNullPrincipal({});
    ssm.checkLoadURIStrWithPrincipal(
      principal,
      aURL.href,
      ssm.DISALLOW_INHERIT_PRINCIPAL
    );
  } catch (e) {
    return false;
  }
  return true;
}

/*
 * This listens to DOMMetaAdded events and collects relevant metadata about the
 * meta tag received. Then, it sends the metadata gathered from the meta tags
 * and the url of the page as it's payload to be inserted into moz_places.
 */
class ContentMetaChild extends JSWindowActorChild {
  constructor() {
    super();

    // Store a mapping of the best description and preview
    // image collected so far for a given URL.
    this.metaTags = new Map();
  }

  didDestroy() {
    for (let entry of this.metaTags.values()) {
      entry.timeout.cancel();
    }
  }

  handleEvent(event) {
    if (event.type != "DOMMetaAdded") {
      return;
    }

    const metaTag = event.originalTarget;
    const window = metaTag.ownerGlobal;

    // If there's no meta tag, ignore this. Also verify that the window
    // matches just to be safe.
    if (!metaTag || !metaTag.ownerDocument || window != this.contentWindow) {
      return;
    }

    const url = metaTag.ownerDocument.documentURI;

    let name = metaTag.name;
    let prop = metaTag.getAttributeNS(null, "property");
    if (!name && !prop) {
      return;
    }

    let tag = name || prop;

    const entry = this.metaTags.get(url) || {
      description: { value: null, currMaxScore: -1 },
      image: { value: null, currMaxScore: -1 },
      timeout: null,
    };

    // Malformed meta tag - do not store it
    const content = metaTag.getAttributeNS(null, "content");
    if (!content) {
      return;
    }

    if (shouldExtractMetadata(DESCRIPTION_RULES, tag, entry.description)) {
      // Extract the description
      entry.description.value = content;
      entry.description.currMaxScore = DESCRIPTION_RULES.indexOf(tag);
    } else if (shouldExtractMetadata(PREVIEW_IMAGE_RULES, tag, entry.image)) {
      // Extract the preview image
      let value;
      try {
        value = new URL(content, url);
      } catch (e) {
        return;
      }
      if (value && checkLoadURIStr(value)) {
        entry.image.value = value.href;
        entry.image.currMaxScore = PREVIEW_IMAGE_RULES.indexOf(tag);
      }
    } else {
      // We don't care about other meta tags
      return;
    }

    if (!this.metaTags.has(url)) {
      this.metaTags.set(url, entry);
    }

    if (entry.timeout) {
      entry.timeout.delay = TIMEOUT_DELAY;
    } else {
      // We want to debounce incoming meta tags until we're certain we have the
      // best one for description and preview image, and only store that one
      entry.timeout = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      entry.timeout.initWithCallback(
        () => {
          entry.timeout = null;
          this.metaTags.delete(url);
          // We try to cancel the timers when we get destroyed, but if
          // there's a race, catch it:
          if (!this.manager || this.manager.isClosed) {
            return;
          }

          // Save description and preview image to moz_places
          this.sendAsyncMessage("Meta:SetPageInfo", {
            url,
            description: entry.description.value,
            previewImageURL: entry.image.value,
          });

          // Telemetry for recording the size of page metadata
          let metadataSize = entry.description.value
            ? entry.description.value.length
            : 0;
          metadataSize += entry.image.value ? entry.image.value.length : 0;
          Services.telemetry
            .getHistogramById("PAGE_METADATA_SIZE")
            .add(metadataSize);
        },
        TIMEOUT_DELAY,
        Ci.nsITimer.TYPE_ONE_SHOT
      );
    }
  }
}
