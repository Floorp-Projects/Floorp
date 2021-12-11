// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

// -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

customElements.define(
  "printpreview-pagination",
  class PrintPreviewPagination extends HTMLElement {
    static get markup() {
      return `
      <html:link rel="stylesheet" href="chrome://global/content/printPagination.css" />
      <html:div class="container">
        <html:button id="navigateHome" class="toolbarButton startItem" data-l10n-id="printpreview-homearrow-button"></html:button>
        <html:button id="navigatePrevious" class="toolbarButton" data-l10n-id="printpreview-previousarrow-button"></html:button>
        <html:div class="toolbarCenter"><html:span id="sheetIndicator" data-l10n-id="printpreview-sheet-of-sheets" data-l10n-args='{ "sheetNum": 1, "sheetCount": 1 }'></html:span></html:div>
        <html:button id="navigateNext" class="toolbarButton" data-l10n-id="printpreview-nextarrow-button"></html:button>
        <html:button id="navigateEnd" class="toolbarButton endItem" data-l10n-id="printpreview-endarrow-button"></html:button>
      </html:div>
      `;
    }

    static get defaultProperties() {
      return {
        currentPage: 1,
        sheetCount: 1,
      };
    }

    get previewBrowser() {
      return this._previewBrowser;
    }

    set previewBrowser(aBrowser) {
      this._previewBrowser = aBrowser;
    }

    observePreviewBrowser(browser) {
      if (browser == this.previewBrowser || !this.isConnected) {
        return;
      }
      this.previewBrowser = browser;
      this.mutationObserver.disconnect();
      this.mutationObserver.observe(browser, {
        attributes: ["current-page", "sheet-count"],
      });
      this.updateFromBrowser();
    }

    connectedCallback() {
      MozXULElement.insertFTLIfNeeded("toolkit/printing/printPreview.ftl");

      const shadowRoot = this.attachShadow({ mode: "open" });
      document.l10n.connectRoot(shadowRoot);

      let fragment = MozXULElement.parseXULToFragment(this.constructor.markup);
      this.shadowRoot.append(fragment);

      this.elements = {
        sheetIndicator: shadowRoot.querySelector("#sheetIndicator"),
        homeButton: shadowRoot.querySelector("#navigateHome"),
        previousButton: shadowRoot.querySelector("#navigatePrevious"),
        nextButton: shadowRoot.querySelector("#navigateNext"),
        endButton: shadowRoot.querySelector("#navigateEnd"),
      };

      this.shadowRoot.addEventListener("click", this);

      this.mutationObserver = new MutationObserver(() =>
        this.updateFromBrowser()
      );

      // Initial render with some default values
      // We'll be updated with real values when available
      this.update(this.constructor.defaultProperties);
    }

    disconnectedCallback() {
      document.l10n.disconnectRoot(this.shadowRoot);
      this.shadowRoot.textContent = "";
      this.mutationObserver?.disconnect();
      delete this.mutationObserver;
      this.currentPreviewBrowserObserver?.disconnect();
      delete this.currentPreviewBrowserObserver;
    }

    handleEvent(event) {
      if (event.type == "click" && event.button != 0) {
        return;
      }
      event.stopPropagation();

      switch (event.target) {
        case this.elements.homeButton:
          this.navigate(0, 0, "home");
          break;
        case this.elements.previousButton:
          this.navigate(-1, 0, 0);
          break;
        case this.elements.nextButton:
          this.navigate(1, 0, 0);
          break;
        case this.elements.endButton:
          this.navigate(0, 0, "end");
          break;
      }
    }

    navigate(aDirection, aPageNum, aHomeOrEnd) {
      const nsIWebBrowserPrint = Ci.nsIWebBrowserPrint;
      let targetNum;
      let navType;
      // we use only one of aHomeOrEnd, aDirection, or aPageNum
      if (aHomeOrEnd) {
        // We're going to either the very first page ("home"), or the
        // very last page ("end").
        if (aHomeOrEnd == "home") {
          targetNum = 1;
          navType = nsIWebBrowserPrint.PRINTPREVIEW_HOME;
        } else {
          targetNum = this.sheetCount;
          navType = nsIWebBrowserPrint.PRINTPREVIEW_END;
        }
      } else if (aPageNum) {
        // We're going to a specific page (aPageNum)
        targetNum = Math.min(Math.max(1, aPageNum), this.sheetCount);
        navType = nsIWebBrowserPrint.PRINTPREVIEW_GOTO_PAGENUM;
      } else {
        // aDirection is either +1 or -1, and allows us to increment
        // or decrement our currently viewed page.
        targetNum = Math.min(
          Math.max(1, this.currentSheet + aDirection),
          this.sheetCount
        );
        navType = nsIWebBrowserPrint.PRINTPREVIEW_GOTO_PAGENUM;
      }

      // Preemptively update our own state, rather than waiting for the message from the child process
      // This allows subsequent clicks of next/back to advance 1 page per click if possible
      // and keeps the UI feeling more responsive
      this.update({ currentPage: targetNum });

      this.previewBrowser.sendMessageToActor(
        "Printing:Preview:Navigate",
        {
          navType,
          pageNum: targetNum,
        },
        "Printing"
      );
    }

    update(data = {}) {
      if (data.sheetCount) {
        this.sheetCount = data.sheetCount;
      }
      if (data.currentPage) {
        this.currentSheet = data.currentPage;
      }
      document.l10n.setAttributes(
        this.elements.sheetIndicator,
        this.elements.sheetIndicator.dataset.l10nId,
        {
          sheetNum: this.currentSheet,
          sheetCount: this.sheetCount,
        }
      );
    }

    updateFromBrowser() {
      let sheetCount = parseInt(
        this.previewBrowser.getAttribute("sheet-count"),
        10
      );
      let currentPage = parseInt(
        this.previewBrowser.getAttribute("current-page"),
        10
      );
      this.update({ sheetCount, currentPage });
    }
  }
);
