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
  PrivacyFilter: "resource://gre/modules/sessionstore/PrivacyFilter.jsm",
  SessionHistory: "resource://gre/modules/sessionstore/SessionHistory.jsm",
  Utils: "resource://gre/modules/sessionstore/Utils.jsm",
});

var EXPORTED_SYMBOLS = ["GeckoViewContentChild"];

class GeckoViewContentChild extends GeckoViewActorChild {
  actorCreated() {
    super.actorCreated();

    this.pageShow = new Promise(resolve => {
      this.receivedPageShow = resolve;
    });
  }

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
            contentWindow.windowRoot.addEventListener(
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
      case "RestoreSessionState": {
        this.restoreSessionState(message);
        break;
      }
      case "RestoreHistoryAndNavigate": {
        const { history, switchId } = message.data;
        if (history) {
          SessionHistory.restore(this.docShell, history);
          const historyIndex = history.requestedIndex - 1;
          const webNavigation = this.docShell.QueryInterface(
            Ci.nsIWebNavigation
          );

          if (!switchId) {
            // TODO: Bug 1648158 This won't work for Fission or HistoryInParent.
            webNavigation.sessionHistory.legacySHistory.reloadCurrentEntry();
          } else {
            webNavigation.resumeRedirectedLoad(switchId, historyIndex);
          }
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

  async restoreSessionState(message) {
    // Make sure we showed something before restoring scrolling and form data
    await this.pageShow;

    const { contentWindow } = this;
    const { formdata, scrolldata } = message.data;

    if (formdata) {
      Utils.restoreFrameTreeData(contentWindow, formdata, (frame, data) => {
        // restore() will return false, and thus abort restoration for the
        // current |frame| and its descendants, if |data.url| is given but
        // doesn't match the loaded document's URL.
        return SessionStoreUtils.restoreFormData(frame.document, data);
      });
    }

    if (scrolldata) {
      Utils.restoreFrameTreeData(contentWindow, scrolldata, (frame, data) => {
        if (data.scroll) {
          SessionStoreUtils.restoreScrollPosition(frame, data);
        }
      });
    }

    if (scrolldata && scrolldata.zoom && scrolldata.zoom.displaySize) {
      const utils = contentWindow.windowUtils;
      // Restore zoom level.
      utils.setRestoreResolution(
        scrolldata.zoom.resolution,
        scrolldata.zoom.displaySize.width,
        scrolldata.zoom.displaySize.height
      );
    }
  }

  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;
    if (!this.isContentWindow) {
      // This is not a GeckoView-controlled window
      return;
    }

    switch (aEvent.type) {
      case "pageshow": {
        this.receivedPageShow();
        break;
      }

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
    }
  }
}

const { debug, warn } = GeckoViewContentChild.initLogging("GeckoViewContent");
