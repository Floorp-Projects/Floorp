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

// This needs to match ScreenLength.java
const SCREEN_LENGTH_TYPE_PIXEL = 0;
const SCREEN_LENGTH_TYPE_VISUAL_VIEWPORT_WIDTH = 1;
const SCREEN_LENGTH_TYPE_VISUAL_VIEWPORT_HEIGHT = 2;
const SCREEN_LENGTH_DOCUMENT_WIDTH = 3;
const SCREEN_LENGTH_DOCUMENT_HEIGHT = 4;

// This need to match PanZoomController.java
const SCROLL_BEHAVIOR_SMOOTH = 0;
const SCROLL_BEHAVIOR_AUTO = 1;

XPCOMUtils.defineLazyModuleGetters(this, {
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  ManifestObtainer: "resource://gre/modules/ManifestObtainer.jsm",
  PrivacyFilter: "resource://gre/modules/sessionstore/PrivacyFilter.jsm",
  SessionHistory: "resource://gre/modules/sessionstore/SessionHistory.jsm",
  Utils: "resource://gre/modules/sessionstore/Utils.jsm",
});

var EXPORTED_SYMBOLS = ["GeckoViewContentChild"];

class GeckoViewContentChild extends GeckoViewActorChild {
  toPixels(aLength, aType) {
    const { contentWindow } = this;
    if (aType === SCREEN_LENGTH_TYPE_PIXEL) {
      return aLength;
    } else if (aType === SCREEN_LENGTH_TYPE_VISUAL_VIEWPORT_WIDTH) {
      return aLength * contentWindow.visualViewport.width;
    } else if (aType === SCREEN_LENGTH_TYPE_VISUAL_VIEWPORT_HEIGHT) {
      return aLength * contentWindow.visualViewport.height;
    } else if (aType === SCREEN_LENGTH_DOCUMENT_WIDTH) {
      return aLength * contentWindow.document.body.scrollWidth;
    } else if (aType === SCREEN_LENGTH_DOCUMENT_HEIGHT) {
      return aLength * contentWindow.document.body.scrollHeight;
    }

    return aLength;
  }

  toScrollBehavior(aBehavior) {
    const { contentWindow } = this;
    if (!contentWindow) {
      return 0;
    }
    const { windowUtils } = contentWindow;
    if (aBehavior === SCROLL_BEHAVIOR_SMOOTH) {
      return windowUtils.SCROLL_MODE_SMOOTH;
    } else if (aBehavior === SCROLL_BEHAVIOR_AUTO) {
      return windowUtils.SCROLL_MODE_INSTANT;
    }
    return windowUtils.SCROLL_MODE_SMOOTH;
  }

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

  collectSessionState() {
    const { docShell, contentWindow } = this;
    const history = SessionHistory.collect(docShell);
    let formdata = SessionStoreUtils.collectFormData(contentWindow);
    let scrolldata = SessionStoreUtils.collectScrollPosition(contentWindow);

    // Save the current document resolution.
    let zoom = 1;
    const domWindowUtils = contentWindow.windowUtils;
    zoom = domWindowUtils.getResolution();
    scrolldata = scrolldata || {};
    scrolldata.zoom = {};
    scrolldata.zoom.resolution = zoom;

    // Save some data that'll help in adjusting the zoom level
    // when restoring in a different screen orientation.
    const displaySize = {};
    const width = {},
      height = {};
    domWindowUtils.getContentViewerSize(width, height);

    displaySize.width = width.value;
    displaySize.height = height.value;

    scrolldata.zoom.displaySize = displaySize;

    formdata = PrivacyFilter.filterFormData(formdata || {});

    return { history, formdata, scrolldata };
  }

