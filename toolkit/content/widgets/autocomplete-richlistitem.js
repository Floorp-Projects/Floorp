/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "LoginHelper",
                               "resource://gre/modules/LoginHelper.jsm");


MozElements.MozAutocompleteRichlistitem = class MozAutocompleteRichlistitem extends MozElements.MozRichlistitem {
  constructor() {
    super();

    /**
     * This overrides listitem's mousedown handler because we want to set the
     * selected item even when the shift or accel keys are pressed.
     */
    this.addEventListener("mousedown", (event) => {
      // Call this.control only once since it's not a simple getter.
      let control = this.control;
      if (!control || control.disabled) {
        return;
      }
      if (!this.selected) {
        control.selectItem(this);
      }
      control.currentItem = this;
    });

    this.addEventListener("mouseover", (event) => {
      // The point of implementing this handler is to allow drags to change
      // the selected item.  If the user mouses down on an item, it becomes
      // selected.  If they then drag the mouse to another item, select it.
      // Handle all three primary mouse buttons: right, left, and wheel, since
      // all three change the selection on mousedown.
      let mouseDown = event.buttons & 0b111;
      if (!mouseDown) {
        return;
      }
      // Call this.control only once since it's not a simple getter.
      let control = this.control;
      if (!control || control.disabled) {
        return;
      }
      if (!this.selected) {
        control.selectItem(this);
      }
      control.currentItem = this;
    });

    this.addEventListener("overflow", () => this._onOverflow());
    this.addEventListener("underflow", () => this._onUnderflow());
  }

  connectedCallback() {
    if (this.delayConnectedCallback()) {
      return;
    }

    this.textContent = "";
    this.appendChild(MozXULElement.parseXULToFragment(this._markup));
    this.initializeAttributeInheritance();

    this._boundaryCutoff = null;
    this._inOverflow = false;

    this._adjustAcItem();
  }

  static get inheritedAttributes() {
    return {
      ".ac-type-icon": "selected,current,type",
      ".ac-site-icon": "src=image,selected,type",
      ".ac-title": "selected",
      ".ac-title-text": "selected",
      ".ac-tags": "selected",
      ".ac-tags-text": "selected",
      ".ac-separator": "selected,actiontype,type",
      ".ac-url": "selected,actiontype",
      ".ac-url-text": "selected",
      ".ac-action": "selected,actiontype",
      ".ac-action-text": "selected",
    };
  }

  get _markup() {
    return `
      <image class="ac-type-icon"></image>
      <image class="ac-site-icon"></image>
      <hbox class="ac-title" align="center">
        <description class="ac-text-overflow-container">
          <description class="ac-title-text"></description>
        </description>
      </hbox>
      <hbox class="ac-tags" align="center">
        <description class="ac-text-overflow-container">
          <description class="ac-tags-text"></description>
        </description>
      </hbox>
      <hbox class="ac-separator" align="center">
        <description class="ac-separator-text" value="—"></description>
      </hbox>
      <hbox class="ac-url" align="center">
        <description class="ac-text-overflow-container">
          <description class="ac-url-text"></description>
        </description>
      </hbox>
      <hbox class="ac-action" align="center">
        <description class="ac-text-overflow-container">
          <description class="ac-action-text"></description>
        </description>
      </hbox>
    `;
  }

  get _typeIcon() {
    return this.querySelector(".ac-type-icon");
  }

  get _titleText() {
    return this.querySelector(".ac-title-text");
  }

  get _tags() {
    return this.querySelector(".ac-tags");
  }

  get _tagsText() {
    return this.querySelector(".ac-tags-text");
  }

  get _separator() {
    return this.querySelector(".ac-separator");
  }

  get _urlText() {
    return this.querySelector(".ac-url-text");
  }

  get _actionText() {
    return this.querySelector(".ac-action-text");
  }

  get label() {
    // This property is a string that is read aloud by screen readers,
    // so it must not contain anything that should not be user-facing.

    let parts = [
      this.getAttribute("title"),
      this.getAttribute("displayurl"),
    ];
    let label = parts.filter(str => str).join(" ");

    // allow consumers that have extended popups to override
    // the label values for the richlistitems
    let panel = this.parentNode.parentNode;
    if (panel.createResultLabel) {
      return panel.createResultLabel(this, label);
    }

    return label;
  }

  get _stringBundle() {
    if (!this.__stringBundle) {
      this.__stringBundle = Services.strings.createBundle(
        "chrome://global/locale/autocomplete.properties"
      );
    }
    return this.__stringBundle;
  }

  get boundaryCutoff() {
    if (!this._boundaryCutoff) {
      this._boundaryCutoff = Services.prefs.
        getIntPref("toolkit.autocomplete.richBoundaryCutoff");
    }
    return this._boundaryCutoff;
  }

  _cleanup() {
    this.removeAttribute("url");
    this.removeAttribute("image");
    this.removeAttribute("title");
    this.removeAttribute("text");
    this.removeAttribute("displayurl");
  }

  _onOverflow() {
    this._inOverflow = true;
    this._handleOverflow();
  }

  _onUnderflow() {
    this._inOverflow = false;
    this._handleOverflow();
  }

  _getBoundaryIndices(aText, aSearchTokens) {
    // Short circuit for empty search ([""] == "")
    if (aSearchTokens == "")
      return [0, aText.length];

    // Find which regions of text match the search terms
    let regions = [];
    for (let search of Array.prototype.slice.call(aSearchTokens)) {
      let matchIndex = -1;
      let searchLen = search.length;

      // Find all matches of the search terms, but stop early for perf
      let lowerText = aText.substr(0, this.boundaryCutoff).toLowerCase();
      while ((matchIndex = lowerText.indexOf(search, matchIndex + 1)) >= 0) {
        regions.push([matchIndex, matchIndex + searchLen]);
      }
    }

    // Sort the regions by start position then end position
    regions = regions.sort((a, b) => {
      let start = a[0] - b[0];
      return (start == 0) ? a[1] - b[1] : start;
    });

    // Generate the boundary indices from each region
    let start = 0;
    let end = 0;
    let boundaries = [];
    let len = regions.length;
    for (let i = 0; i < len; i++) {
      // We have a new boundary if the start of the next is past the end
      let region = regions[i];
      if (region[0] > end) {
        // First index is the beginning of match
        boundaries.push(start);
        // Second index is the beginning of non-match
        boundaries.push(end);

        // Track the new region now that we've stored the previous one
        start = region[0];
      }

      // Push back the end index for the current or new region
      end = Math.max(end, region[1]);
    }

    // Add the last region
    boundaries.push(start);
    boundaries.push(end);

    // Put on the end boundary if necessary
    if (end < aText.length)
      boundaries.push(aText.length);

    // Skip the first item because it's always 0
    return boundaries.slice(1);
  }

  _getSearchTokens(aSearch) {
    let search = aSearch.toLowerCase();
    return search.split(/\s+/);
  }

  _setUpDescription(aDescriptionElement, aText, aNoEmphasis) {
    // Get rid of all previous text
    if (!aDescriptionElement) {
      return;
    }
    while (aDescriptionElement.hasChildNodes())
      aDescriptionElement.firstChild.remove();

    // If aNoEmphasis is specified, don't add any emphasis
    if (aNoEmphasis) {
      aDescriptionElement.appendChild(document.createTextNode(aText));
      return;
    }

    // Get the indices that separate match and non-match text
    let search = this.getAttribute("text");
    let tokens = this._getSearchTokens(search);
    let indices = this._getBoundaryIndices(aText, tokens);

    this._appendDescriptionSpans(indices, aText, aDescriptionElement,
      aDescriptionElement);
  }

  _appendDescriptionSpans(indices, text, spansParentElement, descriptionElement) {
    let next;
    let start = 0;
    let len = indices.length;
    // Even indexed boundaries are matches, so skip the 0th if it's empty
    for (let i = indices[0] == 0 ? 1 : 0; i < len; i++) {
      next = indices[i];
      let spanText = text.substr(start, next - start);
      start = next;

      if (i % 2 == 0) {
        // Emphasize the text for even indices
        let span = spansParentElement.appendChild(
          document.createElementNS("http://www.w3.org/1999/xhtml", "span"));
        this._setUpEmphasisSpan(span, descriptionElement);
        span.textContent = spanText;
      } else {
        // Otherwise, it's plain text
        spansParentElement.appendChild(document.createTextNode(spanText));
      }
    }
  }

  _setUpTags(tags) {
    while (this._tagsText.hasChildNodes()) {
      this._tagsText.firstChild.remove();
    }

    let anyTagsMatch = false;

    // Include only tags that match the search string.
    for (let tag of tags) {
      // Check if the tag matches the search string.
      let search = this.getAttribute("text");
      let tokens = this._getSearchTokens(search);
      let indices = this._getBoundaryIndices(tag, tokens);

      if (indices.length == 2 &&
        indices[0] == 0 &&
        indices[1] == tag.length) {
        // The tag doesn't match the search string, so don't include it.
        continue;
      }

      anyTagsMatch = true;

      let tagSpan =
        document.createElementNS("http://www.w3.org/1999/xhtml", "span");
      tagSpan.classList.add("ac-tag");
      this._tagsText.appendChild(tagSpan);

      this._appendDescriptionSpans(indices, tag, tagSpan, this._tagsText);
    }

    return anyTagsMatch;
  }

  _setUpEmphasisSpan(aSpan, aDescriptionElement) {
    aSpan.classList.add("ac-emphasize-text");
    switch (aDescriptionElement) {
      case this._titleText:
        aSpan.classList.add("ac-emphasize-text-title");
        break;
      case this._tagsText:
        aSpan.classList.add("ac-emphasize-text-tag");
        break;
      case this._urlText:
        aSpan.classList.add("ac-emphasize-text-url");
        break;
      case this._actionText:
        aSpan.classList.add("ac-emphasize-text-action");
        break;
    }
  }

  /**
   * This will generate an array of emphasis pairs for use with
   * _setUpEmphasisedSections(). Each pair is a tuple (array) that
   * represents a block of text - containing the text of that block, and a
   * boolean for whether that block should have an emphasis styling applied
   * to it.
   *
   * These pairs are generated by parsing a localised string (aSourceString)
   * with parameters, in the format that is used by
   * nsIStringBundle.formatStringFromName():
   *
   * "textA %1$S textB textC %2$S"
   *
   * Or:
   *
   * "textA %S"
   *
   * Where "%1$S", "%2$S", and "%S" are intended to be replaced by provided
   * replacement strings. These are specified an array of tuples
   * (aReplacements), each containing the replacement text and a boolean for
   * whether that text should have an emphasis styling applied. This is used
   * as a 1-based array - ie, "%1$S" is replaced by the item in the first
   * index of aReplacements, "%2$S" by the second, etc. "%S" will always
   * match the first index.
   */
  _generateEmphasisPairs(aSourceString, aReplacements) {
    let pairs = [];

    // Split on %S, %1$S, %2$S, etc. ie:
    //   "textA %S"
    //     becomes ["textA ", "%S"]
    //   "textA %1$S textB textC %2$S"
    //     becomes ["textA ", "%1$S", " textB textC ", "%2$S"]
    let parts = aSourceString.split(/(%(?:[0-9]+\$)?S)/);

    for (let part of parts) {
      // The above regex will actually give us an empty string at the
      // end - we don't want that, as we don't want to later generate an
      // empty text node for it.
      if (part.length === 0)
        continue;

      // Determine if this token is a replacement token or a normal text
      // token. If it is a replacement token, we want to extract the
      // numerical number. However, we still want to match on "$S".
      let match = part.match(/^%(?:([0-9]+)\$)?S$/);

      if (match) {
        // "%S" doesn't have a numerical number in it, but will always
        // be assumed to be 1. Furthermore, the input string specifies
        // these with a 1-based index, but we want a 0-based index.
        let index = (match[1] || 1) - 1;

        if (index >= 0 && index < aReplacements.length) {
          pairs.push([...aReplacements[index]]);
        }
      } else {
        pairs.push([part]);
      }
    }

    return pairs;
  }

  /**
   * _setUpEmphasisedSections() has the same use as _setUpDescription,
   * except instead of taking a string and highlighting given tokens, it takes
   * an array of pairs generated by _generateEmphasisPairs(). This allows
   * control over emphasising based on specific blocks of text, rather than
   * search for substrings.
   */
  _setUpEmphasisedSections(aDescriptionElement, aTextPairs) {
    // Get rid of all previous text
    while (aDescriptionElement.hasChildNodes())
      aDescriptionElement.firstChild.remove();

    for (let [text, emphasise] of aTextPairs) {
      if (emphasise) {
        let span = aDescriptionElement.appendChild(
          document.createElementNS("http://www.w3.org/1999/xhtml", "span"));
        span.textContent = text;
        switch (emphasise) {
          case "match":
            this._setUpEmphasisSpan(span, aDescriptionElement);
            break;
        }
      } else {
        aDescriptionElement.appendChild(document.createTextNode(text));
      }
    }
  }

  _unescapeUrl(url) {
    return Services.textToSubURI.unEscapeURIForUI("UTF-8", url);
  }

  _reuseAcItem() {
    let action = this._parseActionUrl(this.getAttribute("url"));
    let popup = this.parentNode.parentNode;

    // If the item is a searchengine action, then it should
    // only be reused if the engine name is the same as the
    // popup's override engine name, if any.
    if (!action ||
      action.type != "searchengine" ||
      !popup.overrideSearchEngineName ||
      action.params.engineName == popup.overrideSearchEngineName) {
      this.collapsed = false;

      // The popup may have changed size between now and the last
      // time the item was shown, so always handle over/underflow.
      let dwu = window.windowUtils;
      let popupWidth = dwu.getBoundsWithoutFlushing(this.parentNode).width;
      if (!this._previousPopupWidth || this._previousPopupWidth != popupWidth) {
        this._previousPopupWidth = popupWidth;
        this.handleOverUnderflow();
      }

      return true;
    }

    return false;
  }

  _adjustAcItem() {
    let originalUrl = this.getAttribute("ac-value");
    let title = this.getAttribute("ac-comment");
    this.setAttribute("url", originalUrl);
    this.setAttribute("image", this.getAttribute("ac-image"));
    this.setAttribute("title", title);
    this.setAttribute("text", this.getAttribute("ac-text"));

    let popup = this.parentNode.parentNode;
    let titleLooksLikeUrl = false;
    let displayUrl = originalUrl;
    let emphasiseUrl = true;
    let trimDisplayUrl = true;

    let type = this.getAttribute("originaltype");
    let types = new Set(type.split(/\s+/));
    let initialTypes = new Set(types);
    // Remove types that should ultimately not be in the `type` string.
    types.delete("action");
    types.delete("autofill");
    types.delete("heuristic");
    type = [...types][0] || "";

    let action;

    if (initialTypes.has("autofill") && !initialTypes.has("action")) {
      // Treat autofills as visiturl actions, unless they are already also
      // actions.
      action = {
        type: "visiturl",
        params: { url: title },
      };
    }

    this.removeAttribute("actiontype");
    this.classList.remove(
      "overridable-action",
      "emptySearchQuery",
      "aliasOffer"
    );

    // If the type includes an action, set up the item appropriately.
    if (initialTypes.has("action") || action) {
      action = action || this._parseActionUrl(originalUrl);
      this.setAttribute("actiontype", action.type);

      switch (action.type) {
        case "switchtab":
          {
            this.classList.add("overridable-action");
            displayUrl = action.params.url;
            let desc = this._stringBundle.GetStringFromName("switchToTab2");
            this._setUpDescription(this._actionText, desc, true);
            break;
          }
        case "remotetab":
          {
            displayUrl = action.params.url;
            let desc = action.params.deviceName;
            this._setUpDescription(this._actionText, desc, true);
            break;
          }
        case "searchengine":
          {
            emphasiseUrl = false;

            // The order here is not localizable, we default to appending
            // "- Search with Engine" to the search string, to be able to
            // properly generate emphasis pairs. That said, no localization
            // changed the order while it was possible, so doesn't look like
            // there's a strong need for that.
            let {
              engineName,
              searchSuggestion,
              searchQuery,
              alias,
            } = action.params;

            // Override the engine name if the popup defines an override.
            let override = popup.overrideSearchEngineName;
            if (override && override != engineName) {
              engineName = override;
              action.params.engineName = override;
              let newURL =
                PlacesUtils.mozActionURI(action.type, action.params);
              this.setAttribute("url", newURL);
            }

            let engineStr =
              this._stringBundle.formatStringFromName("searchWithEngine", [engineName], 1);
            this._setUpDescription(this._actionText, engineStr, true);

            // Make the title by generating an array of pairs and its
            // corresponding interpolation string (e.g., "%1$S") to pass to
            // _generateEmphasisPairs.
            let pairs;
            if (searchSuggestion) {
              // Check if the search query appears in the suggestion.  It may
              // not.  If it does, then emphasize the query in the suggestion
              // and otherwise just include the suggestion without emphasis.
              let idx = searchSuggestion.indexOf(searchQuery);
              if (idx >= 0) {
                pairs = [
                  [searchSuggestion.substring(0, idx), ""],
                  [searchQuery, "match"],
                  [searchSuggestion.substring(idx + searchQuery.length), ""],
                ];
              } else {
                pairs = [
                  [searchSuggestion, ""],
                ];
              }
            } else if (alias &&
              !searchQuery.trim() &&
              !initialTypes.has("heuristic")) {
              // For non-heuristic alias results that have an empty query, we
              // want to show "@engine -- Search with Engine" to make it clear
              // that the user can search by selecting the result and using
              // the alias.  Normally we hide the "Search with Engine" part
              // until the result is selected or moused over, but not here.
              // Add the aliasOffer class so we can detect this in the CSS.
              this.classList.add("aliasOffer");
              pairs = [
                [alias, ""],
              ];
            } else {
              // Add the emptySearchQuery class if the search query is the
              // empty string.  We use it to hide .ac-separator in CSS.
              if (!searchQuery.trim()) {
                this.classList.add("emptySearchQuery");
              }
              pairs = [
                [searchQuery, ""],
              ];
            }
            let interpStr = pairs.map((pair, i) => `%${i + 1}$S`).join("");
            title = this._generateEmphasisPairs(interpStr, pairs);

            // If this is a default search match, we remove the image so we
            // can style it ourselves with a generic search icon.
            // We don't do this when matching an aliased search engine,
            // because the icon helps with recognising which engine will be
            // used (when using the default engine, we don't need that
            // recognition).
            if (!action.params.alias && !initialTypes.has("favicon")) {
              this.removeAttribute("image");
            }
            break;
          }
        case "visiturl":
          {
            emphasiseUrl = false;
            displayUrl = action.params.url;
            titleLooksLikeUrl = true;
            let visitStr = this._stringBundle.GetStringFromName("visit");
            this._setUpDescription(this._actionText, visitStr, true);
            break;
          }
        case "extension":
          {
            let content = action.params.content;
            displayUrl = content;
            trimDisplayUrl = false;
            this._setUpDescription(this._actionText, content, true);
            break;
          }
      }
    }

    if (trimDisplayUrl) {
      let input = popup.input;
      if (typeof input.trimValue == "function")
        displayUrl = input.trimValue(displayUrl);
      displayUrl = this._unescapeUrl(displayUrl);
    }
    // For performance reasons we may want to limit the displayUrl size.
    if (popup.textRunsMaxLen && displayUrl) {
      displayUrl = displayUrl.substr(0, popup.textRunsMaxLen);
    }
    this.setAttribute("displayurl", displayUrl);

    // Show the domain as the title if we don't have a title.
    if (!title) {
      titleLooksLikeUrl = true;
      try {
        let uri = Services.io.newURI(originalUrl);
        // Not all valid URLs have a domain.
        if (uri.host)
          title = uri.host;
      } catch (e) {}
      if (!title)
        title = displayUrl;
    }

    this._tags.setAttribute("empty", "true");

    if (type == "tag" || type == "bookmark-tag") {
      // The title is separated from the tags by an endash
      let tags;
      [, title, tags] = title.match(/^(.+) \u2013 (.+)$/);

      // Each tag is split by a comma in an undefined order, so sort it
      let sortedTags = tags.split(/\s*,\s*/).sort((a, b) => {
        return a.localeCompare(a);
      });

      let anyTagsMatch = this._setUpTags(sortedTags);
      if (anyTagsMatch) {
        this._tags.removeAttribute("empty");
      }
      if (type == "bookmark-tag") {
        type = "bookmark";
      }
    } else if (type == "keyword") {
      // Note that this is a moz-action with action.type == keyword.
      emphasiseUrl = false;
      let keywordArg = this.getAttribute("text").replace(/^[^\s]+\s*/, "");
      if (!keywordArg) {
        // Treat keyword searches without arguments as visiturl actions.
        type = "visiturl";
        this.setAttribute("actiontype", "visiturl");
        let visitStr = this._stringBundle.GetStringFromName("visit");
        this._setUpDescription(this._actionText, visitStr, true);
      } else {
        let pairs = [
          [title, ""],
          [keywordArg, "match"],
        ];
        let interpStr =
          this._stringBundle.GetStringFromName("bookmarkKeywordSearch");
        title = this._generateEmphasisPairs(interpStr, pairs);
        // The action box will be visible since this is a moz-action, but
        // we want it to appear as if it were not visible, so set its text
        // to the empty string.
        this._setUpDescription(this._actionText, "", false);
      }
    }

    this.setAttribute("type", type);

    if (titleLooksLikeUrl) {
      this._titleText.setAttribute("lookslikeurl", "true");
    } else {
      this._titleText.removeAttribute("lookslikeurl");
    }

    if (Array.isArray(title)) {
      // For performance reasons we may want to limit the title size.
      if (popup.textRunsMaxLen) {
        title.forEach(t => {
          // Limit all the even items.
          for (let i = 0; i < t.length; i += 2) {
            t[i] = t[i].substr(0, popup.textRunsMaxLen);
          }
        });
      }
      this._setUpEmphasisedSections(this._titleText, title);
    } else {
      // For performance reasons we may want to limit the title size.
      if (popup.textRunsMaxLen && title) {
        title = title.substr(0, popup.textRunsMaxLen);
      }
      this._setUpDescription(this._titleText, title, false);
    }
    this._setUpDescription(this._urlText, displayUrl, !emphasiseUrl);

    // Removing the max-width may be jarring when the item is visible, but
    // we have no other choice to properly crop the text.
    // Removing max-widths may cause overflow or underflow events, that
    // will set the _inOverflow property. In case both the old and the new
    // text are overflowing, the overflow event won't happen, and we must
    // enforce an _handleOverflow() call to update the max-widths.
    let wasInOverflow = this._inOverflow;
    this._removeMaxWidths();
    if (wasInOverflow && this._inOverflow) {
      this._handleOverflow();
    }
  }

  _removeMaxWidths() {
    if (this._hasMaxWidths) {
      this._titleText.style.removeProperty("max-width");
      this._tagsText.style.removeProperty("max-width");
      this._urlText.style.removeProperty("max-width");
      this._actionText.style.removeProperty("max-width");
      this._hasMaxWidths = false;
    }
  }

  /**
   * This method truncates the displayed strings as necessary.
   */
  _handleOverflow() {
    let itemRect = this.parentNode.getBoundingClientRect();
    let titleRect = this._titleText.getBoundingClientRect();
    let tagsRect = this._tagsText.getBoundingClientRect();
    let separatorRect = this._separator.getBoundingClientRect();
    let urlRect = this._urlText.getBoundingClientRect();
    let actionRect = this._actionText.getBoundingClientRect();
    let separatorURLActionWidth =
      separatorRect.width + Math.max(urlRect.width, actionRect.width);

    // Total width for the title and URL/action is the width of the item
    // minus the start of the title text minus a little optional extra padding.
    // This extra padding amount is basically arbitrary but keeps the text
    // from getting too close to the popup's edge.
    let dir = this.getAttribute("dir");
    let titleStart = dir == "rtl" ? itemRect.right - titleRect.right :
      titleRect.left - itemRect.left;

    let popup = this.parentNode.parentNode;
    let itemWidth = itemRect.width - titleStart - popup.overflowPadding -
      (popup.margins ? popup.margins.end : 0);

    if (this._tags.hasAttribute("empty")) {
      // The tags box is not displayed in this case.
      tagsRect.width = 0;
    }

    let titleTagsWidth = titleRect.width + tagsRect.width;
    if (titleTagsWidth + separatorURLActionWidth > itemWidth) {
      // Title + tags + URL/action overflows the item width.

      // The percentage of the item width allocated to the title and tags.
      let titleTagsPct = 0.66;

      let titleTagsAvailable = itemWidth - separatorURLActionWidth;
      let titleTagsMaxWidth = Math.max(
        titleTagsAvailable,
        itemWidth * titleTagsPct
      );
      if (titleTagsWidth > titleTagsMaxWidth) {
        // Title + tags overflows the max title + tags width.

        // The percentage of the title + tags width allocated to the
        // title.
        let titlePct = 0.33;

        let titleAvailable = titleTagsMaxWidth - tagsRect.width;
        let titleMaxWidth = Math.max(
          titleAvailable,
          titleTagsMaxWidth * titlePct
        );
        let tagsAvailable = titleTagsMaxWidth - titleRect.width;
        let tagsMaxWidth = Math.max(
          tagsAvailable,
          titleTagsMaxWidth * (1 - titlePct)
        );
        this._titleText.style.maxWidth = titleMaxWidth + "px";
        this._tagsText.style.maxWidth = tagsMaxWidth + "px";
      }
      let urlActionMaxWidth = Math.max(
        itemWidth - titleTagsWidth,
        itemWidth * (1 - titleTagsPct)
      );
      urlActionMaxWidth -= separatorRect.width;
      this._urlText.style.maxWidth = urlActionMaxWidth + "px";
      this._actionText.style.maxWidth = urlActionMaxWidth + "px";
      this._hasMaxWidths = true;
    }
  }

  handleOverUnderflow() {
    this._removeMaxWidths();
    this._handleOverflow();
  }

  _parseActionUrl(aUrl) {
    if (!aUrl.startsWith("moz-action:"))
      return null;

    // URL is in the format moz-action:ACTION,PARAMS
    // Where PARAMS is a JSON encoded object.
    let [, type, params] = aUrl.match(/^moz-action:([^,]+),(.*)$/);

    let action = {
      type,
    };

    try {
      action.params = JSON.parse(params);
      for (let key in action.params) {
        action.params[key] = decodeURIComponent(action.params[key]);
      }
    } catch (e) {
      // If this failed, we assume that params is not a JSON object, and
      // is instead just a flat string. This may happen for legacy
      // search components.
      action.params = {
        url: params,
      };
    }

    return action;
  }
};

