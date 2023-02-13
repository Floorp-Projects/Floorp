/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["BackgroundThumbnailsChild"];

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "PageThumbUtils",
  "resource://gre/modules/PageThumbUtils.jsm"
);

// NOTE: Copied from nsSandboxFlags.h
/**
 * This flag prevents content from creating new auxiliary browsing contexts,
 * e.g. using the target attribute, or the window.open() method.
 */
const SANDBOXED_AUXILIARY_NAVIGATION = 0x2;

class BackgroundThumbnailsChild extends JSWindowActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "Browser:Thumbnail:ContentInfo": {
        if (
          message.data.isImage ||
          this.contentWindow.ImageDocument.isInstance(this.document)
        ) {
          // To avoid sending additional messages between processes, we return
          // the image data directly with the size info.
          return lazy.PageThumbUtils.createImageThumbnailCanvas(
            this.contentWindow,
            this.document.location,
            message.data.targetWidth,
            message.data.backgroundColor
          );
        }

        let [width, height] = lazy.PageThumbUtils.getContentSize(
          this.contentWindow
        );
        return { width, height };
      }

      case "Browser:Thumbnail:LoadURL": {
        let docShell = this.docShell.QueryInterface(Ci.nsIWebNavigation);

        // We want a low network priority for this service - lower than b/g tabs
        // etc - so set it to the lowest priority available.
        docShell
          .QueryInterface(Ci.nsIDocumentLoader)
          .loadGroup.QueryInterface(Ci.nsISupportsPriority).priority =
          Ci.nsISupportsPriority.PRIORITY_LOWEST;

        docShell.allowMedia = false;
        docShell.allowPlugins = false;
        docShell.allowContentRetargeting = false;
        let defaultFlags =
          Ci.nsIRequest.LOAD_ANONYMOUS |
          Ci.nsIRequest.LOAD_BYPASS_CACHE |
          Ci.nsIRequest.INHIBIT_CACHING |
          Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_HISTORY;
        docShell.defaultLoadFlags = defaultFlags;
        this.browsingContext.sandboxFlags |= SANDBOXED_AUXILIARY_NAVIGATION;
        docShell.useTrackingProtection = true;

        // Get the document to force a content viewer to be created, otherwise
        // the first load can fail.
        if (!this.document) {
          return false;
        }

        let loadURIOptions = {
          // Bug 1498603 verify usages of systemPrincipal here
          triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
          loadFlags: Ci.nsIWebNavigation.LOAD_FLAGS_STOP_CONTENT,
        };
        try {
          docShell.loadURI(message.data.url, loadURIOptions);
        } catch (ex) {
          return false;
        }

        return true;
      }
    }

    return undefined;
  }

  handleEvent(event) {
    if (event.type == "DOMDocElementInserted") {
      // Arrange to prevent (most) popup dialogs for this window - popups done
      // in the parent (eg, auth) aren't prevented, but alert() etc are.
      // disableDialogs only works on the current inner window, so it has
      // to be called every page load, but before scripts run.
      this.contentWindow.windowUtils.disableDialogs();
    }
  }
}
