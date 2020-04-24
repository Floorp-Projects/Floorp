/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  const { AppConstants } = ChromeUtils.import(
    "resource://gre/modules/AppConstants.jsm"
  );

  /**
   * XUL:richlistbox element.
   */
  MozElements.RichListBox = class RichListBox extends MozElements.BaseControl {
    constructor() {
      super();

      this.selectedItems = new ChromeNodeList();
      this._currentIndex = null;
      this._lastKeyTime = 0;
      this._incrementalString = "";
      this._suppressOnSelect = false;
      this._userSelecting = false;
      this._selectTimeout = null;
      this._currentItem = null;
      this._selectionStart = null;

      this.addEventListener(
        "keypress",
        event => {
          if (event.altKey || event.metaKey) {
            return;
          }

          switch (event.keyCode) {
            case KeyEvent.DOM_VK_UP:
              this._moveByOffsetFromUserEvent(-1, event);
              break;
            case KeyEvent.DOM_VK_DOWN:
              this._moveByOffsetFromUserEvent(1, event);
              break;
            case KeyEvent.DOM_VK_HOME:
              this._moveByOffsetFromUserEvent(-this.currentIndex, event);
              break;
            case KeyEvent.DOM_VK_END:
              this._moveByOffsetFromUserEvent(
                this.getRowCount() - this.currentIndex - 1,
                event
              );
              break;
            case KeyEvent.DOM_VK_PAGE_UP:
              this._moveByOffsetFromUserEvent(this.scrollOnePage(-1), event);
              break;
            case KeyEvent.DOM_VK_PAGE_DOWN:
              this._moveByOffsetFromUserEvent(this.scrollOnePage(1), event);
              break;
          }
        },
        { mozSystemGroup: true }
      );

      this.addEventListener("keypress", event => {
        if (event.target != this) {
          return;
        }

        if (
          event.key == " " &&
          event.ctrlKey &&
          !event.shiftKey &&
          !event.altKey &&
          !event.metaKey &&
          this.currentItem &&
          this.selType == "multiple"
        ) {
          this.toggleItemSelection(this.currentItem);
        }

        if (!event.charCode || event.altKey || event.ctrlKey || event.metaKey) {
          return;
        }

        if (event.timeStamp - this._lastKeyTime > 1000) {
          this._incrementalString = "";
        }

        var key = String.fromCharCode(event.charCode).toLowerCase();
        this._incrementalString += key;
        this._lastKeyTime = event.timeStamp;

        // If all letters in the incremental string are the same, just
        // try to match the first one
        var incrementalString = /^(.)\1+$/.test(this._incrementalString)
          ? RegExp.$1
          : this._incrementalString;
        var length = incrementalString.length;

        var rowCount = this.getRowCount();
        var l = this.selectedItems.length;
        var start = l > 0 ? this.getIndexOfItem(this.selectedItems[l - 1]) : -1;
        // start from the first element if none was selected or from the one
        // following the selected one if it's a new or a repeated-letter search
        if (start == -1 || length == 1) {
          start++;
        }

        for (var i = 0; i < rowCount; i++) {
          var k = (start + i) % rowCount;
          var listitem = this.getItemAtIndex(k);
          if (!this._canUserSelect(listitem)) {
            continue;
          }
          // allow richlistitems to specify the string being searched for
          var searchText =
            "searchLabel" in listitem
              ? listitem.searchLabel
              : listitem.getAttribute("label"); // (see also bug 250123)
          searchText = searchText.substring(0, length).toLowerCase();
          if (searchText == incrementalString) {
            this.ensureIndexIsVisible(k);
            this.timedSelect(listitem, this._selectDelay);
            break;
          }
        }
      });

      this.addEventListener("focus", event => {
        if (this.getRowCount() > 0) {
          if (this.currentIndex == -1) {
            this.currentIndex = this.getIndexOfFirstVisibleRow();
            let currentItem = this.getItemAtIndex(this.currentIndex);
            if (currentItem) {
              this.selectItem(currentItem);
            }
          } else {
            this._fireEvent(this.currentItem, "DOMMenuItemActive");
          }
        }
        this._lastKeyTime = 0;
      });

      this.addEventListener("click", event => {
        // clicking into nothing should unselect multiple selections
        if (event.originalTarget == this && this.selType == "multiple") {
          this.clearSelection();
          this.currentItem = null;
        }
      });

      this.addEventListener("MozSwipeGesture", event => {
        // Only handle swipe gestures up and down
        switch (event.direction) {
          case event.DIRECTION_DOWN:
            this.scrollTop = this.scrollHeight;
            break;
          case event.DIRECTION_UP:
            this.scrollTop = 0;
            break;
        }
      });
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }

      this.setAttribute("allowevents", "true");
      this._refreshSelection();
    }

    // nsIDOMXULSelectControlElement
    set selectedItem(val) {
      this.selectItem(val);
    }
    get selectedItem() {
      return this.selectedItems.length ? this.selectedItems[0] : null;
    }

    // nsIDOMXULSelectControlElement
    set selectedIndex(val) {
      if (val >= 0) {
        // This is a micro-optimization so that a call to getIndexOfItem or
        // getItemAtIndex caused by _fireOnSelect (especially for derived
        // widgets) won't loop the children.
        this._selecting = {
          item: this.getItemAtIndex(val),
          index: val,
        };
        this.selectItem(this._selecting.item);
        delete this._selecting;
      } else {
        this.clearSelection();
        this.currentItem = null;
      }
    }
    get selectedIndex() {
      if (this.selectedItems.length) {
        return this.getIndexOfItem(this.selectedItems[0]);
      }
      return -1;
    }

    // nsIDOMXULSelectControlElement
    set value(val) {
      var kids = this.getElementsByAttribute("value", val);
      if (kids && kids.item(0)) {
        this.selectItem(kids[0]);
      }
      return val;
    }
    get value() {
      if (this.selectedItems.length) {
        return this.selectedItem.value;
      }
      return null;
    }

    // nsIDOMXULSelectControlElement
    get itemCount() {
      return this.itemChildren.length;
    }

    // nsIDOMXULSelectControlElement
    set selType(val) {
      this.setAttribute("seltype", val);
      return val;
    }
    get selType() {
      return this.getAttribute("seltype");
    }

    // nsIDOMXULSelectControlElement
    set currentItem(val) {
      if (this._currentItem == val) {
        return val;
      }

      if (this._currentItem) {
        this._currentItem.current = false;
        if (!val && !this.suppressMenuItemEvent) {
          // An item is losing focus and there is no new item to focus.
          // Notify a11y that there is no focused item.
          this._fireEvent(this._currentItem, "DOMMenuItemInactive");
        }
      }
      this._currentItem = val;

      if (val) {
        val.current = true;
        if (!this.suppressMenuItemEvent) {
          // Notify a11y that this item got focus.
          this._fireEvent(val, "DOMMenuItemActive");
        }
      }

      return val;
    }
    get currentItem() {
      return this._currentItem;
    }

    // nsIDOMXULSelectControlElement
    set currentIndex(val) {
      if (val >= 0) {
        this.currentItem = this.getItemAtIndex(val);
      } else {
        this.currentItem = null;
      }
    }
    get currentIndex() {
      return this.currentItem ? this.getIndexOfItem(this.currentItem) : -1;
    }

    // nsIDOMXULSelectControlElement
    get selectedCount() {
      return this.selectedItems.length;
    }

    get itemChildren() {
      let children = Array.from(this.children).filter(
        node => node.localName == "richlistitem"
      );
      return children;
    }

    set suppressOnSelect(val) {
      this.setAttribute("suppressonselect", val);
    }
    get suppressOnSelect() {
      return this.getAttribute("suppressonselect") == "true";
    }

    set _selectDelay(val) {
      this.setAttribute("_selectDelay", val);
    }
    get _selectDelay() {
      return this.getAttribute("_selectDelay") || 50;
    }

    _fireOnSelect() {
      // make sure not to modify last-selected when suppressing select events
      // (otherwise we'll lose the selection when a template gets rebuilt)
      if (this._suppressOnSelect || this.suppressOnSelect) {
        return;
      }

      // remember the current item and all selected items with IDs
      var state = this.currentItem ? this.currentItem.id : "";
      if (this.selType == "multiple" && this.selectedCount) {
        let getId = function getId(aItem) {
          return aItem.id;
        };
        state +=
          " " +
          [...this.selectedItems]
            .filter(getId)
            .map(getId)
            .join(" ");
      }
      if (state) {
        this.setAttribute("last-selected", state);
      } else {
        this.removeAttribute("last-selected");
      }

      // preserve the index just in case no IDs are available
      if (this.currentIndex > -1) {
        this._currentIndex = this.currentIndex + 1;
      }

      var event = document.createEvent("Events");
      event.initEvent("select", true, true);
      this.dispatchEvent(event);

      // always call this (allows a commandupdater without controller)
      document.commandDispatcher.updateCommands("richlistbox-select");
    }

    getNextItem(aStartItem, aDelta) {
      while (aStartItem) {
        aStartItem = aStartItem.nextSibling;
        if (
          aStartItem &&
          aStartItem.localName == "richlistitem" &&
          (!this._userSelecting || this._canUserSelect(aStartItem))
        ) {
          --aDelta;
          if (aDelta == 0) {
            return aStartItem;
          }
        }
      }
      return null;
    }

    getPreviousItem(aStartItem, aDelta) {
      while (aStartItem) {
        aStartItem = aStartItem.previousSibling;
        if (
          aStartItem &&
          aStartItem.localName == "richlistitem" &&
          (!this._userSelecting || this._canUserSelect(aStartItem))
        ) {
          --aDelta;
          if (aDelta == 0) {
            return aStartItem;
          }
        }
      }
      return null;
    }

    appendItem(aLabel, aValue) {
      var item = this.ownerDocument.createXULElement("richlistitem");
      item.setAttribute("value", aValue);

      var label = this.ownerDocument.createXULElement("label");
      label.setAttribute("value", aLabel);
      label.setAttribute("flex", "1");
      label.setAttribute("crop", "end");
      item.appendChild(label);

      this.appendChild(item);

      return item;
    }

    // nsIDOMXULSelectControlElement
    getIndexOfItem(aItem) {
      // don't search the children, if we're looking for none of them
      if (aItem == null) {
        return -1;
      }
      if (this._selecting && this._selecting.item == aItem) {
        return this._selecting.index;
      }
      return this.itemChildren.indexOf(aItem);
    }

    // nsIDOMXULSelectControlElement
    getItemAtIndex(aIndex) {
      if (this._selecting && this._selecting.index == aIndex) {
        return this._selecting.item;
      }
      return this.itemChildren[aIndex] || null;
    }

    // nsIDOMXULMultiSelectControlElement
    addItemToSelection(aItem) {
      if (this.selType != "multiple" && this.selectedCount) {
        return;
      }

      if (aItem.selected) {
        return;
      }

      this.selectedItems.append(aItem);
      aItem.selected = true;

      this._fireOnSelect();
    }

    // nsIDOMXULMultiSelectControlElement
    removeItemFromSelection(aItem) {
      if (!aItem.selected) {
        return;
      }

      this.selectedItems.remove(aItem);
      aItem.selected = false;
      this._fireOnSelect();
    }

    // nsIDOMXULMultiSelectControlElement
    toggleItemSelection(aItem) {
      if (aItem.selected) {
        this.removeItemFromSelection(aItem);
      } else {
        this.addItemToSelection(aItem);
      }
    }

    // nsIDOMXULMultiSelectControlElement
    selectItem(aItem) {
      if (!aItem || aItem.disabled) {
        return;
      }

      if (this.selectedItems.length == 1 && this.selectedItems[0] == aItem) {
        return;
      }

      this._selectionStart = null;

      var suppress = this._suppressOnSelect;
      this._suppressOnSelect = true;

      this.clearSelection();
      this.addItemToSelection(aItem);
      this.currentItem = aItem;

      this._suppressOnSelect = suppress;
      this._fireOnSelect();
    }

    // nsIDOMXULMultiSelectControlElement
    selectItemRange(aStartItem, aEndItem) {
      if (this.selType != "multiple") {
        return;
      }

      if (!aStartItem) {
        aStartItem = this._selectionStart
          ? this._selectionStart
          : this.currentItem;
      }

      if (!aStartItem) {
        aStartItem = aEndItem;
      }

      var suppressSelect = this._suppressOnSelect;
      this._suppressOnSelect = true;

      this._selectionStart = aStartItem;

      var currentItem;
      var startIndex = this.getIndexOfItem(aStartItem);
      var endIndex = this.getIndexOfItem(aEndItem);
      if (endIndex < startIndex) {
        currentItem = aEndItem;
        aEndItem = aStartItem;
        aStartItem = currentItem;
      } else {
        currentItem = aStartItem;
      }

      while (currentItem) {
        this.addItemToSelection(currentItem);
        if (currentItem == aEndItem) {
          currentItem = this.getNextItem(currentItem, 1);
          break;
        }
        currentItem = this.getNextItem(currentItem, 1);
      }

      // Clear around new selection
      // Don't use clearSelection() because it causes a lot of noise
      // with respect to selection removed notifications used by the
      // accessibility API support.
      var userSelecting = this._userSelecting;
      this._userSelecting = false; // that's US automatically unselecting
      for (; currentItem; currentItem = this.getNextItem(currentItem, 1)) {
        this.removeItemFromSelection(currentItem);
      }

      for (
        currentItem = this.getItemAtIndex(0);
        currentItem != aStartItem;
        currentItem = this.getNextItem(currentItem, 1)
      ) {
        this.removeItemFromSelection(currentItem);
      }
      this._userSelecting = userSelecting;

      this._suppressOnSelect = suppressSelect;

      this._fireOnSelect();
    }

    // nsIDOMXULMultiSelectControlElement
    selectAll() {
      this._selectionStart = null;

      var suppress = this._suppressOnSelect;
      this._suppressOnSelect = true;

      var item = this.getItemAtIndex(0);
      while (item) {
        this.addItemToSelection(item);
        item = this.getNextItem(item, 1);
      }

      this._suppressOnSelect = suppress;
      this._fireOnSelect();
    }

    // nsIDOMXULMultiSelectControlElement
    clearSelection() {
      if (this.selectedItems) {
        while (this.selectedItems.length) {
          let item = this.selectedItems[0];
          item.selected = false;
          this.selectedItems.remove(item);
        }
      }

      this._selectionStart = null;
      this._fireOnSelect();
    }

    // nsIDOMXULMultiSelectControlElement
    getSelectedItem(aIndex) {
      return aIndex < this.selectedItems.length
        ? this.selectedItems[aIndex]
        : null;
    }

    ensureIndexIsVisible(aIndex) {
      return this.ensureElementIsVisible(this.getItemAtIndex(aIndex));
    }

    ensureElementIsVisible(aElement, aAlignToTop) {
      if (!aElement) {
        return;
      }

      // These calculations assume that there is no padding on the
      // "richlistbox" element, although there might be a margin.
      var targetRect = aElement.getBoundingClientRect();
      var scrollRect = this.getBoundingClientRect();
      var offset = targetRect.top - scrollRect.top;
      if (!aAlignToTop && offset >= 0) {
        // scrollRect.bottom wouldn't take a horizontal scroll bar into account
        let scrollRectBottom = scrollRect.top + this.clientHeight;
        offset = targetRect.bottom - scrollRectBottom;
        if (offset <= 0) {
          return;
        }
      }
      this.scrollTop += offset;
    }

    getIndexOfFirstVisibleRow() {
      var children = this.itemChildren;

      for (var ix = 0; ix < children.length; ix++) {
        if (this._isItemVisible(children[ix])) {
          return ix;
        }
      }

      return -1;
    }

    getRowCount() {
      return this.itemChildren.length;
    }

    scrollOnePage(aDirection) {
      var children = this.itemChildren;

      if (!children.length) {
        return 0;
      }

      // If nothing is selected, we just select the first element
      // at the extreme we're moving away from
      if (!this.currentItem) {
        return aDirection == -1 ? children.length : 0;
      }

      // If the current item is visible, scroll by one page so that
      // the new current item is at approximately the same position as
      // the existing current item.
      let height = this.getBoundingClientRect().height;
      if (this._isItemVisible(this.currentItem)) {
        this.scrollBy(0, height * aDirection);
      }

      // Figure out, how many items fully fit into the view port
      // (including the currently selected one), and determine
      // the index of the first one lying (partially) outside
      let currentItemRect = this.currentItem.getBoundingClientRect();
      var startBorder = currentItemRect.y;
      if (aDirection == -1) {
        startBorder += currentItemRect.height;
      }

      var index = this.currentIndex;
      for (var ix = index; 0 <= ix && ix < children.length; ix += aDirection) {
        let childRect = children[ix].getBoundingClientRect();
        if (childRect.height == 0) {
          continue; // hidden children have a y of 0
        }
        var endBorder = childRect.y + (aDirection == -1 ? childRect.height : 0);
        if ((endBorder - startBorder) * aDirection > height) {
          break; // we've reached the desired distance
        }
        index = ix;
      }

      return index != this.currentIndex
        ? index - this.currentIndex
        : aDirection;
    }

    _refreshSelection() {
      // when this method is called, we know that either the currentItem
      // and selectedItems we have are null (ctor) or a reference to an
      // element no longer in the DOM (template).

      // first look for the last-selected attribute
      var state = this.getAttribute("last-selected");
      if (state) {
        var ids = state.split(" ");

        var suppressSelect = this._suppressOnSelect;
        this._suppressOnSelect = true;
        this.clearSelection();
        for (let i = 1; i < ids.length; i++) {
          var selectedItem = document.getElementById(ids[i]);
          if (selectedItem) {
            this.addItemToSelection(selectedItem);
          }
        }

        var currentItem = document.getElementById(ids[0]);
        if (!currentItem && this._currentIndex) {
          currentItem = this.getItemAtIndex(
            Math.min(this._currentIndex - 1, this.getRowCount())
          );
        }
        if (currentItem) {
          this.currentItem = currentItem;
          if (this.selType != "multiple" && this.selectedCount == 0) {
            this.selectedItem = currentItem;
          }

          if (this.getBoundingClientRect().height) {
            this.ensureElementIsVisible(currentItem);
          } else {
            // XXX hack around a bug in ensureElementIsVisible as it will
            // scroll beyond the last element, bug 493645.
            this.ensureElementIsVisible(currentItem.previousElementSibling);
          }
        }
        this._suppressOnSelect = suppressSelect;
        // XXX actually it's just a refresh, but at least
        // the Extensions manager expects this:
        this._fireOnSelect();
        return;
      }

      // try to restore the selected items according to their IDs
      // (applies after a template rebuild, if last-selected was not set)
      if (this.selectedItems) {
        let itemIds = [];
        for (let i = this.selectedCount - 1; i >= 0; i--) {
          let selectedItem = this.selectedItems[i];
          itemIds.push(selectedItem.id);
          this.selectedItems.remove(selectedItem);
        }
        for (let i = 0; i < itemIds.length; i++) {
          let selectedItem = document.getElementById(itemIds[i]);
          if (selectedItem) {
            this.selectedItems.append(selectedItem);
          }
        }
      }
      if (this.currentItem && this.currentItem.id) {
        this.currentItem = document.getElementById(this.currentItem.id);
      } else {
        this.currentItem = null;
      }

      // if we have no previously current item or if the above check fails to
      // find the previous nodes (which causes it to clear selection)
      if (!this.currentItem && this.selectedCount == 0) {
        this.currentIndex = this._currentIndex ? this._currentIndex - 1 : 0;

        // cf. listbox constructor:
        // select items according to their attributes
        var children = this.itemChildren;
        for (let i = 0; i < children.length; ++i) {
          if (children[i].getAttribute("selected") == "true") {
            this.selectedItems.append(children[i]);
          }
        }
      }

      if (this.selType != "multiple" && this.selectedCount == 0) {
        this.selectedItem = this.currentItem;
      }
    }

    _isItemVisible(aItem) {
      if (!aItem) {
        return false;
      }

      var y = this.getBoundingClientRect().y;

      // Partially visible items are also considered visible
      let itemRect = aItem.getBoundingClientRect();
      return (
        itemRect.y + itemRect.height > y && itemRect.y < y + this.clientHeight
      );
    }

    moveByOffset(aOffset, aIsSelecting, aIsSelectingRange) {
      if ((aIsSelectingRange || !aIsSelecting) && this.selType != "multiple") {
        return;
      }

      var newIndex = this.currentIndex + aOffset;
      if (newIndex < 0) {
        newIndex = 0;
      }

      var numItems = this.getRowCount();
      if (newIndex > numItems - 1) {
        newIndex = numItems - 1;
      }

      var newItem = this.getItemAtIndex(newIndex);
      // make sure that the item is actually visible/selectable
      if (this._userSelecting && newItem && !this._canUserSelect(newItem)) {
        newItem =
          aOffset > 0
            ? this.getNextItem(newItem, 1) || this.getPreviousItem(newItem, 1)
            : this.getPreviousItem(newItem, 1) || this.getNextItem(newItem, 1);
      }
      if (newItem) {
        this.ensureIndexIsVisible(this.getIndexOfItem(newItem));
        if (aIsSelectingRange) {
          this.selectItemRange(null, newItem);
        } else if (aIsSelecting) {
          this.selectItem(newItem);
        }

        this.currentItem = newItem;
      }
    }

    _moveByOffsetFromUserEvent(aOffset, aEvent) {
      if (!aEvent.defaultPrevented) {
        this._userSelecting = true;
        this.moveByOffset(aOffset, !aEvent.ctrlKey, aEvent.shiftKey);
        this._userSelecting = false;
        aEvent.preventDefault();
      }
    }

    _canUserSelect(aItem) {
      var style = document.defaultView.getComputedStyle(aItem);
      return (
        style.display != "none" &&
        style.visibility == "visible" &&
        style.MozUserInput != "none"
      );
    }

    _selectTimeoutHandler(aMe) {
      aMe._fireOnSelect();
      aMe._selectTimeout = null;
    }

    timedSelect(aItem, aTimeout) {
      var suppress = this._suppressOnSelect;
      if (aTimeout != -1) {
        this._suppressOnSelect = true;
      }

      this.selectItem(aItem);

      this._suppressOnSelect = suppress;

      if (aTimeout != -1) {
        if (this._selectTimeout) {
          window.clearTimeout(this._selectTimeout);
        }
        this._selectTimeout = window.setTimeout(
          this._selectTimeoutHandler,
          aTimeout,
          this
        );
      }
    }

    /**
     * For backwards-compatibility and for convenience.
     * Use ensureElementIsVisible instead
     */
    ensureSelectedElementIsVisible() {
      return this.ensureElementIsVisible(this.selectedItem);
    }

    _fireEvent(aTarget, aName) {
      let event = document.createEvent("Events");
      event.initEvent(aName, true, true);
      aTarget.dispatchEvent(event);
    }
  };

  MozXULElement.implementCustomInterface(MozElements.RichListBox, [
    Ci.nsIDOMXULSelectControlElement,
    Ci.nsIDOMXULMultiSelectControlElement,
  ]);

  customElements.define("richlistbox", MozElements.RichListBox);

  /**
   * XUL:richlistitem element.
   */
  MozElements.MozRichlistitem = class MozRichlistitem extends MozElements.BaseText {
    constructor() {
      super();

      this.selectedByMouseOver = false;

      /**
       * If there is no modifier key, we select on mousedown, not
       * click, so that drags work correctly.
       */
      this.addEventListener("mousedown", event => {
        var control = this.control;
        if (!control || control.disabled) {
          return;
        }
        if (
          (!event.ctrlKey ||
            (AppConstants.platform == "macosx" && event.button == 2)) &&
          !event.shiftKey &&
          !event.metaKey
        ) {
          if (!this.selected) {
            control.selectItem(this);
          }
          control.currentItem = this;
        }
      });

      /**
       * On a click (up+down on the same item), deselect everything
       * except this item.
       */
      this.addEventListener("click", event => {
        if (event.button != 0) {
          return;
        }

        var control = this.control;
        if (!control || control.disabled) {
          return;
        }
        control._userSelecting = true;
        if (control.selType != "multiple") {
          control.selectItem(this);
        } else if (event.ctrlKey || event.metaKey) {
          control.toggleItemSelection(this);
          control.currentItem = this;
        } else if (event.shiftKey) {
          control.selectItemRange(null, this);
          control.currentItem = this;
        } else {
          /* We want to deselect all the selected items except what was
          clicked, UNLESS it was a right-click.  We have to do this
          in click rather than mousedown so that you can drag a
          selected group of items */

          // use selectItemRange instead of selectItem, because this
          // doesn't de- and reselect this item if it is selected
          control.selectItemRange(this, this);
        }
        control._userSelecting = false;
      });
    }

    /**
     * nsIDOMXULSelectControlItemElement
     */
    get label() {
      const XUL_NS =
        "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
      return Array.from(
        this.getElementsByTagNameNS(XUL_NS, "label"),
        label => label.value
      ).join(" ");
    }

    set searchLabel(val) {
      if (val !== null) {
        this.setAttribute("searchlabel", val);
      }
      // fall back to the label property (default value)
      else {
        this.removeAttribute("searchlabel");
      }
      return val;
    }

    get searchLabel() {
      return this.hasAttribute("searchlabel")
        ? this.getAttribute("searchlabel")
        : this.label;
    }
    /**
     * nsIDOMXULSelectControlItemElement
     */
    set value(val) {
      this.setAttribute("value", val);
      return val;
    }

    get value() {
      return this.getAttribute("value");
    }

    /**
     * nsIDOMXULSelectControlItemElement
     */
    set selected(val) {
      if (val) {
        this.setAttribute("selected", "true");
      } else {
        this.removeAttribute("selected");
      }

      return val;
    }

    get selected() {
      return this.getAttribute("selected") == "true";
    }
    /**
     * nsIDOMXULSelectControlItemElement
     */
    get control() {
      var parent = this.parentNode;
      while (parent) {
        if (parent.localName == "richlistbox") {
          return parent;
        }
        parent = parent.parentNode;
      }
      return null;
    }

    set current(val) {
      if (val) {
        this.setAttribute("current", "true");
      } else {
        this.removeAttribute("current");
      }
      return val;
    }

    get current() {
      return this.getAttribute("current") == "true";
    }
  };

  MozXULElement.implementCustomInterface(MozElements.MozRichlistitem, [
    Ci.nsIDOMXULSelectControlItemElement,
  ]);

  customElements.define("richlistitem", MozElements.MozRichlistitem);
}
