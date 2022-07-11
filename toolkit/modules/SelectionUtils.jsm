/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["SelectionUtils"];

var SelectionUtils = {
  /**
   * Trim the selection text to a reasonable size and sanitize it to make it
   * safe for search query input.
   *
   * @param aSelection
   *        The selection text to trim.
   * @param aMaxLen
   *        The maximum string length, defaults to a reasonable size if undefined.
   * @return The trimmed selection text.
   */
  trimSelection(aSelection, aMaxLen) {
    // Selections of more than 150 characters aren't useful.
    const maxLen = Math.min(aMaxLen || 150, aSelection.length);

    if (aSelection.length > maxLen) {
      // only use the first maxLen important chars. see bug 221361
      let pattern = new RegExp("^(?:\\s*.){0," + maxLen + "}");
      pattern.test(aSelection);
      aSelection = RegExp.lastMatch;
    }

    aSelection = aSelection.trim().replace(/\s+/g, " ");

    if (aSelection.length > maxLen) {
      aSelection = aSelection.substr(0, maxLen);
    }

    return aSelection;
  },

  /**
   * Retrieve the text selection details for the given window.
   *
   * @param  aTopWindow
   *         The top window of the element containing the selection.
   * @param  aCharLen
   *         The maximum string length for the selection text.
   * @return The selection details containing the full and trimmed selection text
   *         and link details for link selections.
   */
  getSelectionDetails(aTopWindow, aCharLen) {
    let focusedWindow = {};
    let focusedElement = Services.focus.getFocusedElementForWindow(
      aTopWindow,
      true,
      focusedWindow
    );
    focusedWindow = focusedWindow.value;

    let selection = focusedWindow.getSelection();
    let selectionStr = selection.toString();
    let fullText;

    let url;
    let linkText;

    let isDocumentLevelSelection = true;
    // try getting a selected text in text input.
    if (!selectionStr && focusedElement) {
      // Don't get the selection for password fields. See bug 565717.
      if (
        ChromeUtils.getClassName(focusedElement) === "HTMLTextAreaElement" ||
        (ChromeUtils.getClassName(focusedElement) === "HTMLInputElement" &&
          focusedElement.mozIsTextField(true))
      ) {
        selection = focusedElement.editor.selection;
        selectionStr = selection.toString();
        isDocumentLevelSelection = false;
      }
    }

    let collapsed = selection.isCollapsed;

    if (selectionStr) {
      // Have some text, let's figure out if it looks like a URL that isn't
      // actually a link.
      linkText = selectionStr.trim();
      if (/^(?:https?|ftp):/i.test(linkText)) {
        try {
          url = Services.io.newURI(linkText);
        } catch (ex) {}
      } else if (/^(?:[a-z\d-]+\.)+[a-z]+$/i.test(linkText)) {
        // Check if this could be a valid url, just missing the protocol.
        // Now let's see if this is an intentional link selection. Our guess is
        // based on whether the selection begins/ends with whitespace or is
        // preceded/followed by a non-word character.

        // selection.toString() trims trailing whitespace, so we look for
        // that explicitly in the first and last ranges.
        let beginRange = selection.getRangeAt(0);
        let delimitedAtStart = /^\s/.test(beginRange);
        if (!delimitedAtStart) {
          let container = beginRange.startContainer;
          let offset = beginRange.startOffset;
          if (container.nodeType == container.TEXT_NODE && offset > 0) {
            delimitedAtStart = /\W/.test(container.textContent[offset - 1]);
          } else {
            delimitedAtStart = true;
          }
        }

        let delimitedAtEnd = false;
        if (delimitedAtStart) {
          let endRange = selection.getRangeAt(selection.rangeCount - 1);
          delimitedAtEnd = /\s$/.test(endRange);
          if (!delimitedAtEnd) {
            let container = endRange.endContainer;
            let offset = endRange.endOffset;
            if (
              container.nodeType == container.TEXT_NODE &&
              offset < container.textContent.length
            ) {
              delimitedAtEnd = /\W/.test(container.textContent[offset]);
            } else {
              delimitedAtEnd = true;
            }
          }
        }

        if (delimitedAtStart && delimitedAtEnd) {
          try {
            url = Services.uriFixup.getFixupURIInfo(linkText).preferredURI;
          } catch (ex) {}
        }
      }
    }

    if (selectionStr) {
      // Pass up to 16K through unmolested.  If an add-on needs more, they will
      // have to use a content script.
      fullText = selectionStr.substr(0, 16384);
      selectionStr = this.trimSelection(selectionStr, aCharLen);
    }

    if (url && !url.host) {
      url = null;
    }

    return {
      text: selectionStr,
      docSelectionIsCollapsed: collapsed,
      isDocumentLevelSelection,
      fullText,
      linkURL: url ? url.spec : null,
      linkText: url ? linkText : "",
    };
  },
};
