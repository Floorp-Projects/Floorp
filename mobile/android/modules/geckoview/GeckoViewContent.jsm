/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewContent"];

ChromeUtils.import("resource://gre/modules/GeckoViewModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

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
        "GeckoView:SaveState",
        "GeckoView:SetActive",
        "GeckoView:ZoomToInput",
    ]);

    this.messageManager.addMessageListener("GeckoView:SaveStateFinish", this);
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
      case "GeckoView:SetActive":
        if (aData.active) {
          this.browser.setAttribute("primary", "true");
          this.browser.focus();
          this.browser.docShellIsActive = true;
        } else {
          this.browser.removeAttribute("primary");
          this.browser.docShellIsActive = false;
          this.browser.blur();
        }
        break;
      case "GeckoView:SaveState":
        if (!this._saveStateCallbacks) {
          this._saveStateCallbacks = new Map();
          this._saveStateNextId = 0;
        }
        this._saveStateCallbacks.set(this._saveStateNextId, aCallback);
        this.messageManager.sendAsyncMessage("GeckoView:SaveState", {id: this._saveStateNextId});
        this._saveStateNextId++;
        break;
      case "GeckoView:RestoreState":
        this.messageManager.sendAsyncMessage("GeckoView:RestoreState", {state: aData.state});
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
        this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils)
                   .remoteFrameFullscreenReverted();
        break;
      case "GeckoView:DOMFullscreenRequest":
        this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils)
                   .remoteFrameFullscreenChanged(aMsg.target);
        break;
      case "GeckoView:SaveStateFinish":
        if (!this._saveStateCallbacks || !this._saveStateCallbacks.has(aMsg.data.id)) {
          warn `Failed to save state due to missing callback`;
          return;
        }
        this._saveStateCallbacks.get(aMsg.data.id).onSuccess(aMsg.data.state);
        this._saveStateCallbacks.delete(aMsg.data.id);
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
          type: "GeckoView:ContentCrash"
        });
      }
      break;
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
    };

    finder.caseSensitive = !!aData.matchCase;
    finder.entireWord = !!aData.wholeWord;

    if (aCallback) {
      finder.addResultListener(this._finderListener);
    }

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
    try {
      this.browser.finder.removeSelection();
    } catch (e) {
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
    finder.onHighlightAllChange(!!aData.highlightAll);
    finder.onModalHighlightChange(!!aData.dimPage);

    if (!finder.searchString) {
      return;
    }
    if (!aData.highlightAll && !aData.dimPage && !aData.drawOutline) {
      finder.highlighter.highlight(false);
      return;
    }
    const linksOnly = this._finderListener &&
                      this._finderListener.response.linksOnly;
    finder.highlighter.highlight(true,
                                 finder.searchString,
                                 linksOnly,
                                 !!aData.drawOutline);
  }
}