MozXULElement.implementCustomInterface(
  MozElements.MozAutocompleteRichlistitem,
  [Ci.nsIDOMXULSelectControlItemElement]
);

class MozAutocompleteRichlistitemInsecureWarning extends MozElements.MozAutocompleteRichlistitem {
  constructor() {
    super();

    this.addEventListener("click", (event) => {
      if (event.button != 0) {
        return;
      }

      let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
      window.openTrustedLinkIn(baseURL + "insecure-password", "tab", {
        relatedToCurrent: true,
      });
    });
  }

  connectedCallback() {
    if (this.delayConnectedCallback()) {
      return;
    }

    super.connectedCallback();

    // Unlike other autocomplete items, the height of the insecure warning
    // increases by wrapping. So "forceHandleUnderflow" is for container to
    // recalculate an item's height and width.
    this.classList.add("forceHandleUnderflow");
  }

  static get inheritedAttributes() {
    return {
      ".ac-type-icon": "selected,current,type",
      ".ac-site-icon": "src=image,selected,type",
      ".ac-title-text": "selected",
      ".ac-tags-text": "selected",
      ".ac-separator": "selected,actiontype,type",
      ".ac-url": "selected,actiontype",
      ".ac-url-text": "selected",
      ".ac-action": "selected,actiontype",
      ".ac-action-text": "selected",
    };
  }

