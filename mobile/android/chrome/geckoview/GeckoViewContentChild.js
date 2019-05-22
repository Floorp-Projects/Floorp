/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {GeckoViewChildModule} = ChromeUtils.import("resource://gre/modules/GeckoViewChildModule.jsm");
var {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
var {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

// This needs to match ScreenLength.java
const SCREEN_LENGTH_TYPE_PIXEL = 0;
const SCREEN_LENGTH_TYPE_VIEWPORT_WIDTH = 1;
const SCREEN_LENGTH_TYPE_VIEWPORT_HEIGHT = 2;
const SCREEN_LENGTH_DOCUMENT_WIDTH = 3;
const SCREEN_LENGTH_DOCUMENT_HEIGHT = 4;

// This need to match PanZoomController.java
const SCROLL_BEHAVIOR_SMOOTH = 0;
const SCROLL_BEHAVIOR_AUTO = 1;

XPCOMUtils.defineLazyModuleGetters(this, {
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.jsm",
  GeckoViewAutoFill: "resource://gre/modules/GeckoViewAutoFill.jsm",
  ManifestObtainer: "resource://gre/modules/ManifestObtainer.jsm",
  PrivacyFilter: "resource://gre/modules/sessionstore/PrivacyFilter.jsm",
  SessionHistory: "resource://gre/modules/sessionstore/SessionHistory.jsm",
});

class GeckoViewContentChild extends GeckoViewChildModule {
  onInit() {
    debug `onInit`;

    // We don't load this in the global namespace because
    // a Utils.jsm in a11y will clobber us.
    XPCOMUtils.defineLazyModuleGetters(this, {
      Utils: "resource://gre/modules/sessionstore/Utils.jsm",
    });

    this.messageManager.addMessageListener("GeckoView:DOMFullscreenEntered",
                                           this);
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenExited",
                                           this);
    this.messageManager.addMessageListener("GeckoView:RestoreState", this);
    this.messageManager.addMessageListener("GeckoView:SetActive", this);
    this.messageManager.addMessageListener("GeckoView:UpdateInitData", this);
    this.messageManager.addMessageListener("GeckoView:ZoomToInput", this);
    this.messageManager.addMessageListener("GeckoView:ScrollBy", this);
    this.messageManager.addMessageListener("GeckoView:ScrollTo", this);

    const options = {
        mozSystemGroup: true,
        capture: false,
    };
    addEventListener("DOMFormHasPassword", this, options);
    addEventListener("DOMInputPasswordAdded", this, options);
    addEventListener("pagehide", this, options);
    addEventListener("pageshow", this, options);
    addEventListener("focusin", this, options);
    addEventListener("focusout", this, options);
    addEventListener("mozcaretstatechanged", this, options);

    XPCOMUtils.defineLazyGetter(this, "_autoFill", () =>
        new GeckoViewAutoFill(this.eventDispatcher));

    // Notify WebExtension process script that this tab is ready for extension content to load.
    Services.obs.notifyObservers(this.messageManager, "tab-content-frameloader-created");
  }

  onEnable() {
    debug `onEnable`;

    addEventListener("DOMTitleChanged", this, false);
    addEventListener("DOMWindowFocus", this, false);
    addEventListener("DOMWindowClose", this, false);
    addEventListener("MozDOMFullscreen:Entered", this, false);
    addEventListener("MozDOMFullscreen:Exit", this, false);
    addEventListener("MozDOMFullscreen:Exited", this, false);
    addEventListener("MozDOMFullscreen:Request", this, false);
    addEventListener("contextmenu", this, { capture: true });
    addEventListener("DOMContentLoaded", this, false);
  }

  onDisable() {
    debug `onDisable`;

    removeEventListener("DOMTitleChanged", this);
    removeEventListener("DOMWindowFocus", this);
    removeEventListener("DOMWindowClose", this);
    removeEventListener("MozDOMFullscreen:Entered", this);
    removeEventListener("MozDOMFullscreen:Exit", this);
    removeEventListener("MozDOMFullscreen:Exited", this);
    removeEventListener("MozDOMFullscreen:Request", this);
    removeEventListener("contextmenu", this, { capture: true });
    removeEventListener("DOMContentLoaded", this);
  }

  collectSessionState() {
    let history = SessionHistory.collect(docShell);
    let formdata = SessionStoreUtils.collectFormData(content);
    let scrolldata = SessionStoreUtils.collectScrollPosition(content);

    // Save the current document resolution.
    let zoom = 1;
    let domWindowUtils = content.windowUtils;
    zoom = domWindowUtils.getResolution();
    scrolldata = scrolldata || {};
    scrolldata.zoom = {};
    scrolldata.zoom.resolution = zoom;

    // Save some data that'll help in adjusting the zoom level
    // when restoring in a different screen orientation.
    let displaySize = {};
    let width = {}, height = {};
    domWindowUtils.getContentViewerSize(width, height);

    displaySize.width = width.value;
    displaySize.height = height.value;

    scrolldata.zoom.displaySize = displaySize;

    formdata = PrivacyFilter.filterFormData(formdata || {});

    return {history, formdata, scrolldata};
  }

  toPixels(aLength, aType) {
    if (aType === SCREEN_LENGTH_TYPE_PIXEL) {
      return aLength;
    } else if (aType === SCREEN_LENGTH_TYPE_VIEWPORT_WIDTH) {
      return aLength * content.innerWidth;
    } else if (aType === SCREEN_LENGTH_TYPE_VIEWPORT_HEIGHT) {
      return aLength * content.innerHeight;
    } else if (aType === SCREEN_LENGTH_DOCUMENT_WIDTH) {
      return aLength * content.document.body.scrollWidth;
    } else if (aType === SCREEN_LENGTH_DOCUMENT_HEIGHT) {
      return aLength * content.document.body.scrollHeight;
    }

    return aLength;
  }

  toScrollBehavior(aBehavior) {
    if (!content) {
      return 0;
    }
    if (aBehavior === SCROLL_BEHAVIOR_SMOOTH) {
      return content.windowUtils.SCROLL_MODE_SMOOTH;
    } else if (aBehavior === SCROLL_BEHAVIOR_AUTO) {
      return content.windowUtils.SCROLL_MODE_INSTANT;
    }
    return content.windowUtils.SCROLL_MODE_SMOOTH;
  }

  receiveMessage(aMsg) {
    debug `receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "GeckoView:DOMFullscreenEntered":
        if (content) {
          content.windowUtils
                 .handleFullscreenRequests();
        }
        break;

      case "GeckoView:DOMFullscreenExited":
        if (content) {
          content.windowUtils
                 .exitFullscreen();
        }
        break;
      case "GeckoView:ZoomToInput": {
        let dwu = content.windowUtils;

        let zoomToFocusedInput = function() {
          if (!dwu.flushApzRepaints()) {
            dwu.zoomToFocusedInput();
            return;
          }
          Services.obs.addObserver(function apzFlushDone() {
            Services.obs.removeObserver(apzFlushDone, "apz-repaints-flushed");
            dwu.zoomToFocusedInput();
          }, "apz-repaints-flushed");
        };

        let gotResize = false;
        let onResize = function() {
          gotResize = true;
          if (dwu.isMozAfterPaintPending) {
            addEventListener("MozAfterPaint", function paintDone() {
              removeEventListener("MozAfterPaint", paintDone, {capture: true});
              zoomToFocusedInput();
            }, {capture: true});
          } else {
            zoomToFocusedInput();
          }
        };

        addEventListener("resize", onResize, { capture: true });

        // When the keyboard is displayed, we can get one resize event,
        // multiple resize events, or none at all. Try to handle all these
        // cases by allowing resizing within a set interval, and still zoom to
        // input if there is no resize event at the end of the interval.
        content.setTimeout(() => {
          removeEventListener("resize", onResize, { capture: true });
          if (!gotResize) {
            onResize();
          }
        }, 500);
        break;
      }

      case "GeckoView:RestoreState":
        this._savedState = aMsg.data;

        if (this._savedState.history) {
          let restoredHistory = SessionHistory.restore(docShell, this._savedState.history);

          addEventListener("load", _ => {
            const formdata = this._savedState.formdata;
            if (formdata) {
              this.Utils.restoreFrameTreeData(content, formdata, (frame, data) => {
                // restore() will return false, and thus abort restoration for the
                // current |frame| and its descendants, if |data.url| is given but
                // doesn't match the loaded document's URL.
                return SessionStoreUtils.restoreFormData(frame.document, data);
              });
            }
          }, {capture: true, mozSystemGroup: true, once: true});

          let scrollRestore = _ => {
            if (content.location != "about:blank") {
              const scrolldata = this._savedState.scrolldata;
              if (scrolldata) {
                this.Utils.restoreFrameTreeData(content, scrolldata, (frame, data) => {
                  if (data.scroll) {
                    SessionStoreUtils.restoreScrollPosition(frame, data);
                  }
                });
              }
              delete this._savedState;
              removeEventListener("pageshow", scrollRestore);
            }
          };

          addEventListener("pageshow", scrollRestore, {capture: true, mozSystemGroup: true});

          if (!this.progressFilter) {
            this.progressFilter =
              Cc["@mozilla.org/appshell/component/browser-status-filter;1"]
              .createInstance(Ci.nsIWebProgress);
            this.flags = Ci.nsIWebProgress.NOTIFY_LOCATION;
          }

          this.progressFilter.addProgressListener(this, this.flags);
          let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                                    .getInterface(Ci.nsIWebProgress);
          webProgress.addProgressListener(this.progressFilter, this.flags);

          restoredHistory.QueryInterface(Ci.nsISHistory).reloadCurrentEntry();
        }
        break;

      case "GeckoView:SetActive":
          if (content && aMsg.data.suspendMedia) {
              content.windowUtils.mediaSuspend = aMsg.data.active ? Ci.nsISuspendedTypes.NONE_SUSPENDED : Ci.nsISuspendedTypes.SUSPENDED_PAUSE;
          }
        break;

      case "GeckoView:UpdateInitData":
        // Provide a hook for native code to detect a transfer.
        Services.obs.notifyObservers(
            docShell, "geckoview-content-global-transferred");
        break;
      case "GeckoView:ScrollBy":
        let x = {};
        let y = {};
        content.windowUtils.getVisualViewportOffset(x, y);
        content.windowUtils.scrollToVisual(
          x.value + this.toPixels(aMsg.data.widthValue, aMsg.data.widthType),
          y.value + this.toPixels(aMsg.data.heightValue, aMsg.data.heightType),
          content.windowUtils.UPDATE_TYPE_MAIN_THREAD,
          this.toScrollBehavior(aMsg.data.behavior)
        );
        break;
      case "GeckoView:ScrollTo":
        content.windowUtils.scrollToVisual(
          this.toPixels(aMsg.data.widthValue, aMsg.data.widthType),
          this.toPixels(aMsg.data.heightValue, aMsg.data.heightType),
          content.windowUtils.UPDATE_TYPE_MAIN_THREAD,
          this.toScrollBehavior(aMsg.data.behavior)
        );
        break;
    }
  }

  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    debug `handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "contextmenu":
        function nearestParentAttribute(aNode, aAttribute) {
          while (aNode && aNode.hasAttribute &&
                 !aNode.hasAttribute(aAttribute)) {
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
        const uri = createAbsoluteUri(baseUri,
          nearestParentAttribute(node, "href"));
        const title = nearestParentAttribute(node, "title");
        const alt = nearestParentAttribute(node, "alt");
        const elementType = ChromeUtils.getClassName(node);
        const isImage = elementType === "HTMLImageElement";
        const isMedia = elementType === "HTMLVideoElement" ||
                        elementType === "HTMLAudioElement";
        const elementSrc = (isImage || isMedia) && (node.currentSrc || node.src);

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
      case "DOMFormHasPassword":
        this._autoFill.addElement(
            FormLikeFactory.createFromForm(aEvent.composedTarget));
        break;
      case "DOMInputPasswordAdded": {
        const input = aEvent.composedTarget;
        if (!input.form) {
          this._autoFill.addElement(FormLikeFactory.createFromField(input));
        }
        break;
      }
      case "MozDOMFullscreen:Request":
        sendAsyncMessage("GeckoView:DOMFullscreenRequest");
        break;
      case "MozDOMFullscreen:Entered":
      case "MozDOMFullscreen:Exited":
        // Content may change fullscreen state by itself, and we should ensure
        // that the parent always exits fullscreen when content has left
        // full screen mode.
        if (content && content.document.fullscreenElement) {
          break;
        }
        // fall-through
      case "MozDOMFullscreen:Exit":
        sendAsyncMessage("GeckoView:DOMFullscreenExit");
        break;
      case "DOMTitleChanged":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMTitleChanged",
          title: content.document.title,
        });
        break;
      case "DOMWindowFocus":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMWindowFocus",
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
      case "focusin":
        if (aEvent.composedTarget instanceof content.HTMLInputElement) {
          this._autoFill.onFocus(aEvent.composedTarget);
        }
        break;
      case "focusout":
        if (aEvent.composedTarget instanceof content.HTMLInputElement) {
          this._autoFill.onFocus(null);
        }
        break;
      case "pagehide":
        if (aEvent.target === content.document) {
          this._autoFill.clearElements();
        }
        break;
      case "pageshow":
        if (aEvent.target === content.document && aEvent.persisted) {
          this._autoFill.scanDocument(aEvent.target);
        }
        break;
      case "mozcaretstatechanged":
        if (aEvent.reason === "presscaret" || aEvent.reason === "releasecaret") {
          this.eventDispatcher.sendRequest({
            type: "GeckoView:PinOnScreen",
            pinned: aEvent.reason === "presscaret",
          });
        }
        break;
      case "DOMContentLoaded": {
        content.requestIdleCallback(async () => {
          const manifest = await ManifestObtainer.contentObtainManifest(content);
          if (manifest) {
            this.eventDispatcher.sendRequest({
              type: "GeckoView:WebAppManifest",
              manifest,
            });
          }
        });
      }
    }
  }

  // WebProgress event handler.
  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    debug `onLocationChange`;

    if (this._savedState) {
      const scrolldata = this._savedState.scrolldata;
      if (scrolldata && scrolldata.zoom && scrolldata.zoom.displaySize) {
        let utils = content.windowUtils;
        // Restore zoom level.
        utils.setRestoreResolution(scrolldata.zoom.resolution,
                                   scrolldata.zoom.displaySize.width,
                                   scrolldata.zoom.displaySize.height);
      }
    }

    this.progressFilter.removeProgressListener(this);
    let webProgress = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIWebProgress);
    webProgress.removeProgressListener(this.progressFilter);
  }
}

const {debug, warn} = GeckoViewContentChild.initLogging("GeckoViewContent"); // eslint-disable-line no-unused-vars
const module = GeckoViewContentChild.create(this);
