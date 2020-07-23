/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  const MozPopupElement = MozElements.MozElementMixin(XULPopupElement);
  MozElements.MozAutocompleteRichlistboxPopup = class MozAutocompleteRichlistboxPopup extends MozPopupElement {
    constructor() {
      super();

      this.mInput = null;
      this.mPopupOpen = false;
      this._currentIndex = 0;
      this._disabledItemClicked = false;

      this.setListeners();
    }

    initialize() {
      this.setAttribute("ignorekeys", "true");
      this.setAttribute("level", "top");
      this.setAttribute("consumeoutsideclicks", "never");

      this.textContent = "";
      this.appendChild(this.constructor.fragment);

      /**
       * This is the default number of rows that we give the autocomplete
       * popup when the textbox doesn't have a "maxrows" attribute
       * for us to use.
       */
      this.defaultMaxRows = 6;

      /**
       * In some cases (e.g. when the input's dropmarker button is clicked),
       * the input wants to display a popup with more rows. In that case, it
       * should increase its maxRows property and store the "normal" maxRows
       * in this field. When the popup is hidden, we restore the input's
       * maxRows to the value stored in this field.
       *
       * This field is set to -1 between uses so that we can tell when it's
       * been set by the input and when we need to set it in the popupshowing
       * handler.
       */
      this._normalMaxRows = -1;
      this._previousSelectedIndex = -1;
      this.mLastMoveTime = Date.now();
      this.mousedOverIndex = -1;
      this._richlistbox = this.querySelector(".autocomplete-richlistbox");

      if (!this.listEvents) {
        this.listEvents = {
          handleEvent: event => {
            if (!this.parentNode) {
              return;
            }

            switch (event.type) {
              case "mousedown":
                this._disabledItemClicked = !!event.target.closest(
                  "richlistitem"
                )?.disabled;
                break;
              case "mouseup":
                // Don't call onPopupClick for the scrollbar buttons, thumb,
                // slider, etc. If we hit the richlistbox and not a
                // richlistitem, we ignore the event.
                if (
                  event.target.closest("richlistbox,richlistitem").localName ==
                    "richlistitem" &&
                  !this._disabledItemClicked
                ) {
                  this.onPopupClick(event);
                }
                this._disabledItemClicked = false;
                break;
              case "mousemove":
                if (Date.now() - this.mLastMoveTime <= 30) {
                  return;
                }

                let item = event.target.closest("richlistbox,richlistitem");

                // If we hit the richlistbox and not a richlistitem, we ignore
                // the event.
                if (item.localName == "richlistbox") {
                  return;
                }

                let index = this.richlistbox.getIndexOfItem(item);

                this.mousedOverIndex = index;

                if (item.selectedByMouseOver) {
                  this.richlistbox.selectedIndex = index;
                }

                this.mLastMoveTime = Date.now();
                break;
            }
          },
        };
      }
      this.richlistbox.addEventListener("mousedown", this.listEvents);
      this.richlistbox.addEventListener("mouseup", this.listEvents);
      this.richlistbox.addEventListener("mousemove", this.listEvents);
    }

    get richlistbox() {
      if (!this._richlistbox) {
        this.initialize();
      }
      return this._richlistbox;
    }

    static get markup() {
      return `
      <richlistbox class="autocomplete-richlistbox" flex="1"/>
    `;
    }

    /**
     * nsIAutoCompletePopup
     */
    get input() {
      return this.mInput;
    }

    get overrideValue() {
      return null;
    }

    get popupOpen() {
      return this.mPopupOpen;
    }

    get maxRows() {
      return (this.mInput && this.mInput.maxRows) || this.defaultMaxRows;
    }

    set selectedIndex(val) {
      if (val != this.richlistbox.selectedIndex) {
        this._previousSelectedIndex = this.richlistbox.selectedIndex;
      }
      this.richlistbox.selectedIndex = val;
      // Since ensureElementIsVisible may cause an expensive Layout flush,
      // invoke it only if there may be a scrollbar, so if we could fetch
      // more results than we can show at once.
      // maxResults is the maximum number of fetched results, maxRows is the
      // maximum number of rows we show at once, without a scrollbar.
      if (this.mPopupOpen && this.maxResults > this.maxRows) {
        // when clearing the selection (val == -1, so selectedItem will be
        // null), we want to scroll back to the top.  see bug #406194
        this.richlistbox.ensureElementIsVisible(
          this.richlistbox.selectedItem || this.richlistbox.firstElementChild
        );
      }
      return val;
    }

    get selectedIndex() {
      return this.richlistbox.selectedIndex;
    }

    get maxResults() {
      // This is how many richlistitems will be kept around.
      // Note, this getter may be overridden, or instances
      // can have the nomaxresults attribute set to have no
      // limit.
      if (this.getAttribute("nomaxresults") == "true") {
        return Infinity;
      }
      return 20;
    }

    get matchCount() {
      return Math.min(this.mInput.controller.matchCount, this.maxResults);
    }

    get overflowPadding() {
      return Number(this.getAttribute("overflowpadding"));
    }

    set view(val) {
      return val;
    }

    get view() {
      return this.mInput.controller;
    }

    closePopup() {
      if (this.mPopupOpen) {
        this.hidePopup();
        this.removeAttribute("width");
      }
    }

    getNextIndex(aReverse, aAmount, aIndex, aMaxRow) {
      if (aMaxRow < 0) {
        return -1;
      }

      var newIdx = aIndex + (aReverse ? -1 : 1) * aAmount;
      if (
        (aReverse && aIndex == -1) ||
        (newIdx > aMaxRow && aIndex != aMaxRow)
      ) {
        newIdx = aMaxRow;
      } else if ((!aReverse && aIndex == -1) || (newIdx < 0 && aIndex != 0)) {
        newIdx = 0;
      }

      if (
        (newIdx < 0 && aIndex == 0) ||
        (newIdx > aMaxRow && aIndex == aMaxRow)
      ) {
        aIndex = -1;
      } else {
        aIndex = newIdx;
      }

      return aIndex;
    }

    onPopupClick(aEvent) {
      this.input.controller.handleEnter(true, aEvent);
    }

    onSearchBegin() {
      this.mousedOverIndex = -1;

      if (typeof this._onSearchBegin == "function") {
        this._onSearchBegin();
      }
    }

    openAutocompletePopup(aInput, aElement) {
      // until we have "baseBinding", (see bug #373652) this allows
      // us to override openAutocompletePopup(), but still call
      // the method on the base class
      this._openAutocompletePopup(aInput, aElement);
    }

    _openAutocompletePopup(aInput, aElement) {
      if (!this._richlistbox) {
        this.initialize();
      }

      if (!this.mPopupOpen) {
        // It's possible that the panel is hidden initially
        // to avoid impacting startup / new window performance
        aInput.popup.hidden = false;

        this.mInput = aInput;
        // clear any previous selection, see bugs 400671 and 488357
        this.selectedIndex = -1;

        var width = aElement.getBoundingClientRect().width;
        this.setAttribute("width", width > 100 ? width : 100);
        // invalidate() depends on the width attribute
        this._invalidate();

        this.openPopup(aElement, "after_start", 0, 0, false, false);
      }
    }

    invalidate(reason) {
      // Don't bother doing work if we're not even showing
      if (!this.mPopupOpen) {
        return;
      }

      this._invalidate(reason);
    }

    _invalidate(reason) {
      // collapsed if no matches
      this.richlistbox.collapsed = this.matchCount == 0;

      // Update the richlistbox height.
      if (this._adjustHeightRAFToken) {
        cancelAnimationFrame(this._adjustHeightRAFToken);
        this._adjustHeightRAFToken = null;
      }

      if (this.mPopupOpen) {
        delete this._adjustHeightOnPopupShown;
        this._adjustHeightRAFToken = requestAnimationFrame(() =>
          this.adjustHeight()
        );
      } else {
        this._adjustHeightOnPopupShown = true;
      }

      this._currentIndex = 0;
      if (this._appendResultTimeout) {
        clearTimeout(this._appendResultTimeout);
      }
      this._appendCurrentResult(reason);
    }

    _collapseUnusedItems() {
      let existingItemsCount = this.richlistbox.children.length;
      for (let i = this.matchCount; i < existingItemsCount; ++i) {
        let item = this.richlistbox.children[i];

        item.collapsed = true;
        if (typeof item._onCollapse == "function") {
          item._onCollapse();
        }
      }
    }

    adjustHeight() {
      // Figure out how many rows to show
      let rows = this.richlistbox.children;
      let numRows = Math.min(this.matchCount, this.maxRows, rows.length);

      // Default the height to 0 if we have no rows to show
      let height = 0;
      if (numRows) {
        let firstRowRect = rows[0].getBoundingClientRect();
        if (this._rlbPadding == undefined) {
          let style = window.getComputedStyle(this.richlistbox);
          let paddingTop = parseInt(style.paddingTop) || 0;
          let paddingBottom = parseInt(style.paddingBottom) || 0;
          this._rlbPadding = paddingTop + paddingBottom;
        }

        // The class `forceHandleUnderflow` is for the item might need to
        // handle OverUnderflow or Overflow when the height of an item will
        // be changed dynamically.
        for (let i = 0; i < numRows; i++) {
          if (rows[i].classList.contains("forceHandleUnderflow")) {
            rows[i].handleOverUnderflow();
          }
        }

        let lastRowRect = rows[numRows - 1].getBoundingClientRect();
        // Calculate the height to have the first row to last row shown
        height = lastRowRect.bottom - firstRowRect.top + this._rlbPadding;
      }

      this._collapseUnusedItems();

      this.richlistbox.style.removeProperty("height");
      // We need to get the ceiling of the calculated value to ensure that the box fully contains
      // all of its contents and doesn't cause a scrollbar since nsIBoxObject only expects a
      // `long`. e.g. if `height` is 99.5 the richlistbox would render at height 99px with a
      // scrollbar for the extra 0.5px.
      this.richlistbox.height = Math.ceil(height);
    }

    _appendCurrentResult(invalidateReason) {
      var controller = this.mInput.controller;
      var matchCount = this.matchCount;
      var existingItemsCount = this.richlistbox.children.length;

      // Process maxRows per chunk to improve performance and user experience
      for (let i = 0; i < this.maxRows; i++) {
        if (this._currentIndex >= matchCount) {
          break;
        }
        let item;
        let itemExists = this._currentIndex < existingItemsCount;

        let originalValue, originalText, originalType;
        let style = controller.getStyleAt(this._currentIndex);
        let value =
          style && style.includes("autofill")
            ? controller.getFinalCompleteValueAt(this._currentIndex)
            : controller.getValueAt(this._currentIndex);
        let label = controller.getLabelAt(this._currentIndex);
        let comment = controller.getCommentAt(this._currentIndex);
        let image = controller.getImageAt(this._currentIndex);
        // trim the leading/trailing whitespace
        let trimmedSearchString = controller.searchString
          .replace(/^\s+/, "")
          .replace(/\s+$/, "");

        let reusable = false;
        if (itemExists) {
          item = this.richlistbox.children[this._currentIndex];

          // Url may be a modified version of value, see _adjustAcItem().
          originalValue =
            item.getAttribute("url") || item.getAttribute("ac-value");
          originalText = item.getAttribute("ac-text");
          originalType = item.getAttribute("originaltype");

          // The styles on the list which have different <content> structure and overrided
          // _adjustAcItem() are unreusable.
          const UNREUSEABLE_STYLES = [
            "autofill-profile",
            "autofill-footer",
            "autofill-clear-button",
            "autofill-insecureWarning",
            "generatedPassword",
            "importableLogins",
            "insecureWarning",
            "loginsFooter",
            "loginWithOrigin",
          ];
          // Reuse the item when its style is exactly equal to the previous style or
          // neither of their style are in the UNREUSEABLE_STYLES.
          reusable =
            originalType === style ||
            !(
              UNREUSEABLE_STYLES.includes(style) ||
              UNREUSEABLE_STYLES.includes(originalType)
            );
        }

        // If no reusable item available, then create a new item.
        if (!reusable) {
          let options = null;
          switch (style) {
            case "autofill-profile":
              options = { is: "autocomplete-profile-listitem" };
              break;
            case "autofill-footer":
              options = { is: "autocomplete-profile-listitem-footer" };
              break;
            case "autofill-clear-button":
              options = { is: "autocomplete-profile-listitem-clear-button" };
              break;
            case "autofill-insecureWarning":
              options = { is: "autocomplete-creditcard-insecure-field" };
              break;
            case "importableLogins":
              options = { is: "autocomplete-importable-logins-richlistitem" };
              break;
            case "generatedPassword":
              options = { is: "autocomplete-generated-password-richlistitem" };
              break;
            case "insecureWarning":
              options = { is: "autocomplete-richlistitem-insecure-warning" };
              break;
            case "loginsFooter":
              options = { is: "autocomplete-richlistitem-logins-footer" };
              break;
            case "loginWithOrigin":
              options = { is: "autocomplete-login-richlistitem" };
              break;
            default:
              options = { is: "autocomplete-richlistitem" };
          }
          item = document.createXULElement("richlistitem", options);
          item.className = "autocomplete-richlistitem";
        }

        item.setAttribute("dir", this.style.direction);
        item.setAttribute("ac-image", image);
        item.setAttribute("ac-value", value);
        item.setAttribute("ac-label", label);
        item.setAttribute("ac-comment", comment);
        item.setAttribute("ac-text", trimmedSearchString);

        // Completely reuse the existing richlistitem for invalidation
        // due to new results, but only when: the item is the same, *OR*
        // we are about to replace the currently moused-over item, to
        // avoid surprising the user.
        let iface = Ci.nsIAutoCompletePopup;
        if (
          reusable &&
          originalText == trimmedSearchString &&
          invalidateReason == iface.INVALIDATE_REASON_NEW_RESULT &&
          (originalValue == value ||
            this.mousedOverIndex === this._currentIndex)
        ) {
          // try to re-use the existing item
          item._reuseAcItem();
          this._currentIndex++;
          continue;
        } else {
          if (typeof item._cleanup == "function") {
            item._cleanup();
          }
          item.setAttribute("originaltype", style);
        }

        if (reusable) {
          // Adjust only when the result's type is reusable for existing
          // item's. Otherwise, we might insensibly call old _adjustAcItem()
          // as new binding has not been attached yet.
          // We don't need to worry about switching to new binding, since
          // _adjustAcItem() will fired by its own constructor accordingly.
          item._adjustAcItem();
          item.collapsed = false;
        } else if (itemExists) {
          let oldItem = this.richlistbox.children[this._currentIndex];
          this.richlistbox.replaceChild(item, oldItem);
        } else {
          this.richlistbox.appendChild(item);
        }

        this._currentIndex++;
      }

      if (typeof this.onResultsAdded == "function") {
        // The items bindings may not be attached yet, so we must delay this
        // before we can properly handle items properly without breaking
        // the richlistbox.
        Services.tm.dispatchToMainThread(() => this.onResultsAdded());
      }

      if (this._currentIndex < matchCount) {
        // yield after each batch of items so that typing the url bar is
        // responsive
        this._appendResultTimeout = setTimeout(
          () => this._appendCurrentResult(),
          0
        );
      }
    }

    selectBy(aReverse, aPage) {
      try {
        var amount = aPage ? 5 : 1;

        // because we collapsed unused items, we can't use this.richlistbox.getRowCount(), we need to use the matchCount
        this.selectedIndex = this.getNextIndex(
          aReverse,
          amount,
          this.selectedIndex,
          this.matchCount - 1
        );
        if (this.selectedIndex == -1) {
          this.input._focus();
        }
      } catch (ex) {
        // do nothing - occasionally timer-related js errors happen here
        // e.g. "this.selectedIndex has no properties", when you type fast and hit a
        // navigation key before this popup has opened
      }
    }

    disconnectedCallback() {
      if (this.listEvents) {
        this.richlistbox.removeEventListener("mousedown", this.listEvents);
        this.richlistbox.removeEventListener("mouseup", this.listEvents);
        this.richlistbox.removeEventListener("mousemove", this.listEvents);
        delete this.listEvents;
      }
    }

    setListeners() {
      this.addEventListener("popupshowing", event => {
        // If normalMaxRows wasn't already set by the input, then set it here
        // so that we restore the correct number when the popup is hidden.

        // Null-check this.mInput; see bug 1017914
        if (this._normalMaxRows < 0 && this.mInput) {
          this._normalMaxRows = this.mInput.maxRows;
        }

        this.mPopupOpen = true;
      });

      this.addEventListener("popupshown", event => {
        if (this._adjustHeightOnPopupShown) {
          delete this._adjustHeightOnPopupShown;
          this.adjustHeight();
        }
      });

      this.addEventListener("popuphiding", event => {
        var isListActive = true;
        if (this.selectedIndex == -1) {
          isListActive = false;
        }
        this.input.controller.stopSearch();

        this.mPopupOpen = false;

        // Reset the maxRows property to the cached "normal" value (if there's
        // any), and reset normalMaxRows so that we can detect whether it was set
        // by the input when the popupshowing handler runs.

        // Null-check this.mInput; see bug 1017914
        if (this.mInput && this._normalMaxRows > 0) {
          this.mInput.maxRows = this._normalMaxRows;
        }
        this._normalMaxRows = -1;
        // If the list was being navigated and then closed, make sure
        // we fire accessible focus event back to textbox

        // Null-check this.mInput; see bug 1017914
        if (isListActive && this.mInput) {
          this.mInput.mIgnoreFocus = true;
          this.mInput._focus();
          this.mInput.mIgnoreFocus = false;
        }
      });
    }
  };

  MozPopupElement.implementCustomInterface(
    MozElements.MozAutocompleteRichlistboxPopup,
    [Ci.nsIAutoCompletePopup]
  );

  customElements.define(
    "autocomplete-richlistbox-popup",
    MozElements.MozAutocompleteRichlistboxPopup,
    {
      extends: "panel",
    }
  );
}