  get _markup() {
    return `
      <image class="ac-type-icon"></image>
      <image class="ac-site-icon"></image>
      <vbox class="ac-title" align="left">
        <description class="ac-text-overflow-container">
          <description class="ac-title-text"></description>
        </description>
      </vbox>
      <hbox class="ac-tags" align="center">
        <description class="ac-text-overflow-container">
          <description class="ac-tags-text"></description>
        </description>
      </hbox>
      <hbox class="ac-separator" align="center">
        <description class="ac-separator-text" value="—"></description>
      </hbox>
      <hbox class="ac-url" align="center">
        <description class="ac-text-overflow-container">
          <description class="ac-url-text"></description>
        </description>
      </hbox>
      <hbox class="ac-action" align="center">
        <description class="ac-text-overflow-container">
          <description class="ac-action-text"></description>
        </description>
      </hbox>
    `;
  }

  get _learnMoreString() {
    if (!this.__learnMoreString) {
      this.__learnMoreString = Services.strings.createBundle(
        "chrome://passwordmgr/locale/passwordmgr.properties"
      ).
      GetStringFromName("insecureFieldWarningLearnMore");
    }
    return this.__learnMoreString;
  }

  /**
   * Override _getSearchTokens to have the Learn More text emphasized
   */
  _getSearchTokens(aSearch) {
    return [this._learnMoreString.toLowerCase()];
  }
}

