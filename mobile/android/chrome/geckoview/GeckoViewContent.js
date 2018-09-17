/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewContentModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  FormData: "resource://gre/modules/FormData.jsm",
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.jsm",
  PrivacyFilter: "resource://gre/modules/sessionstore/PrivacyFilter.jsm",
  ScrollPosition: "resource://gre/modules/ScrollPosition.jsm",
  Services: "resource://gre/modules/Services.jsm",
  SessionHistory: "resource://gre/modules/sessionstore/SessionHistory.jsm",
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
    this.messageManager.addMessageListener("GeckoView:SetActive",
                                           this);

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
    let domWindowUtils = content.windowUtils;
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

          addEventListener("load", _ => {
            const formdata = this._savedState.formdata;
            if (formdata) {
              FormData.restoreTree(content, formdata);
            }
          }, {capture: true, mozSystemGroup: true, once: true});

          addEventListener("pageshow", _ => {
            const scrolldata = this._savedState.scrolldata;
            if (scrolldata) {
              ScrollPosition.restoreTree(content, scrolldata);
            }
            delete this._savedState;
          }, {capture: true, mozSystemGroup: true, once: true});

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
    }
  }

  // eslint-disable-next-line complexity
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
      case "DOMFormHasPassword":
        this._addAutoFillElement(
            FormLikeFactory.createFromForm(aEvent.composedTarget));
        break;
      case "DOMInputPasswordAdded": {
        const input = aEvent.composedTarget;
        if (!input.form) {
          this._addAutoFillElement(FormLikeFactory.createFromField(input));
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
      case "focusin":
        if (aEvent.composedTarget instanceof content.HTMLInputElement) {
          this._onAutoFillFocus(aEvent.composedTarget);
        }
        break;
      case "focusout":
        if (aEvent.composedTarget instanceof content.HTMLInputElement) {
          this._onAutoFillFocus(null);
        }
        break;
      case "pagehide":
        if (aEvent.target === content.document) {
          this._clearAutoFillElements();
        }
        break;
      case "pageshow":
        if (aEvent.target === content.document && aEvent.persisted) {
          this._scanAutoFillDocument(aEvent.target);
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

  /**
   * Process an auto-fillable form and send the relevant details of the form
   * to Java. Multiple calls within a short time period for the same form are
   * coalesced, so that, e.g., if multiple inputs are added to a form in
   * succession, we will only perform one processing pass. Note that for inputs
   * without forms, FormLikeFactory treats the document as the "form", but
   * there is no difference in how we process them.
   *
   * @param aFormLike A FormLike object produced by FormLikeFactory.
   * @param aFromDeferredTask Signal that this call came from our internal
   *                          coalescing task. Other caller must not use this
   *                          parameter.
   */
  _addAutoFillElement(aFormLike, aFromDeferredTask) {
    let task = this._autoFillTasks &&
               this._autoFillTasks.get(aFormLike.rootElement);
    if (task && !aFromDeferredTask) {
      // We already have a pending task; cancel that and start a new one.
      debug `Canceling previous auto-fill task`;
      task.disarm();
      task = null;
    }

    if (!task) {
      if (aFromDeferredTask) {
        // Canceled before we could run the task.
        debug `Auto-fill task canceled`;
        return;
      }
      // Start a new task so we can coalesce adding elements in one batch.
      debug `Deferring auto-fill task`;
      task = new DeferredTask(
          () => this._addAutoFillElement(aFormLike, true), 100);
      task.arm();
      if (!this._autoFillTasks) {
        this._autoFillTasks = new WeakMap();
      }
      this._autoFillTasks.set(aFormLike.rootElement, task);
      return;
    }

    debug `Adding auto-fill ${aFormLike}`;

    this._autoFillTasks.delete(aFormLike.rootElement);
    this._autoFillId = this._autoFillId || 0;

    if (!this._autoFillInfos) {
      this._autoFillInfos = new WeakMap();
      this._autoFillElements = new Map();
    }

    let sendFocusEvent = false;
    const getInfo = (element, parent) => {
      let info = this._autoFillInfos.get(element);
      if (info) {
        return info;
      }
      info = {
        id: ++this._autoFillId,
        parent,
        tag: element.tagName,
        type: element instanceof content.HTMLInputElement ? element.type : null,
        editable: (element instanceof content.HTMLInputElement) &&
                  ["color", "date", "datetime-local", "email", "month",
                   "number", "password", "range", "search", "tel", "text",
                   "time", "url", "week"].includes(element.type),
        disabled: element instanceof content.HTMLInputElement ? element.disabled
                                                              : null,
        attributes: Object.assign({}, ...Array.from(element.attributes)
            .filter(attr => attr.localName !== "value")
            .map(attr => ({[attr.localName]: attr.value}))),
        origin: element.ownerDocument.location.origin,
      };
      this._autoFillInfos.set(element, info);
      this._autoFillElements.set(info.id, Cu.getWeakReference(element));
      sendFocusEvent |= (element === element.ownerDocument.activeElement);
      return info;
    };

    const rootInfo = getInfo(aFormLike.rootElement, null);
    rootInfo.children = aFormLike.elements.map(
        element => getInfo(element, rootInfo.id));

    this.eventDispatcher.dispatch("GeckoView:AddAutoFill", rootInfo, {
      onSuccess: responses => {
        // `responses` is an object with IDs as keys.
        debug `Performing auto-fill ${responses}`;

        const AUTOFILL_STATE = "-moz-autofill";
        const winUtils = content.windowUtils;

        for (let id in responses) {
          const entry = this._autoFillElements &&
                        this._autoFillElements.get(+id);
          const element = entry && entry.get();
          const value = responses[id] || "";

          if (element instanceof content.HTMLInputElement &&
              !element.disabled && element.parentElement) {
            element.value = value;

            // Fire both "input" and "change" events.
            element.dispatchEvent(new element.ownerGlobal.Event(
                "input", { bubbles: true }));
            element.dispatchEvent(new element.ownerGlobal.Event(
                "change", { bubbles: true }));

            if (winUtils && element.value === value) {
              // Add highlighting for autofilled fields.
              winUtils.addManuallyManagedState(element, AUTOFILL_STATE);

              // Remove highlighting when the field is changed.
              element.addEventListener("input", _ =>
                  winUtils.removeManuallyManagedState(element, AUTOFILL_STATE),
                  { mozSystemGroup: true, once: true });
            }

          } else if (element) {
            warn `Don't know how to auto-fill ${element.tagName}`;
          }
        }
      },
      onError: error => {
        warn `Cannot perform autofill ${error}`;
      },
    });

    if (sendFocusEvent) {
      // We might have missed sending a focus event for the active element.
      this._onAutoFillFocus(aFormLike.ownerDocument.activeElement);
    }
  }

  /**
   * Called when an auto-fillable field is focused or blurred.
   *
   * @param aTarget Focused element, or null if an element has lost focus.
   */
  _onAutoFillFocus(aTarget) {
    debug `Auto-fill focus on ${aTarget && aTarget.tagName}`;

    let info = aTarget && this._autoFillInfos &&
               this._autoFillInfos.get(aTarget);
    if (!aTarget || info) {
      this.eventDispatcher.dispatch("GeckoView:OnAutoFillFocus", info);
    }
  }

  /**
   * Clear all tracked auto-fill forms and notify Java.
   */
  _clearAutoFillElements() {
    debug `Clearing auto-fill`;

    this._autoFillTasks = undefined;
    this._autoFillInfos = undefined;
    this._autoFillElements = undefined;

    this.eventDispatcher.sendRequest({
      type: "GeckoView:ClearAutoFill",
    });
  }

  /**
   * Scan for auto-fillable forms and add them if necessary. Called when a page
   * is navigated to through history, in which case we don't get our typical
   * "input added" notifications.
   *
   * @param aDoc Document to scan.
   */
  _scanAutoFillDocument(aDoc) {
    // Add forms first; only check forms with password inputs.
    const inputs = aDoc.querySelectorAll("input[type=password]");
    let inputAdded = false;
    for (let i = 0; i < inputs.length; i++) {
      if (inputs[i].form) {
        // Let _addAutoFillElement coalesce multiple calls for the same form.
        this._addAutoFillElement(
            FormLikeFactory.createFromForm(inputs[i].form));
      } else if (!inputAdded) {
        // Treat inputs without forms as one unit, and process them only once.
        inputAdded = true;
        this._addAutoFillElement(
            FormLikeFactory.createFromField(inputs[i]));
      }
    }

    // Finally add frames.
    const frames = aDoc.defaultView.frames;
    for (let i = 0; i < frames.length; i++) {
      this._scanAutoFillDocument(frames[i].document);
    }
  }
}

let {debug, warn} = GeckoViewContent.initLogging("GeckoViewContent");
let module = GeckoViewContent.create(this);