  loadEntry(loadOptions, history) {
    if (!loadOptions) {
      history.QueryInterface(Ci.nsISHistory).reloadCurrentEntry();
      return;
    }

    const webNavigation = this.docShell.QueryInterface(Ci.nsIWebNavigation);

    const {
      referrerInfo,
      triggeringPrincipal,
      uri,
      flags,
      csp,
      headers,
    } = loadOptions;

    webNavigation.loadURI(uri, {
      triggeringPrincipal: E10SUtils.deserializePrincipal(triggeringPrincipal),
      referrerInfo: E10SUtils.deserializeReferrerInfo(referrerInfo),
      loadFlags: flags,
      csp: E10SUtils.deserializeCSP(csp),
      headers,
    });
  }

  receiveMessage(message) {
    const { name } = message;
    debug`receiveMessage: ${name}`;

    switch (name) {
      case "GeckoView:DOMFullscreenEntered":
        this.contentWindow?.windowUtils.handleFullscreenRequests();
        break;
      case "GeckoView:DOMFullscreenExited":
        this.contentWindow?.windowUtils.exitFullscreen();
        break;
      case "GeckoView:ZoomToInput": {
        const { contentWindow } = this;
        const dwu = contentWindow.windowUtils;

        const zoomToFocusedInput = function() {
          if (!dwu.flushApzRepaints()) {
            dwu.zoomToFocusedInput();
            return;
          }
          Services.obs.addObserver(function apzFlushDone() {
            Services.obs.removeObserver(apzFlushDone, "apz-repaints-flushed");
            dwu.zoomToFocusedInput();
          }, "apz-repaints-flushed");
        };

        const { force } = message.data;

        let gotResize = false;
        const onResize = function() {
          gotResize = true;
          if (dwu.isMozAfterPaintPending) {
            contentWindow.addEventListener(
              "MozAfterPaint",
              () => zoomToFocusedInput(),
              { capture: true, once: true }
            );
          } else {
            zoomToFocusedInput();
          }
        };

        contentWindow.addEventListener("resize", onResize, { capture: true });

        // When the keyboard is displayed, we can get one resize event,
        // multiple resize events, or none at all. Try to handle all these
        // cases by allowing resizing within a set interval, and still zoom to
        // input if there is no resize event at the end of the interval.
        contentWindow.setTimeout(() => {
          contentWindow.removeEventListener("resize", onResize, {
            capture: true,
          });
          if (!gotResize && force) {
            onResize();
          }
        }, 500);
        break;
      }
      case "GeckoView:RestoreState": {
        const { contentWindow, docShell } = this;
        const { history, formdata, scrolldata, loadOptions } = message.data;

        if (history) {
          const restoredHistory = SessionHistory.restore(docShell, history);

          contentWindow.addEventListener(
            "load",
            _ => {
              if (formdata) {
                Utils.restoreFrameTreeData(
                  contentWindow,
                  formdata,
                  (frame, data) => {
                    // restore() will return false, and thus abort restoration for the
                    // current |frame| and its descendants, if |data.url| is given but
                    // doesn't match the loaded document's URL.
                    return SessionStoreUtils.restoreFormData(
                      frame.document,
                      data
                    );
                  }
                );
              }
            },
            { capture: true, mozSystemGroup: true, once: true }
          );

          const scrollRestore = _ => {
            if (contentWindow.location != "about:blank") {
              if (scrolldata) {
                Utils.restoreFrameTreeData(
                  contentWindow,
                  scrolldata,
                  (frame, data) => {
                    if (data.scroll) {
                      SessionStoreUtils.restoreScrollPosition(frame, data);
                    }
                  }
                );
              }
              contentWindow.removeEventListener("pageshow", scrollRestore, {
                capture: true,
                mozSystemGroup: true,
              });
            }
          };

          contentWindow.addEventListener("pageshow", scrollRestore, {
            capture: true,
            mozSystemGroup: true,
          });

          const progressFilter = Cc[
            "@mozilla.org/appshell/component/browser-status-filter;1"
          ].createInstance(Ci.nsIWebProgress);
          const flags = Ci.nsIWebProgress.NOTIFY_LOCATION;

          const progressListener = {
            QueryInterface: ChromeUtils.generateQI(["nsIWebProgressListener"]),

            onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
              debug`onLocationChange`;

              if (
                scrolldata &&
                scrolldata.zoom &&
                scrolldata.zoom.displaySize
              ) {
                const utils = contentWindow.windowUtils;
                // Restore zoom level.
                utils.setRestoreResolution(
                  scrolldata.zoom.resolution,
                  scrolldata.zoom.displaySize.width,
                  scrolldata.zoom.displaySize.height
                );
              }

              progressFilter.removeProgressListener(this);
              const webProgress = docShell
                .QueryInterface(Ci.nsIInterfaceRequestor)
                .getInterface(Ci.nsIWebProgress);
              webProgress.removeProgressListener(progressFilter);
            },
          };

          progressFilter.addProgressListener(progressListener, flags);
          const webProgress = docShell
            .QueryInterface(Ci.nsIInterfaceRequestor)
            .getInterface(Ci.nsIWebProgress);
          webProgress.addProgressListener(progressFilter, flags);

          this.loadEntry(loadOptions, restoredHistory);
        }
        break;
      }
      case "GeckoView:UpdateInitData": {
        // Provide a hook for native code to detect a transfer.
        Services.obs.notifyObservers(
          this.docShell,
          "geckoview-content-global-transferred"
        );
        break;
      }
      case "GeckoView:ScrollBy": {
        const x = {};
        const y = {};
        const { contentWindow } = this;
        const {
          widthValue,
          widthType,
          heightValue,
          heightType,
          behavior,
        } = message.data;
        contentWindow.windowUtils.getVisualViewportOffset(x, y);
        contentWindow.windowUtils.scrollToVisual(
          x.value + this.toPixels(widthValue, widthType),
          y.value + this.toPixels(heightValue, heightType),
          contentWindow.windowUtils.UPDATE_TYPE_MAIN_THREAD,
          this.toScrollBehavior(behavior)
        );
        break;
      }
      case "GeckoView:ScrollTo": {
        const { contentWindow } = this;
        const {
          widthValue,
          widthType,
          heightValue,
          heightType,
          behavior,
        } = message.data;
        contentWindow.windowUtils.scrollToVisual(
          this.toPixels(widthValue, widthType),
          this.toPixels(heightValue, heightType),
          contentWindow.windowUtils.UPDATE_TYPE_MAIN_THREAD,
          this.toScrollBehavior(behavior)
        );
        break;
      }
      case "CollectSessionState": {
        return this.collectSessionState();
      }
    }