class MozAutocompleteRichlistitemLoginsFooter extends MozElements.MozAutocompleteRichlistitem {
  constructor() {
    super();

    function handleEvent(event) {
      if (event.button != 0) {
        return;
      }

      LoginHelper.openPasswordManager(this.ownerGlobal, {
        filterString: this._data.hostname,
        entryPoint: "autocomplete",
      });
    }

    this.addEventListener("click", handleEvent);
  }

  get _data() {
    return JSON.parse(this.getAttribute("ac-value"));
  }

  _adjustAcItem() {
    this._titleText.textContent = this._data.label;
  }
}

class MozAutocompleteRichlistitemLoginWithOrigin extends MozElements.MozRichlistitem {
  connectedCallback() {
    if (this.delayConnectedCallback()) {
      return;
    }

    this.textContent = "";
    this.appendChild(MozXULElement.parseXULToFragment(this._markup));
    this.initializeAttributeInheritance();
    this._adjustAcItem();
  }


  static get inheritedAttributes() {
    return {
      ".login-username": "text=ac-value",
    };
  }

  get _markup() {
    return `
      <div xmlns="http://www.w3.org/1999/xhtml"
           xmlns:xul="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
           class="login-wrapper">
        <xul:image class="ac-site-icon"></xul:image>
        <div class="login-text">
          <div class="login-row login-username"></div>
          <div class="login-row login-origin"></div>
        </div>
      </div>
    `;
  }

