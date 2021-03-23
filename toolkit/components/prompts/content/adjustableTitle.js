/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { PromptUtils } = ChromeUtils.import(
  "resource://gre/modules/SharedPromptUtils.jsm"
);

// We expect our consumer to provide Services.jsm.
/* global Services */

const AdjustableTitle = {
  _cssSnippet: `
    #titleContainer {
      display: flex;
      flex-direction: row;
      margin-inline: 4px;
      --icon-size: 20px; /* 16px icon + 4px spacing. */
      padding-inline-start: var(--icon-size);
      /* Ensure we don't exceed the bounds of the dialog minus our own padding: */
      max-width: calc(100vw - 32px - var(--icon-size));
      overflow: hidden;
      justify-content: end;
      position: relative;
      min-height: max(16px, 1.5em);
    }

    #titleContainer[noicon] {
      --icon-size: 0;
    }

    #titleContainer[noicon] > .titleIcon {
      display: none;
    }

    .titleIcon {
      /* We put the icon in its own element rather than on .titleContainer, because
       * it needs to overlap the text when the text overflows.
       */
      position: absolute;
      /* center vertically: */
      top: max(0px, calc(0.75em - 8px));
      left: 0;
      width: 16px;
      padding-right: 4px;
      height: 16px;
      background-image: var(--icon-url, url("chrome://global/skin/icons/defaultFavicon.svg"));
      background-size: 16px 16px;
      background-repeat: no-repeat;
      background-color: var(--in-content-page-background);
      -moz-context-properties: fill;
      fill: currentColor;
    }

    #titleText {
      font-weight: 600;
      flex: 1 0 auto; /* Grow but do not shrink. */
      white-space: nowrap;
    }

    #titleContainer[overflown] {
      mask-image: linear-gradient(to right, black, black 20px, transparent 20px, black 120px);
      mask-repeat: no-repeat;
    }

    /* hide the old title */
    #infoTitle {
      display: none;
    }
   `,

  _insertMarkup() {
    let iconEl = document.createElement("div");
    iconEl.className = "titleIcon";
    this._titleEl = document.createElement("span");
    this._titleEl.id = "titleText";
    this._containerEl = document.createElement("div");
    this._containerEl.id = "titleContainer";
    this._containerEl.className = "dialogRow titleContainer";
    this._containerEl.append(iconEl, this._titleEl);
    let targetID = document.documentElement.getAttribute("headerparent");
    document.getElementById(targetID).prepend(this._containerEl);
    let styleEl = document.createElement("style");
    styleEl.textContent = this._cssSnippet;
    document.documentElement.prepend(styleEl);
  },

  _overflowHandler() {
    requestAnimationFrame(async () => {
      let isOverflown = await window.promiseDocumentFlushed(() => {
        return (
          this._containerEl.getBoundingClientRect().width -
            this._titleEl.getBoundingClientRect().width -
            this._titleEl.previousElementSibling.getBoundingClientRect().width <
          0
        );
      });
      this._containerEl.toggleAttribute("overflown", isOverflown);
      if (isOverflown) {
        this._titleEl.setAttribute("title", this._titleEl.textContent);
      } else {
        this._titleEl.removeAttribute("title");
      }
    });
  },

  _updateTitle(title) {
    title = JSON.parse(title);
    if (title.raw) {
      this._titleEl.textContent = title.raw;
    } else {
      document.l10n.setAttributes(this._titleEl, title.l10nId);
    }

    if (!document.documentElement.hasAttribute("neediconheader")) {
      this._containerEl.setAttribute("noicon", "true");
    } else {
      this._overflowHandler();
    }
  },

  init() {
    // Only run this if we're embedded and proton modals are enabled.
    if (!window.docShell.chromeEventHandler || !PromptUtils.protonModals) {
      return;
    }

    this._insertMarkup();
    let title = document.documentElement.getAttribute("headertitle");
    if (title) {
      this._updateTitle(title);
    }
    this._mutObs = new MutationObserver(() => {
      this._updateTitle(document.documentElement.getAttribute("headertitle"));
    });
    this._mutObs.observe(document.documentElement, {
      attributes: true,
      attributeFilter: ["headertitle"],
    });
  },
};

document.addEventListener(
  "DOMContentLoaded",
  () => {
    AdjustableTitle.init();
  },
  { once: true }
);
