/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewModule } from "resource://gre/modules/GeckoViewModule.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  isProductURL: "chrome://global/content/shopping/ShoppingProduct.mjs",
  ShoppingProduct: "chrome://global/content/shopping/ShoppingProduct.mjs",
});

export class GeckoViewContent extends GeckoViewModule {
  onInit() {
    this.registerListener([
      "GeckoViewContent:ExitFullScreen",
      "GeckoView:ClearMatches",
      "GeckoView:DisplayMatches",
      "GeckoView:FindInPage",
      "GeckoView:HasCookieBannerRuleForBrowsingContextTree",
      "GeckoView:RestoreState",
      "GeckoView:ContainsFormData",
      "GeckoView:RequestCreateAnalysis",
      "GeckoView:RequestAnalysisStatus",
      "GeckoView:RequestAnalysisCreationStatus",
      "GeckoView:PollForAnalysisCompleted",
      "GeckoView:SendClickAttributionEvent",
      "GeckoView:SendImpressionAttributionEvent",
      "GeckoView:SendPlacementAttributionEvent",
      "GeckoView:RequestAnalysis",
      "GeckoView:RequestRecommendations",
      "GeckoView:ReportBackInStock",
      "GeckoView:ScrollBy",
      "GeckoView:ScrollTo",
      "GeckoView:SetActive",
      "GeckoView:SetFocused",
      "GeckoView:SetPriorityHint",
      "GeckoView:UpdateInitData",
      "GeckoView:ZoomToInput",
      "GeckoView:IsPdfJs",
    ]);
  }

  onEnable() {
    this.window.addEventListener(
      "MozDOMFullscreen:Entered",
      this,
      /* capture */ true,
      /* untrusted */ false
    );
    this.window.addEventListener(
      "MozDOMFullscreen:Exited",
      this,
      /* capture */ true,
      /* untrusted */ false
    );
    this.window.addEventListener(
      "framefocusrequested",
      this,
      /* capture */ true,
      /* untrusted */ false
    );

    this.window.addEventListener("DOMWindowClose", this);
    this.window.addEventListener("pagetitlechanged", this);
    this.window.addEventListener("pageinfo", this);

    this.window.addEventListener("cookiebannerdetected", this);
    this.window.addEventListener("cookiebannerhandled", this);

    Services.obs.addObserver(this, "oop-frameloader-crashed");
    Services.obs.addObserver(this, "ipc:content-shutdown");
  }

  onDisable() {
    this.window.removeEventListener(
      "MozDOMFullscreen:Entered",
      this,
      /* capture */ true
    );
    this.window.removeEventListener(
      "MozDOMFullscreen:Exited",
      this,
      /* capture */ true
    );
    this.window.removeEventListener(
      "framefocusrequested",
      this,
      /* capture */ true
    );

    this.window.removeEventListener("DOMWindowClose", this);
    this.window.removeEventListener("pagetitlechanged", this);
    this.window.removeEventListener("pageinfo", this);

    this.window.removeEventListener("cookiebannerdetected", this);
    this.window.removeEventListener("cookiebannerhandled", this);

    Services.obs.removeObserver(this, "oop-frameloader-crashed");
    Services.obs.removeObserver(this, "ipc:content-shutdown");
  }

  get actor() {
    return this.getActor("GeckoViewContent");
  }

  get isPdfJs() {
    return (
      this.browser.contentPrincipal.spec === "resource://pdf.js/web/viewer.html"
    );
  }

  // Goes up the browsingContext chain and sends the message every time
  // we cross the process boundary so that every process in the chain is
  // notified.
  sendToAllChildren(aEvent, aData) {
    let { browsingContext } = this.actor;

    while (browsingContext) {
      if (!browsingContext.currentWindowGlobal) {
        break;
      }

      const currentPid = browsingContext.currentWindowGlobal.osPid;
      const parentPid = browsingContext.parent?.currentWindowGlobal.osPid;

      if (currentPid != parentPid) {
        const actor =
          browsingContext.currentWindowGlobal.getActor("GeckoViewContent");
        actor.sendAsyncMessage(aEvent, aData);
      }

      browsingContext = browsingContext.parent;
    }
  }

