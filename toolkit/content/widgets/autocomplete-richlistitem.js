/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
  );

  MozElements.MozAutocompleteRichlistitem = class MozAutocompleteRichlistitem extends MozElements.MozRichlistitem {
    constructor() {
      super();

      /**
       * This overrides listitem's mousedown handler because we want to set the
       * selected item even when the shift or accel keys are pressed.
       */
      this.addEventListener("mousedown", event => {
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

      this.addEventListener("mouseover", event => {
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
        ".ac-separator": "selected,type",
        ".ac-url": "selected",
        ".ac-url-text": "selected",
      };
    }

    get _markup() {
      return `
      <image class="ac-type-icon"/>
      <image class="ac-site-icon"/>
      <hbox class="ac-title" align="center">
        <description class="ac-text-overflow-container">
          <description class="ac-title-text"/>
        </description>
      </hbox>
      <hbox class="ac-separator" align="center">
        <description class="ac-separator-text" value="—"/>
      </hbox>
      <hbox class="ac-url" align="center" aria-hidden="true">
        <description class="ac-text-overflow-container">
          <description class="ac-url-text"/>
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

    get _separator() {
      return this.querySelector(".ac-separator");
    }

    get _urlText() {
      return this.querySelector(".ac-url-text");
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
        this._boundaryCutoff = Services.prefs.getIntPref(
          "toolkit.autocomplete.richBoundaryCutoff"
        );
      }
      return this._boundaryCutoff;
    }

    _cleanup() {
      this.removeAttribute("url");
      this.removeAttribute("image");
      this.removeAttribute("title");
      this.removeAttribute("text");
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
      if (aSearchTokens == "") {
        return [0, aText.length];
      }

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
        return start == 0 ? a[1] - b[1] : start;
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
      if (end < aText.length) {
        boundaries.push(aText.length);
      }

      // Skip the first item because it's always 0
      return boundaries.slice(1);
    }

    _getSearchTokens(aSearch) {
      let search = aSearch.toLowerCase();
      return search.split(/\s+/);
    }

    _setUpDescription(aDescriptionElement, aText) {
      // Get rid of all previous text
      if (!aDescriptionElement) {
        return;
      }
      while (aDescriptionElement.hasChildNodes()) {
        aDescriptionElement.firstChild.remove();
      }

      // Get the indices that separate match and non-match text
      let search = this.getAttribute("text");
      let tokens = this._getSearchTokens(search);
      let indices = this._getBoundaryIndices(aText, tokens);

      this._appendDescriptionSpans(
        indices,
        aText,
        aDescriptionElement,
        aDescriptionElement
      );
    }

    _appendDescriptionSpans(
      indices,
      text,
      spansParentElement,
      descriptionElement
    ) {
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
            document.createElementNS("http://www.w3.org/1999/xhtml", "span")
          );
          this._setUpEmphasisSpan(span, descriptionElement);
          span.textContent = spanText;
        } else {
          // Otherwise, it's plain text
          spansParentElement.appendChild(document.createTextNode(spanText));
        }
      }
    }

    _setUpEmphasisSpan(aSpan, aDescriptionElement) {
      aSpan.classList.add("ac-emphasize-text");
      switch (aDescriptionElement) {
        case this._titleText:
          aSpan.classList.add("ac-emphasize-text-title");
          break;
        case this._urlText:
          aSpan.classList.add("ac-emphasize-text-url");
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
        if (part.length === 0) {
          continue;
        }

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
      while (aDescriptionElement.hasChildNodes()) {
        aDescriptionElement.firstChild.remove();
      }

      for (let [text, emphasise] of aTextPairs) {
        if (emphasise) {
          let span = aDescriptionElement.appendChild(
            document.createElementNS("http://www.w3.org/1999/xhtml", "span")
          );
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
      return Services.textToSubURI.unEscapeURIForUI(url);
    }

    _reuseAcItem() {
      this.collapsed = false;

      // The popup may have changed size between now and the last
      // time the item was shown, so always handle over/underflow.
      let dwu = window.windowUtils;
      let popupWidth = dwu.getBoundsWithoutFlushing(this.parentNode).width;
      if (!this._previousPopupWidth || this._previousPopupWidth != popupWidth) {
        this._previousPopupWidth = popupWidth;
        this.handleOverUnderflow();
      }
    }

    _adjustAcItem() {
      let originalUrl = this.getAttribute("ac-value");
      let title = this.getAttribute("ac-comment");
      this.setAttribute("url", originalUrl);
      this.setAttribute("image", this.getAttribute("ac-image"));
      this.setAttribute("title", title);
      this.setAttribute("text", this.getAttribute("ac-text"));

      let type = this.getAttribute("originaltype");
      let types = new Set(type.split(/\s+/));
      // Remove types that should ultimately not be in the `type` string.
      types.delete("autofill");
      type = [...types][0] || "";
      this.setAttribute("type", type);

      let displayUrl = this._unescapeUrl(originalUrl);

      // Show the domain as the title if we don't have a title.
      if (!title) {
        try {
          let uri = Services.io.newURI(originalUrl);
          // Not all valid URLs have a domain.
          if (uri.host) {
            title = uri.host;
          }
        } catch (e) {}
        if (!title) {
          title = displayUrl;
        }
      }

      if (Array.isArray(title)) {
        this._setUpEmphasisedSections(this._titleText, title);
      } else {
        this._setUpDescription(this._titleText, title);
      }
      this._setUpDescription(this._urlText, displayUrl);

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
        this._urlText.style.removeProperty("max-width");
        this._hasMaxWidths = false;
      }
    }

    /**
     * This method truncates the displayed strings as necessary.
     */
    _handleOverflow() {
      let itemRect = this.parentNode.getBoundingClientRect();
      let titleRect = this._titleText.getBoundingClientRect();
      let separatorRect = this._separator.getBoundingClientRect();
      let urlRect = this._urlText.getBoundingClientRect();
      let separatorURLWidth = separatorRect.width + urlRect.width;

      // Total width for the title and URL is the width of the item
      // minus the start of the title text minus a little optional extra padding.
      // This extra padding amount is basically arbitrary but keeps the text
      // from getting too close to the popup's edge.
      let dir = this.getAttribute("dir");
      let titleStart =
        dir == "rtl"
          ? itemRect.right - titleRect.right
          : titleRect.left - itemRect.left;

      let popup = this.parentNode.parentNode;
      let itemWidth =
        itemRect.width -
        titleStart -
        popup.overflowPadding -
        (popup.margins ? popup.margins.end : 0);

      let titleWidth = titleRect.width;
      if (titleWidth + separatorURLWidth > itemWidth) {
        // The percentage of the item width allocated to the title.
        let titlePct = 0.66;

        let titleAvailable = itemWidth - separatorURLWidth;
        let titleMaxWidth = Math.max(titleAvailable, itemWidth * titlePct);
        if (titleWidth > titleMaxWidth) {
          this._titleText.style.maxWidth = titleMaxWidth + "px";
        }
        let urlMaxWidth = Math.max(
          itemWidth - titleWidth,
          itemWidth * (1 - titlePct)
        );
        urlMaxWidth -= separatorRect.width;
        this._urlText.style.maxWidth = urlMaxWidth + "px";
        this._hasMaxWidths = true;
      }
    }

    handleOverUnderflow() {
      this._removeMaxWidths();
      this._handleOverflow();
    }
  };

  MozXULElement.implementCustomInterface(
    MozElements.MozAutocompleteRichlistitem,
    [Ci.nsIDOMXULSelectControlItemElement]
  );

  class MozAutocompleteRichlistitemInsecureWarning extends MozElements.MozAutocompleteRichlistitem {
    constructor() {
      super();

      this.addEventListener("click", event => {
        if (event.button != 0) {
          return;
        }

        let baseURL = Services.urlFormatter.formatURLPref(
          "app.support.baseURL"
        );
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
        ".ac-separator": "selected,type",
        ".ac-url": "selected",
        ".ac-url-text": "selected",
      };
    }

    get _markup() {
      return `
      <image class="ac-type-icon"/>
      <image class="ac-site-icon"/>
      <vbox class="ac-title" align="left">
        <description class="ac-text-overflow-container">
          <description class="ac-title-text"/>
        </description>
      </vbox>
      <hbox class="ac-separator" align="center">
        <description class="ac-separator-text" value="—"/>
      </hbox>
      <hbox class="ac-url" align="center">
        <description class="ac-text-overflow-container">
          <description class="ac-url-text"/>
        </description>
      </hbox>
    `;
    }

    get _learnMoreString() {
      if (!this.__learnMoreString) {
        this.__learnMoreString = Services.strings
          .createBundle("chrome://passwordmgr/locale/passwordmgr.properties")
          .GetStringFromName("insecureFieldWarningLearnMore");
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

        const { LoginHelper } = ChromeUtils.import(
          "resource://gre/modules/LoginHelper.jsm"
        );

        // ac-label gets populated from getCommentAt despite the attribute name.
        // The "comment" is used to populate additional visible text.
        let { formHostname } = JSON.parse(this.getAttribute("ac-label"));

        LoginHelper.openPasswordManager(this.ownerGlobal, {
          filterString: formHostname,
          entryPoint: "autocomplete",
        });
      }

      this.addEventListener("click", handleEvent);
    }
  }

  class MozAutocompleteTwoLineRichlistitem extends MozElements.MozRichlistitem {
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
        // getLabelAt:
        ".line1-label": "text=ac-value",
        // getCommentAt:
        ".line2-label": "text=ac-label",
      };
    }

    get _markup() {
      return `
      <div xmlns="http://www.w3.org/1999/xhtml"
           xmlns:xul="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
           class="two-line-wrapper">
        <xul:image class="ac-site-icon"></xul:image>
        <div class="labels-wrapper">
          <div class="label-row line1-label"></div>
          <div class="label-row line2-label"></div>
        </div>
      </div>
    `;
    }

    _adjustAcItem() {
      let outerBoxRect = this.parentNode.getBoundingClientRect();

      // Make item fit in popup as XUL box could not constrain
      // item's width
      this.firstElementChild.style.width = outerBoxRect.width + "px";
    }

    _onOverflow() {}

    _onUnderflow() {}

    handleOverUnderflow() {}
  }

  class MozAutocompleteLoginRichlistitem extends MozAutocompleteTwoLineRichlistitem {
    static get inheritedAttributes() {
      return {
        // getLabelAt:
        ".line1-label": "text=ac-value",
        // Don't inherit ac-label with getCommentAt since the label is JSON.
      };
    }

    _adjustAcItem() {
      super._adjustAcItem();

      let details = JSON.parse(this.getAttribute("ac-label"));
      this.querySelector(".line2-label").textContent = details.comment;
    }
  }

  class MozAutocompleteGeneratedPasswordRichlistitem extends MozAutocompleteTwoLineRichlistitem {
    constructor() {
      super();

      // Line 2 and line 3 both display text with a different line-height than
      // line 1 but we want the line-height to be the same so we wrap the text
      // in <span> and only adjust the line-height via font CSS properties on them.
      this.generatedPasswordText = document.createElement("span");

      this.line3Text = document.createElement("span");
      this.line3 = document.createElement("div");
      this.line3.className = "label-row generated-password-autosave";
      this.line3.append(this.line3Text);
    }

    get _autoSaveString() {
      if (!this.__autoSaveString) {
        let brandShorterName = Services.strings
          .createBundle("chrome://branding/locale/brand.properties")
          .GetStringFromName("brandShorterName");
        this.__autoSaveString = Services.strings
          .createBundle("chrome://passwordmgr/locale/passwordmgr.properties")
          .formatStringFromName("generatedPasswordWillBeSaved", [
            brandShorterName,
          ]);
      }
      return this.__autoSaveString;
    }

    _adjustAcItem() {
      let { generatedPassword, willAutoSaveGeneratedPassword } = JSON.parse(
        this.getAttribute("ac-label")
      );
      let line2Label = this.querySelector(".line2-label");
      line2Label.textContent = "";
      this.generatedPasswordText.textContent = generatedPassword;
      line2Label.append(this.generatedPasswordText);

      if (willAutoSaveGeneratedPassword) {
        this.line3Text.textContent = this._autoSaveString;
        this.querySelector(".labels-wrapper").append(this.line3);
      } else {
        this.line3.remove();
      }

      super._adjustAcItem();
    }
  }

  customElements.define(
    "autocomplete-richlistitem",
    MozElements.MozAutocompleteRichlistitem,
    {
      extends: "richlistitem",
    }
  );

  customElements.define(
    "autocomplete-richlistitem-insecure-warning",
    MozAutocompleteRichlistitemInsecureWarning,
    {
      extends: "richlistitem",
    }
  );

  customElements.define(
    "autocomplete-richlistitem-logins-footer",
    MozAutocompleteRichlistitemLoginsFooter,
    {
      extends: "richlistitem",
    }
  );

  customElements.define(
    "autocomplete-two-line-richlistitem",
    MozAutocompleteTwoLineRichlistitem,
    {
      extends: "richlistitem",
    }
  );

  customElements.define(
    "autocomplete-login-richlistitem",
    MozAutocompleteLoginRichlistitem,
    {
      extends: "richlistitem",
    }
  );

  customElements.define(
    "autocomplete-generated-password-richlistitem",
    MozAutocompleteGeneratedPasswordRichlistitem,
    {
      extends: "richlistitem",
    }
  );
}
