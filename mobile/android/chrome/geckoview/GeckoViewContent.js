/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewContentModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
  SessionHistory: "resource://gre/modules/sessionstore/SessionHistory.jsm",
  FormData: "resource://gre/modules/FormData.jsm",
  PrivacyFilter: "resource://gre/modules/sessionstore/PrivacyFilter.jsm",
  ScrollPosition: "resource://gre/modules/ScrollPosition.jsm",
});

class GeckoViewContent extends GeckoViewContentModule {
  onInit() {
    debug `onInit`;

    // We don't load this in the global namespace because
    // a Utils.jsm in a11y will clobber us.
    XPCOMUtils.defineLazyModuleGetters(this, {
      Utils: "resource://gre/modules/sessionstore/Utils.jsm",
    });

    this.messageManager.addMessageListener("GeckoView:SaveState",
                                           this);
    this.messageManager.addMessageListener("GeckoView:RestoreState",
                                           this);
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenEntered",
                                           this);
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenExited",
                                           this);
    this.messageManager.addMessageListener("GeckoView:ZoomToInput",
                                           this);
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
  }

  collectSessionState() {
    let history = SessionHistory.collect(docShell);
    let [formdata, scrolldata] = this.Utils.mapFrameTree(content, FormData.collect, ScrollPosition.collect);

    // Save the current document resolution.
    let zoom = { value: 1 };
    let domWindowUtils = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
    domWindowUtils.getResolution(zoom);
    scrolldata = scrolldata || {};
    scrolldata.zoom = {};
    scrolldata.zoom.resolution = zoom.value;

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

  receiveMessage(aMsg) {
    debug `receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "GeckoView:DOMFullscreenEntered":
        if (content) {
          content.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindowUtils)
                 .handleFullscreenRequests();
        }
        break;

      case "GeckoView:DOMFullscreenExited":
        if (content) {
          content.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindowUtils)
                 .exitFullscreen();
        }
        break;

      case "GeckoView:ZoomToInput": {
        let dwu = content.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils);

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

      case "GeckoView:SaveState":
        if (this._savedState) {
          // Short circuit and return the pending state if we're in the process of restoring
          sendAsyncMessage("GeckoView:SaveStateFinish", {state: JSON.stringify(this._savedState), id: aMsg.data.id});
        } else {
          try {
            let state = this.collectSessionState();
            sendAsyncMessage("GeckoView:SaveStateFinish", {
              state: state ? JSON.stringify(state) : null,
              id: aMsg.data.id
            });
          } catch (e) {
            sendAsyncMessage("GeckoView:SaveStateFinish", {
              error: e.message,
              id: aMsg.data.id
            });
          }
        }
        break;

      case "GeckoView:RestoreState":
        this._savedState = JSON.parse(aMsg.data.state);

        if (this._savedState.history) {
          let restoredHistory = SessionHistory.restore(docShell, this._savedState.history);

          addEventListener("load", this, {capture: true, mozSystemGroup: true, once: true});
          addEventListener("pageshow", this, {capture: true, mozSystemGroup: true, once: true});

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
    }
  }

  handleEvent(aEvent) {
    debug `handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "contextmenu":
        function nearestParentHref(node) {
          while (node && !node.href) {
            node = node.parentNode;
          }
          return node && node.href;
        }

        const node = aEvent.composedTarget;
        const hrefNode = nearestParentHref(node);
        const elementType = ChromeUtils.getClassName(node);
        const isImage = elementType === "HTMLImageElement";
        const isMedia = elementType === "HTMLVideoElement" ||
                        elementType === "HTMLAudioElement";

        if (hrefNode || isImage || isMedia) {
          this.eventDispatcher.sendRequest({
            type: "GeckoView:ContextMenu",
            screenX: aEvent.screenX,
            screenY: aEvent.screenY,
            uri: hrefNode,
            elementType,
            elementSrc: (isImage || isMedia)
                        ? node.currentSrc || node.src
                        : null
          });
          aEvent.preventDefault();
        }
        break;
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
          title: content.document.title
        });
        break;
      case "DOMWindowFocus":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMWindowFocus"
        });
        break;
      case "DOMWindowClose":
        if (!aEvent.isTrusted) {
          return;
        }

        aEvent.preventDefault();
        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMWindowClose"
        });
        break;
      case "load": {
        const formdata = this._savedState.formdata;
        if (formdata) {
          FormData.restoreTree(content, formdata);
        }
        break;
      }
      case "pageshow": {
        const scrolldata = this._savedState.scrolldata;
        if (scrolldata) {
          ScrollPosition.restoreTree(content, scrolldata);
        }
        delete this._savedState;
        break;
      }
    }
  }

  // WebProgress event handler.
  onLocationChange(aWebProgress, aRequest, aLocationURI, aFlags) {
    debug `onLocationChange`;

    if (this._savedState) {
      const scrolldata = this._savedState.scrolldata;
      if (scrolldata && scrolldata.zoom && scrolldata.zoom.displaySize) {
        let utils = content.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
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

let {debug, warn} = GeckoViewContent.initLogging("GeckoViewContent");
let module = GeckoViewContent.create(this);
