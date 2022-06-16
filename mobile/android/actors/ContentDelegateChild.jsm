/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);

var { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ManifestObtainer: "resource://gre/modules/ManifestObtainer.jsm",
});

var EXPORTED_SYMBOLS = ["ContentDelegateChild"];

class ContentDelegateChild extends GeckoViewActorChild {
  notifyParentOfViewportFit() {
    if (this.triggerViewportFitChange) {
      this.contentWindow.cancelIdleCallback(this.triggerViewportFitChange);
    }
    this.triggerViewportFitChange = this.contentWindow.requestIdleCallback(
      () => {
        this.triggerViewportFitChange = null;
        const viewportFit = this.contentWindow.windowUtils.getViewportFitInfo();
        if (this.lastViewportFit === viewportFit) {
          return;
        }
        this.lastViewportFit = viewportFit;
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMMetaViewportFit",
          viewportfit: viewportFit,
        });
      }
    );
  }

  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "contextmenu": {
        function nearestParentAttribute(aNode, aAttribute) {
          while (
            aNode &&
            aNode.hasAttribute &&
            !aNode.hasAttribute(aAttribute)
          ) {
            aNode = aNode.parentNode;
          }
          return aNode && aNode.getAttribute && aNode.getAttribute(aAttribute);
        }

        function createAbsoluteUri(aBaseUri, aUri) {
          if (!aUri || !aBaseUri || !aBaseUri.displaySpec) {
            return null;
          }
          return Services.io.newURI(aUri, null, aBaseUri).displaySpec;
        }

        const node = aEvent.composedTarget;
        const baseUri = node.ownerDocument.baseURIObject;
        const uri = createAbsoluteUri(
          baseUri,
          nearestParentAttribute(node, "href")
        );
        const title = nearestParentAttribute(node, "title");
        const alt = nearestParentAttribute(node, "alt");
        const elementType = ChromeUtils.getClassName(node);
        const isImage = elementType === "HTMLImageElement";
        const isMedia =
          elementType === "HTMLVideoElement" ||
          elementType === "HTMLAudioElement";
        let elementSrc = (isImage || isMedia) && (node.currentSrc || node.src);
        if (elementSrc) {
          const isBlob = elementSrc.startsWith("blob:");
          if (isBlob && !URL.isValidURL(elementSrc)) {
            elementSrc = null;
          }
        }

        if (uri || isImage || isMedia) {
          const msg = {
            type: "GeckoView:ContextMenu",
            // We don't have full zoom on Android, so using CSS coordinates
            // here is fine, since the CSS coordinate spaces match between the
            // child and parent processes.
            screenX: aEvent.screenX,
            screenY: aEvent.screenY,
            baseUri: (baseUri && baseUri.displaySpec) || null,
            uri,
            title,
            alt,
            elementType,
            elementSrc: elementSrc || null,
          };

          this.eventDispatcher.sendRequest(msg);
          aEvent.preventDefault();
        }
        break;
      }
      case "MozDOMFullscreen:Request": {
        this.sendAsyncMessage("GeckoView:DOMFullscreenRequest", {});
        break;
      }
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited":
        // Content may change fullscreen state by itself, and we should ensure
        // that the parent always exits fullscreen when content has left
        // full screen mode.
        if (this.contentWindow?.document.fullscreenElement) {
          break;
        }
      // fall-through
      case "MozDOMFullscreen:Exit":
        this.sendAsyncMessage("GeckoView:DOMFullscreenExit", {});
        break;
      case "DOMMetaViewportFitChanged":
        if (aEvent.originalTarget.ownerGlobal == this.contentWindow) {
          this.notifyParentOfViewportFit();
        }
        break;
      case "DOMContentLoaded": {
        if (aEvent.originalTarget.ownerGlobal == this.contentWindow) {
          // If loaded content doesn't have viewport-fit, parent still
          // uses old value of previous content.
          this.notifyParentOfViewportFit();
        }
        if (this.contentWindow !== this.contentWindow?.top) {
          // Only check WebApp manifest on the top level window.
          return;
        }
        this.contentWindow.requestIdleCallback(async () => {
          const manifest = await lazy.ManifestObtainer.contentObtainManifest(
            this.contentWindow
          );
          if (manifest) {
            this.eventDispatcher.sendRequest({
              type: "GeckoView:WebAppManifest",
              manifest,
            });
          }
        });
        break;
      }
      case "MozFirstContentfulPaint": {
        this.eventDispatcher.sendRequest({
          type: "GeckoView:FirstContentfulPaint",
        });
        break;
      }
      case "MozPaintStatusReset": {
        this.eventDispatcher.sendRequest({
          type: "GeckoView:PaintStatusReset",
        });
        break;
      }
    }
  }
}

const { debug, warn } = ContentDelegateChild.initLogging(
  "ContentDelegateChild"
);
