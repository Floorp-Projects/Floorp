/**
 * SelectHelperUI: Provides an interface for making a choice in a list.
 *   Supports simultaneous selection of choices and group headers.
 */
var SelectHelperUI = {
  _selectedIndexes: null,
  _list: null,

  get _container() {
    delete this._container;
    return this._container = document.getElementById("select-container");
  },

  get _listbox() {
    delete this._listbox;
    return this._listbox = document.getElementById("select-commands");
  },

  get _title() {
    delete this._title;
    return this._title = document.getElementById("select-title");
  },

  show: function selectHelperShow(aList, aTitle) {
    if (this._list)
      this.reset();

    this._list = aList;

    // The element label is used as a title to give more context
    this._title.value = aTitle || "";
    this._container.setAttribute("multiple", aList.multiple ? "true" : "false");

    // Save already selected indexes to detect what has changed when a a new
    // element is selected
    this._selectedIndexes = this._getSelectedIndexes();
    let firstSelected = null;

    // Using a fragment prevent us to hang on huge list
    let fragment = document.createDocumentFragment();
    let choices = aList.choices;
    for (let i = 0; i < choices.length; i++) {
      let choice = choices[i];
      let item = document.createElement("listitem");

      item.setAttribute("class", "option-command listitem-iconic action-button");
      item.setAttribute("image", "");
      item.setAttribute("flex", "1");
      item.setAttribute("crop", "center");
      item.setAttribute("label", choice.text);

      choice.selected ? item.classList.add("selected")
                      : item.classList.remove("selected");

      choice.disabled ? item.setAttribute("disabled", "true")
                      : item.removeAttribute("disabled");
      fragment.appendChild(item);

      if (choice.group) {
        item.classList.add("optgroup");
        continue;
      }

      item.optionIndex = choice.optionIndex;
      item.choiceIndex = i;

      if (choice.inGroup)
        item.classList.add("in-optgroup");

      if (choice.selected) {
        item.classList.add("selected");
        firstSelected = firstSelected || item;
      }
    }
    this._listbox.appendChild(fragment);
    this._container.hidden = false;
  
    BrowserUI.pushPopup(this, this._container);
    this._scrollElementIntoView(firstSelected);
    this._container.addEventListener("click", this, false);
    window.addEventListener("resize", this, true);
    this.sizeToContent();

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("SelectUI", true, false, window, true);
    window.dispatchEvent(evt);
  },

  reset: function selectHelperReset() {
    this._updateControl();
    while (this._listbox.hasChildNodes())
      this._listbox.removeChild(this._listbox.lastChild);
    this._list = null;
    this._title.value = "";
    this._selectedIndexes = null;
    BrowserUI.popPopup(this);
  },

  sizeToContent: function selectHelperSizeToContent() {
    this._container.firstChild.maxHeight = window.innerHeight * 0.75;
  },

  hide: function selectHelperHide() {
    if (!this._list)
      return;

    window.removeEventListener("resize", this, true);
    this._container.removeEventListener("click", this, false);
    this._container.hidden = true;
    this.reset();

    let evt = document.createEvent("UIEvents");
    evt.initUIEvent("SelectUI", true, false, window, true);
    window.dispatchEvent(evt);
  },

  unselectAll: function selectHelperUnselectAll() {
    if (!this._list)
      return;

    let choices = this._list.choices;
    this._forEachOption(function(aItem, aIndex) {
      aItem.selected = false;
      choices[aIndex].selected = false;
    });
  },

  selectByIndex: function selectHelperSelectByIndex(aIndex) {
    if (!this._list)
      return;

    let choices = this._list.choices;
    let children = this._listbox.childNodes;
    for (let i = 0; i < children.length; i++) {
      let option = children[i];
      if (option.optionIndex == aIndex) {
        let choice = choices[i];
        if (this._list.multiple) {
          choice.selected = !choice.selected;
          option.setAttribute("selected", choice.selected);
        } else {
          option.setAttribute("selected", "true");
          choice.selected = true;
          this._scrollElementIntoView(option);
        }
        break;
      }
    }
  },

  _getSelectedIndexes: function _selectHelperGetSelectedIndexes() {
    let indexes = [];
    if (!this._list)
      return indexes;

    let choices = this._list.choices;
    let choiceLength = choices.length;
    for (let i = 0; i < choiceLength; i++) {
      let choice = choices[i];
      if (choice.selected)
        indexes.push(choice.optionIndex);
    }
    return indexes;
  },

  _scrollElementIntoView: function _selectHelperScrollElementIntoView(aElement) {
    if (!aElement)
      return;

    let index = -1;
    this._forEachOption(
      function(aItem, aIndex) {
        if (aItem.optionIndex == aElement.optionIndex)
          index = aIndex;
      }
    );

    if (index == -1)
      return;

    let scrollBoxObject = this._listbox.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    let itemHeight = aElement.getBoundingClientRect().height;
    let visibleItemsCount = this._listbox.boxObject.height / itemHeight;
    if ((index + 1) > visibleItemsCount) {
      let delta = Math.ceil(visibleItemsCount / 2);
      scrollBoxObject.scrollTo(0, ((index + 1) - delta) * itemHeight);
    } else {
      scrollBoxObject.scrollTo(0, 0);
    }
  },

  _forEachOption: function _selectHelperForEachOption(aCallback) {
    let children = this._listbox.childNodes;
    for (let i = 0; i < children.length; i++) {
      let item = children[i];
      if (!item.hasOwnProperty("optionIndex"))
        continue;
      aCallback(item, i);
    }
  },

  _updateControl: function _selectHelperUpdateControl() {
    let currentSelectedIndexes = this._getSelectedIndexes();

    let isIdentical = (this._selectedIndexes && this._selectedIndexes.length == currentSelectedIndexes.length);
    if (isIdentical) {
      for (let i = 0; i < currentSelectedIndexes.length; i++) {
        if (currentSelectedIndexes[i] != this._selectedIndexes[i]) {
          isIdentical = false;
          break;
        }
      }
    }

    if (isIdentical)
      return;

    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceChange", { });
  },

  handleEvent: function selectHelperHandleEvent(aEvent) {
    switch (aEvent.type) {
      case "click":
        let item = aEvent.target;
        if (item && item.hasOwnProperty("optionIndex")) {
          if (this._list.multiple) {
            item.classList.toggle("selected");
          } else {
            this.unselectAll();

            // Select the new one and update the control
            item.classList.add("selected");
          }
          this.onSelect(item.optionIndex, item.classList.contains("selected"), !this._list.multiple);
        } else if (item == this._container) {
          // The click is outside the listbox area, so we need to hide the list
          // This is used instead of the popup mechanism otherwise the click
          // will be dispatched while we want to inhibit it (I think)
          this.hide();
        }
        break;
      case "resize":
        this.sizeToContent();
        break;
    }
  },

  onSelect: function selectHelperOnSelect(aIndex, aSelected, aClearAll) {
    let json = {
      index: aIndex,
      selected: aSelected,
      clearAll: aClearAll
    };
    Browser.selectedBrowser.messageManager.sendAsyncMessage("FormAssist:ChoiceSelect", json);

    // http://www.whatwg.org/specs/web-apps/current-work/multipage/the-button-element.html#the-select-element
    // The list will be closed as soon as the user click if it is not multiple,
    // while list with multiple choices have a button to close it
    if (!this._list.multiple) {
      this._updateControl();
      this.hide();
    }
  }
};