  _adjustAcItem() {
    let outerBoxRect = this.parentNode.getBoundingClientRect();

    // Make item fit in popup as XUL box could not constrain
    // item's width
    this.firstElementChild.style.width = outerBoxRect.width + "px";

    let data = JSON.parse(this.getAttribute("ac-label"));
    let originElement = this.querySelector(".login-origin");
    try {
      let uri = Services.io.newURI(data.loginOrigin);
      // Fallback to handle file: URIs
      originElement.textContent = uri.displayHostPort || data.loginOrigin;
    } catch (ex) {
      originElement.textContent = data.loginOrigin;
    }
  }

  _onOverflow() {}

  _onUnderflow() {}

  handleOverUnderflow() {}
}

customElements.define("autocomplete-richlistitem", MozElements.MozAutocompleteRichlistitem, {
  extends: "richlistitem",
});

customElements.define("autocomplete-richlistitem-insecure-warning", MozAutocompleteRichlistitemInsecureWarning, {
  extends: "richlistitem",
});

customElements.define("autocomplete-richlistitem-logins-footer", MozAutocompleteRichlistitemLoginsFooter, {
  extends: "richlistitem",
});

customElements.define("autocomplete-richlistitem-login-with-origin", MozAutocompleteRichlistitemLoginWithOrigin, {
  extends: "richlistitem",
});
}
