/**
 * SelectHelperUI: Provides an interface for making a choice in a list.
 *   Supports simultaneous selection of choices and group headers.
 */
var SelectHelperUI = {
  _list: null,
  _selectedIndexes: null,

  get _panel() {
    delete this._panel;
    return this._panel = document.getElementById("select-container");
  },

  show: function selectHelperShow(aList) {
    this._list = aList;

    this._container = document.getElementById("select-list");
    this._container.setAttribute("multiple", aList.multiple ? "true" : "false");

    this._selectedIndexes = this._getSelectedIndexes();
    let firstSelected = null;

    // Using a fragment prevent us to hang on huge list
    let fragment = document.createDocumentFragment();
    let choices = aList.choices;
    for (let i = 0; i < choices.length; i++) {
      let choice = choices[i];
      let item = document.createElement("option");
      item.className = "chrome-select-option";
      item.setAttribute("label", choice.text);
      choice.disabled ? item.setAttribute("disabled", choice.disabled)
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
        item.setAttribute("selected", "true");
        firstSelected = firstSelected || item;
      }
    }
    this._container.appendChild(fragment);

    this._panel.hidden = false;
    this._panel.height = this._panel.getBoundingClientRect().height;

    if (!this._docked)
      BrowserUI.pushPopup(this, this._panel);

    this._scrollElementIntoView(firstSelected);

    this._container.addEventListener("click", this, false);
    this._panel.addEventListener("overflow", this, true);
  },

  dock: function selectHelperDock(aContainer) {
    aContainer.insertBefore(this._panel, aContainer.lastChild);
    this.resize();
    this._docked = true;
  },

  undock: function selectHelperUndock() {
    let rootNode = Elements.stack;
    rootNode.insertBefore(this._panel, rootNode.lastChild);
    this._panel.style.maxHeight = "";
    this._docked = false;
  },

  reset: function selectHelperReset() {
    this._updateControl();
    let empty = this._container.cloneNode(false);
    this._container.parentNode.replaceChild(empty, this._container);
    this._container = empty;
    this._list = null;
    this._selectedIndexes = null;
    this._panel.height = "";
  },

  resize: function selectHelperResize() {
    this._panel.style.maxHeight = (window.innerHeight / 1.8) + "px";
  },

  hide: function selectHelperResize() {
    if (!this._list)
      return;

    this._container.removeEventListener("click", this, false);
    this._panel.removeEventListener("overflow", this, true);

    this._panel.hidden = true;

    if (this._docked)
      this.undock();
    else
      BrowserUI.popPopup(this);

    this.reset();
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
    for (let i = 0; i < this._container.childNodes.length; i++) {
      let option = this._container.childNodes[i];
      if (option.optionIndex == aIndex) {
        option.selected = true;
        this._choices[i].selected = true;
        this._scrollElementIntoView(option);
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
        if (aElement.optionIndex == aItem.optionIndex)
          index = aIndex;
      }
    );

    if (index == -1)
      return;

    let scrollBoxObject = this._container.boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    let itemHeight = aElement.getBoundingClientRect().height;
    let visibleItemsCount = this._container.boxObject.height / itemHeight;
    if ((index + 1) > visibleItemsCount) {
      let delta = Math.ceil(visibleItemsCount / 2);
      scrollBoxObject.scrollTo(0, ((index + 1) - delta) * itemHeight);
    }
    else {
      scrollBoxObject.scrollTo(0, 0);
    }
  },

  _forEachOption: function _selectHelperForEachOption(aCallback) {
    let children = this._container.children;
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
            // Toggle the item state
            item.selected = !item.selected;
          }
          else {
            this.unselectAll();

            // Select the new one and update the control
            item.selected = true;
          }
          this.onSelect(item.optionIndex, item.selected, !this._list.multiple);
        }
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
    if (!this._list.multiple) {
      this._updateControl();
      // Update the selectedIndex so the field will fire a new change event if
      // needed
      this._selectedIndexes = [aIndex];
    }

  }
};