    return null;
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
        const elementSrc =
          (isImage || isMedia) && (node.currentSrc || node.src);

        if (uri || isImage || isMedia) {
          const msg = {
            type: "GeckoView:ContextMenu",
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
      case "DOMTitleChanged":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMTitleChanged",
          title: this.contentWindow.document.title,
        });
        break;
      case "DOMWindowClose":
        if (!aEvent.isTrusted) {
          return;
        }

        aEvent.preventDefault();
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMWindowClose",
        });
        break;
      case "mozcaretstatechanged":
        if (
          aEvent.reason === "presscaret" ||
          aEvent.reason === "releasecaret"
        ) {
          this.eventDispatcher.sendRequest({
            type: "GeckoView:PinOnScreen",
            pinned: aEvent.reason === "presscaret",
          });
        }
        break;
      case "DOMContentLoaded": {
        if (aEvent.originalTarget.ownerGlobal == this.contentWindow) {
          // If loaded content doesn't have viewport-fit, parent still
          // uses old value of previous content.
          this.notifyParentOfViewportFit();
        }
        this.contentWindow.requestIdleCallback(async () => {
          const manifest = await ManifestObtainer.contentObtainManifest(
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
    }
  }
}

const { debug, warn } = GeckoViewContentChild.initLogging("GeckoViewContent"); // eslint-disable-line no-unused-vars
