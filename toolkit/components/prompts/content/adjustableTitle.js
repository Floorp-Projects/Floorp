/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let { PromptUtils } = ChromeUtils.import(
  "resource://gre/modules/SharedPromptUtils.jsm"
);

const AdjustableTitle = {
  _cssSnippet: `
    #titleContainer {
      /* This gets display: flex by virtue of being a row in a subdialog, from
        * commonDialog.css . */
      flex-shrink: 0;

      flex-direction: row;
      align-items: baseline;

      margin-inline: 4px;
      /* Ensure we don't exceed the bounds of the dialog: */
      max-width: calc(100vw - 32px);

      --icon-size: 16px;
    }

    #titleContainer[noicon] > .titleIcon {
      display: none;
    }

    .titleIcon {
      width: var(--icon-size);
      height: var(--icon-size);
      padding-inline-end: 4px;
      flex-shrink: 0;

      background-image: var(--icon-url, url("chrome://global/skin/icons/defaultFavicon.svg"));
      background-size: 16px 16px;
      background-origin: content-box;
      background-repeat: no-repeat;
      background-color: var(--in-content-page-background);
      -moz-context-properties: fill;
      fill: currentColor;
    }

    #titleCropper:not([nomaskfade]) {
      display: inline-flex;
    }

    #titleCropper {
      overflow: hidden;

      justify-content: right;
      mask-repeat: no-repeat;
      /* go from left to right with the mask: */
      --mask-dir: right;
    }

    #titleContainer:not([noicon]) > #titleCropper {
      /* Align the icon and text: */
      translate: 0 calc(-1px - max(.6 * var(--icon-size) - .6em, 0px));
    }

    #titleCropper[rtlorigin] {
      justify-content: left;
      /* go from right to left with the mask: */
      --mask-dir: left;
    }


    #titleCropper:not([nomaskfade]) #titleText {
      display: inline-flex;
      white-space: nowrap;
    }

    #titleText {
      font-weight: 600;
      flex: 1 0 auto; /* Grow but do not shrink. */
      unicode-bidi: plaintext; /* Ensure we align RTL text correctly. */
      text-align: match-parent;
    }

    #titleCropper[overflown] {
      mask-image: linear-gradient(to var(--mask-dir), transparent, black 100px);
    }

    /* hide the old title */
    #infoTitle {
      display: none;
    }
   `,

  _insertMarkup() {
    let iconEl = document.createElement("span");
    iconEl.className = "titleIcon";
    this._titleCropEl = document.createElement("span");
    this._titleCropEl.id = "titleCropper";
    this._titleEl = document.createElement("span");
    this._titleEl.id = "titleText";
    this._containerEl = document.createElement("div");
    this._containerEl.id = "titleContainer";
    this._containerEl.className = "dialogRow titleContainer";
    this._titleCropEl.append(this._titleEl);
    this._containerEl.append(iconEl, this._titleCropEl);
    let targetID = document.documentElement.getAttribute("headerparent");
    document.getElementById(targetID).prepend(this._containerEl);
    let styleEl = document.createElement("style");
    styleEl.textContent = this._cssSnippet;
    document.documentElement.prepend(styleEl);
  },

  _overflowHandler() {
    requestAnimationFrame(async () => {
      let isOverflown;
      try {
        isOverflown = await window.promiseDocumentFlushed(() => {
          return (
            this._titleCropEl.getBoundingClientRect().width <
            this._titleEl.getBoundingClientRect().width
          );
        });
      } catch (ex) {
        // In automated tests, this can fail with a DOM exception if
        // the window has closed by the time layout tries to call us.
        // In this case, just bail, and only log any other errors:
        if (
          !DOMException.isInstance(ex) ||
          ex.name != "NoModificationAllowedError"
        ) {
          Cu.reportError(ex);
        }
        return;
      }
      this._titleCropEl.toggleAttribute("overflown", isOverflown);
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
      let { DIRECTION_RTL } = window.windowUtils;
      this._titleCropEl.toggleAttribute(
        "rtlorigin",
        window.windowUtils.getDirectionFromText(title.raw) == DIRECTION_RTL
      );
    } else {
      document.l10n.setAttributes(this._titleEl, title.l10nId);
    }

    if (!document.documentElement.hasAttribute("neediconheader")) {
      this._containerEl.setAttribute("noicon", "true");
    } else if (title.shouldUseMaskFade) {
      this._overflowHandler();
    } else {
      this._titleCropEl.toggleAttribute("nomaskfade", true);
    }
  },

  init() {
    // Only run this if we're embedded and proton modals are enabled.
    if (!window.docShell.chromeEventHandler) {
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