  #sendDOMFullScreenEventToAllChildren(aEvent) {
    let { browsingContext } = this.actor;

    while (browsingContext) {
      if (!browsingContext.currentWindowGlobal) {
        break;
      }

      const currentPid = browsingContext.currentWindowGlobal.osPid;
      const parentPid = browsingContext.parent?.currentWindowGlobal.osPid;

      if (currentPid != parentPid) {
        if (!browsingContext.parent) {
          // Top level browsing context. Use origin actor (Bug 1505916).
          const chromeBC = browsingContext.topChromeWindow?.browsingContext;
          const requestOrigin = chromeBC?.fullscreenRequestOrigin?.get();
          if (requestOrigin) {
            requestOrigin.browsingContext.currentWindowGlobal
              .getActor("GeckoViewContent")
              .sendAsyncMessage(aEvent, {});
            delete chromeBC.fullscreenRequestOrigin;
            return;
          }
        }
        const actor =
          browsingContext.currentWindowGlobal.getActor("GeckoViewContent");
        actor.sendAsyncMessage(aEvent, {});
      }

      browsingContext = browsingContext.parent;
    }
  }

  // Bundle event handler.
  onEvent(aEvent, aData, aCallback) {
    debug`onEvent: event=${aEvent}, data=${aData}`;

    switch (aEvent) {
      case "GeckoViewContent:ExitFullScreen":
        this.browser.ownerDocument.exitFullscreen();
        break;
      case "GeckoView:ClearMatches": {
        if (!this.isPdfJs) {
          this._clearMatches();
        }
        break;
      }
      case "GeckoView:DisplayMatches": {
        if (!this.isPdfJs) {
          this._displayMatches(aData);
        }
        break;
      }
      case "GeckoView:FindInPage": {
        if (!this.isPdfJs) {
          this._findInPage(aData, aCallback);
        }
        break;
      }
      case "GeckoView:ZoomToInput":
        // For ZoomToInput we just need to send the message to the current focused one.
        const actor =
          Services.focus.focusedContentBrowsingContext.currentWindowGlobal.getActor(
            "GeckoViewContent"
          );
        actor.sendAsyncMessage(aEvent, aData);
        break;
      case "GeckoView:ScrollBy":
        // Unclear if that actually works with oop iframes?
        this.sendToAllChildren(aEvent, aData);
        break;
      case "GeckoView:ScrollTo":
        // Unclear if that actually works with oop iframes?
        this.sendToAllChildren(aEvent, aData);
        break;
      case "GeckoView:UpdateInitData":
        this.sendToAllChildren(aEvent, aData);
        break;
      case "GeckoView:SetActive":
        this.browser.docShellIsActive = !!aData.active;
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
      case "GeckoView:SetPriorityHint":
        if (this.browser.isRemoteBrowser) {
          const remoteTab = this.browser.frameLoader?.remoteTab;
          if (remoteTab) {
            remoteTab.priorityHint = aData.priorityHint;
          }
        }
        break;
      case "GeckoView:RestoreState":
        this.actor.restoreState(aData);
        break;
      case "GeckoView:ContainsFormData":
        this._containsFormData(aCallback);
        break;
      case "GeckoView:RequestAnalysis":
        this._requestAnalysis(aData, aCallback);
        break;
      case "GeckoView:RequestCreateAnalysis":
        this._requestCreateAnalysis(aData, aCallback);
        break;
      case "GeckoView:RequestAnalysisStatus":
        this._requestAnalysisStatus(aData, aCallback);
        break;
      case "GeckoView:RequestAnalysisCreationStatus":
        this._requestAnalysisCreationStatus(aData, aCallback);
        break;
      case "GeckoView:PollForAnalysisCompleted":
        this._pollForAnalysisCompleted(aData, aCallback);
        break;
      case "GeckoView:SendClickAttributionEvent":
        this._sendAttributionEvent("click", aData, aCallback);
        break;
      case "GeckoView:SendImpressionAttributionEvent":
        this._sendAttributionEvent("impression", aData, aCallback);
        break;
      case "GeckoView:SendPlacementAttributionEvent":
        this._sendAttributionEvent("placement", aData, aCallback);
        break;
      case "GeckoView:RequestRecommendations":
        this._requestRecommendations(aData, aCallback);
        break;
      case "GeckoView:ReportBackInStock":
        this._reportBackInStock(aData, aCallback);
        break;
      case "GeckoView:IsPdfJs":
        aCallback.onSuccess(this.isPdfJs);
        break;
      case "GeckoView:HasCookieBannerRuleForBrowsingContextTree":
        this._hasCookieBannerRuleForBrowsingContextTree(aCallback);
        break;
    }
  }

  // DOM event handler
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;

    switch (aEvent.type) {
      case "framefocusrequested":
        if (this.browser != aEvent.target) {
          return;
        }
        if (this.browser.hasAttribute("primary")) {
          return;
        }
        this.eventDispatcher.sendRequest({
          type: "GeckoView:FocusRequest",
        });
        aEvent.preventDefault();
        break;
      case "MozDOMFullscreen:Entered":
        if (this.browser == aEvent.target) {
          // Remote browser; dispatch to content process.
          this.#sendDOMFullScreenEventToAllChildren(
            "GeckoView:DOMFullscreenEntered"
          );
        }
        break;
      case "MozDOMFullscreen:Exited":
        this.#sendDOMFullScreenEventToAllChildren(
          "GeckoView:DOMFullscreenExited"
        );
        break;
      case "pagetitlechanged":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:PageTitleChanged",
          title: this.browser.contentTitle,
        });
        break;
      case "DOMWindowClose":
        // We need this because we want to allow the app
        // to close the window itself. If we don't preventDefault()
        // here Gecko will close it immediately.
        aEvent.preventDefault();

        this.eventDispatcher.sendRequest({
          type: "GeckoView:DOMWindowClose",
        });
        break;
      case "pageinfo":
        if (aEvent.detail.previewImageURL) {
          this.eventDispatcher.sendRequest({
            type: "GeckoView:PreviewImage",
            previewImageUrl: aEvent.detail.previewImageURL,
          });
        }
        break;
      case "cookiebannerdetected":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:CookieBannerEvent:Detected",
        });
        break;
      case "cookiebannerhandled":
        this.eventDispatcher.sendRequest({
          type: "GeckoView:CookieBannerEvent:Handled",
        });
        break;
    }
  }

  // nsIObserver event handler
  observe(aSubject, aTopic, aData) {
    debug`observe: ${aTopic}`;
    this._contentCrashed = false;
    const browser = aSubject.ownerElement;

    switch (aTopic) {
      case "oop-frameloader-crashed": {
        if (!browser || browser != this.browser) {
          return;
        }
        this.window.setTimeout(() => {
          if (this._contentCrashed) {
            this.eventDispatcher.sendRequest({
              type: "GeckoView:ContentCrash",
            });
          } else {
            this.eventDispatcher.sendRequest({
              type: "GeckoView:ContentKill",
            });
          }
        }, 250);
        break;
      }
      case "ipc:content-shutdown": {
        aSubject.QueryInterface(Ci.nsIPropertyBag2);
        if (aSubject.get("dumpID")) {
          if (
            browser &&
            aSubject.get("childID") != browser.frameLoader.childID
          ) {
            return;
          }
          this._contentCrashed = true;
        }
        break;
      }
    }
  }

  async _containsFormData(aCallback) {
    aCallback.onSuccess(await this.actor.containsFormData());
  }

  async _requestAnalysis(aData, aCallback) {
    if (
      Services.prefs.getBoolPref("geckoview.shopping.mock_test_response", false)
    ) {
      const analysis = {
        analysis_url: "https://www.example.com/mock_analysis_url",
        product_id: "ABCDEFG123",
        grade: "B",
        adjusted_rating: 4.5,
        needs_analysis: true,
        page_not_supported: true,
        not_enough_reviews: true,
        highlights: null,
        last_analysis_time: 12345,
        deleted_product_reported: true,
        deleted_product: true,
      };
      aCallback.onSuccess({ analysis });
      return;
    }
    const url = Services.io.newURI(aData.url);
    if (!lazy.isProductURL(url)) {
      aCallback.onError(`Cannot requestAnalysis on a non-product url.`);
    } else {
      const product = new lazy.ShoppingProduct(url);
      const analysis = await product.requestAnalysis();
      if (!analysis) {
        aCallback.onError(`Product analysis returned null.`);
        return;
      }
      aCallback.onSuccess({ analysis });
    }
  }

  async _requestCreateAnalysis(aData, aCallback) {
    if (
      Services.prefs.getBoolPref("geckoview.shopping.mock_test_response", false)
    ) {
      const status = "pending";
      aCallback.onSuccess(status);
      return;
    }
    const url = Services.io.newURI(aData.url);
    if (!lazy.isProductURL(url)) {
      aCallback.onError(`Cannot requestCreateAnalysis on a non-product url.`);
    } else {
      const product = new lazy.ShoppingProduct(url);
      const status = await product.requestCreateAnalysis();
      if (!status) {
        aCallback.onError(`Creation of product analysis returned null.`);
        return;
      }
      aCallback.onSuccess(status.status);
    }
  }

  async _requestAnalysisCreationStatus(aData, aCallback) {
    if (
      Services.prefs.getBoolPref("geckoview.shopping.mock_test_response", false)
    ) {
      const status = "in_progress";
      aCallback.onSuccess(status);
      return;
    }
    const url = Services.io.newURI(aData.url);
    if (!lazy.isProductURL(url)) {
      aCallback.onError(
        `Cannot requestAnalysisCreationStatus on a non-product url.`
      );
    } else {
      const product = new lazy.ShoppingProduct(url);
      const status = await product.requestAnalysisCreationStatus();
      if (!status) {
        aCallback.onError(
          `Status of creation of product analysis returned null.`
        );
        return;
      }
      aCallback.onSuccess(status.status);
    }
  }

  async _requestAnalysisStatus(aData, aCallback) {
    if (
      Services.prefs.getBoolPref("geckoview.shopping.mock_test_response", false)
    ) {
      const status = { status: "in_progress", progress: 90.9 };
      aCallback.onSuccess({ status });
      return;
    }
    const url = Services.io.newURI(aData.url);
    if (!lazy.isProductURL(url)) {
      aCallback.onError(`Cannot requestAnalysisStatus on a non-product url.`);
    } else {
      const product = new lazy.ShoppingProduct(url);
      const status = await product.requestAnalysisCreationStatus();
      if (!status) {
        aCallback.onError(`Status of product analysis returned null.`);
        return;
      }
      aCallback.onSuccess({ status });
    }
  }

  async _pollForAnalysisCompleted(aData, aCallback) {
    const url = Services.io.newURI(aData.url);
    if (!lazy.isProductURL(url)) {
      aCallback.onError(
        `Cannot pollForAnalysisCompleted on a non-product url.`
      );
    } else {
      const product = new lazy.ShoppingProduct(url);
      const status = await product.pollForAnalysisCompleted();
      if (!status) {
        aCallback.onError(
          `Polling the status of creation of product analysis returned null.`
        );
        return;
      }
      aCallback.onSuccess(status.status);
    }
  }

  async _sendAttributionEvent(aEvent, aData, aCallback) {
    let result;
    if (
      Services.prefs.getBoolPref("geckoview.shopping.mock_test_response", false)
    ) {
      result = { TEST_AID: "TEST_AID_RESPONSE" };
    } else {
      result = await lazy.ShoppingProduct.sendAttributionEvent(
        aEvent,
        aData.aid,
        "geckoview_android"
      );
    }
    if (!result || !(aData.aid in result) || !result[aData.aid]) {
      aCallback.onSuccess(false);
      return;
    }
    aCallback.onSuccess(true);
  }

  async _requestRecommendations(aData, aCallback) {
    if (
      Services.prefs.getBoolPref("geckoview.shopping.mock_test_response", false)
    ) {
      const recommendations = [
        {
          name: "Mock Product",
          url: "https://example.com/mock_url",
          image_url: "https://example.com/mock_image_url",
          price: "450",
          currency: "USD",
          grade: "C",
          adjusted_rating: 3.5,
          analysis_url: "https://example.com/mock_analysis_url",
          sponsored: true,
          aid: "mock_aid",
        },
      ];
      aCallback.onSuccess({ recommendations });
      return;
    }
    const url = Services.io.newURI(aData.url);
    if (!lazy.isProductURL(url)) {
      aCallback.onError(`Cannot requestRecommendations on a non-product url.`);
    } else {
      const product = new lazy.ShoppingProduct(url);
      const recommendations = await product.requestRecommendations();
      if (!recommendations) {
        aCallback.onError(`Product recommendations returned null.`);
        return;
      }
      aCallback.onSuccess({ recommendations });
    }
  }

  async _reportBackInStock(aData, aCallback) {
    if (
      Services.prefs.getBoolPref("geckoview.shopping.mock_test_response", false)
    ) {
      const message = "report created";
      aCallback.onSuccess(message);
      return;
    }
    const url = Services.io.newURI(aData.url);
    if (!lazy.isProductURL(url)) {
      aCallback.onError(`Cannot reportBackInStock on a non-product url.`);
    } else {
      const product = new lazy.ShoppingProduct(url);
      const message = await product.sendReport();
      if (!message) {
        aCallback.onError(`Reporting back in stock returned null.`);
        return;
      }
      aCallback.onSuccess(message.message);
    }
  }

  async _hasCookieBannerRuleForBrowsingContextTree(aCallback) {
    const { browsingContext } = this.actor;
    aCallback.onSuccess(
      Services.cookieBanners.hasRuleForBrowsingContextTree(browsingContext)
    );
  }

  _findInPage(aData, aCallback) {
    debug`findInPage: data=${aData} callback=${aCallback && "non-null"}`;

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
          debug`onFindResult: ${this.response}`;
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
          debug`onMatchesCountResult: ${this.response}`;
          aCallback.onSuccess(this.response);
          aCallback = undefined;
        }
      },

      onCurrentSelection() {},

      onHighlightFinished() {},
    };

    finder.caseSensitive = !!aData.matchCase;
    finder.entireWord = !!aData.wholeWord;
    finder.matchDiacritics = !!aData.matchDiacritics;
    finder.addResultListener(this._finderListener);

    const drawOutline =
      this._matchDisplayOptions && !!this._matchDisplayOptions.drawOutline;

    if (!aData.searchString || aData.searchString === finder.searchString) {
      // Search again.
      aData.searchString = finder.searchString;
      finder.findAgain(
        aData.searchString,
        !!aData.backwards,
        !!aData.linksOnly,
        drawOutline
      );
    } else {
      finder.fastFind(aData.searchString, !!aData.linksOnly, drawOutline);
    }
  }

  _clearMatches() {
    debug`clearMatches`;

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
    debug`displayMatches: data=${aData}`;

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

const { debug, warn } = GeckoViewContent.initLogging("GeckoViewContent");
