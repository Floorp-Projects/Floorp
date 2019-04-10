/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewContent"];

const {GeckoViewModule} = ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

class GeckoViewContent extends GeckoViewModule {
  onInit() {
    this.registerListener([
      "GeckoViewContent:ExitFullScreen",
      "GeckoView:ClearMatches",
      "GeckoView:DisplayMatches",
      "GeckoView:FindInPage",
      "GeckoView:RestoreState",
      "GeckoView:SetActive",
      "GeckoView:SetFocused",
      "GeckoView:ZoomToInput",
      "GeckoView:ScrollBy",
      "GeckoView:ScrollTo",
    ]);
  }

  onEnable() {
    this.window.addEventListener("MozDOMFullscreen:Entered", this,
      /* capture */ true, /* untrusted */ false);
    this.window.addEventListener("MozDOMFullscreen:Exited", this,
      /* capture */ true, /* untrusted */ false);

    this.messageManager.addMessageListener("GeckoView:DOMFullscreenExit", this);
    this.messageManager.addMessageListener("GeckoView:DOMFullscreenRequest", this);

    Services.obs.addObserver(this, "oop-frameloader-crashed");
  }

  onDisable() {
    this.window.removeEventListener("MozDOMFullscreen:Entered", this,
      /* capture */ true);
    this.window.removeEventListener("MozDOMFullscreen:Exited", this,
      /* capture */ true);

    this.messageManager.removeMessageListener("GeckoView:DOMFullscreenExit", this);
    this.messageManager.removeMessageListener("GeckoView:DOMFullscreenRequest", this);

    Services.obs.removeObserver(this, "oop-frameloader-crashed");
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug `onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoViewContent:ExitFullScreen":
        this.messageManager.sendAsyncMessage("GeckoView:DOMFullscreenExited");
        break;
      case "GeckoView:ClearMatches": {
        this._clearMatches();
        break;
      }
      case "GeckoView:DisplayMatches": {
        this._displayMatches(aData);
        break;
      }
      case "GeckoView:FindInPage": {
        this._findInPage(aData, aCallback);
        break;
      }
      case "GeckoView:ZoomToInput":
        this.messageManager.sendAsyncMessage(aEvent);
        break;
      case "GeckoView:ScrollBy":
        this.messageManager.sendAsyncMessage(aEvent, aData);
        break;
      case "GeckoView:ScrollTo":
        this.messageManager.sendAsyncMessage(aEvent, aData);
        break;
      case "GeckoView:SetActive":
        if (aData.active) {
          this.browser.docShellIsActive = true;
        } else {
          this.browser.docShellIsActive = false;
        }
        var msgData = {
          active: aData.active,
          suspendMedia: this.settings.suspendMediaWhenInactive,
        };
        this.messageManager.sendAsyncMessage("GeckoView:SetActive", msgData);
        break;
      case "GeckoView:SetFocused":
        if (aData.focused) {
          this.browser.focus();
          this.browser.setAttribute("primary", "true");
        } else {
          this.browser.removeAttribute("primary");
          this.browser.blur();
        }
        break;
      case "GeckoView:RestoreState":
        this.messageManager.sendAsyncMessage("GeckoView:RestoreState", aData);
        break;
    }
  }

  // DOM event handler
  handleEvent(aEvent) {
    debug `handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "MozDOMFullscreen:Entered":
        if (this.browser == aEvent.target) {
          // Remote browser; dispatch to content process.
          this.messageManager.sendAsyncMessage("GeckoView:DOMFullscreenEntered");
        }
        break;
      case "MozDOMFullscreen:Exited":
        this.messageManager.sendAsyncMessage("GeckoView:DOMFullscreenExited");
        break;
    }
  }

  // Message manager event handler.
  receiveMessage(aMsg) {
    debug `receiveMessage: ${aMsg.name}`;

    switch (aMsg.name) {
      case "GeckoView:DOMFullscreenExit":
        this.window.windowUtils
                   .remoteFrameFullscreenReverted();
        break;
      case "GeckoView:DOMFullscreenRequest":
        this.window.windowUtils
                   .remoteFrameFullscreenChanged(aMsg.target);
        break;
    }
  }

  // nsIObserver event handler
  observe(aSubject, aTopic, aData) {
    debug `observe: ${aTopic}`;

    switch (aTopic) {
      case "oop-frameloader-crashed": {
        const browser = aSubject.ownerElement;
        if (!browser || browser != this.browser) {
          return;
        }

        this.eventDispatcher.sendRequest({
          type: "GeckoView:ContentCrash",
        });
        break;
      }
    }
  }

  _findInPage(aData, aCallback) {
    debug `findInPage: data=${aData} callback=${aCallback && "non-null"}`;

    let finder;
    try {
      finder = this.browser.finder;
    } catch (e) {
      if (aCallback) {
        aCallback.onError(`No finder: ${e}`);
      }
      return;
    }

    if (this._finderListener) {
      finder.removeResultListener(this._finderListener);
    }

    this._finderListener = {
      response: {
        found: false,
        wrapped: false,
        current: 0,
        total: -1,
        searchString: aData.searchString || finder.searchString,
        linkURL: null,
        clientRect: null,
        flags: {
          backwards: !!aData.backwards,
          linksOnly: !!aData.linksOnly,
          matchCase: !!aData.matchCase,
          wholeWord: !!aData.wholeWord,
        },
      },

      onFindResult(aOptions) {
        if (!aCallback || aOptions.searchString !== aData.searchString) {
          // Result from a previous search.
          return;
        }

        Object.assign(this.response, {
          found: aOptions.result !== Ci.nsITypeAheadFind.FIND_NOTFOUND,
          wrapped: aOptions.result !== Ci.nsITypeAheadFind.FIND_FOUND,
          linkURL: aOptions.linkURL,
          clientRect: aOptions.rect && {
            left: aOptions.rect.left,
            top: aOptions.rect.top,
            right: aOptions.rect.right,
            bottom: aOptions.rect.bottom,
          },
          flags: {
            backwards: aOptions.findBackwards,
            linksOnly: aOptions.linksOnly,
            matchCase: this.response.flags.matchCase,
            wholeWord: this.response.flags.wholeWord,
          },
        });

        if (!this.response.found) {
          this.response.current = 0;
          this.response.total = 0;
        }

        // Only send response if we have a count.
        if (!this.response.found || this.response.current !== 0) {
          debug `onFindResult: ${this.response}`;
          aCallback.onSuccess(this.response);
          aCallback = undefined;
        }
      },

      onMatchesCountResult(aResult) {
        if (!aCallback || finder.searchString !== aData.searchString) {
          // Result from a previous search.
          return;
        }

        Object.assign(this.response, {
          current: aResult.current,
          total: aResult.total,
        });

        // Only send response if we have a result. `found` and `wrapped` are
        // both false only when we haven't received a result yet.
        if (this.response.found || this.response.wrapped) {
          debug `onMatchesCountResult: ${this.response}`;
          aCallback.onSuccess(this.response);
          aCallback = undefined;
        }
      },

      onCurrentSelection() {
      },

      onHighlightFinished() {
      },
    };

    finder.caseSensitive = !!aData.matchCase;
    finder.entireWord = !!aData.wholeWord;
    finder.addResultListener(this._finderListener);

    const drawOutline = this._matchDisplayOptions &&
                        !!this._matchDisplayOptions.drawOutline;

    if (!aData.searchString || aData.searchString === finder.searchString) {
      // Search again.
      aData.searchString = finder.searchString;
      finder.findAgain(!!aData.backwards,
                       !!aData.linksOnly,
                       drawOutline);
    } else {
      finder.fastFind(aData.searchString,
                      !!aData.linksOnly,
                      drawOutline);
    }
  }

  _clearMatches() {
    debug `clearMatches`;

    let finder;
    try {
      finder = this.browser.finder;
    } catch (e) {
      return;
    }

    finder.removeSelection();
    finder.highlight(false);

    if (this._finderListener) {
      finder.removeResultListener(this._finderListener);
      this._finderListener = null;
    }
  }

  _displayMatches(aData) {
    debug `displayMatches: data=${aData}`;

    let finder;
    try {
      finder = this.browser.finder;
    } catch (e) {
      return;
    }

    this._matchDisplayOptions = aData;
    finder.onModalHighlightChange(!!aData.dimPage);
    finder.onHighlightAllChange(!!aData.highlightAll);

    if (!aData.highlightAll && !aData.dimPage) {
      finder.highlight(false);
      return;
    }

    if (!this._finderListener || !finder.searchString) {
      return;
    }
    const linksOnly = this._finderListener.response.linksOnly;
    finder.highlight(true, finder.searchString, linksOnly, !!aData.drawOutline);
  }
}

const {debug, warn} = GeckoViewContent.initLogging("GeckoViewContent"); // eslint-disable-line no-unused-vars
