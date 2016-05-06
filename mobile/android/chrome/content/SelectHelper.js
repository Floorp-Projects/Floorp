/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var SelectHelper = {
  _uiBusy: false,

  handleEvent: function(event) {
    this.handleClick(event.target);
  },

  handleClick: function(target) {
    // if we're busy looking at a select we want to eat any clicks that
    // come to us, but not to process them
    if (this._uiBusy || !this._isMenu(target) || this._isDisabledElement(target)) {
      return;
    }

    this._uiBusy = true;
    this.show(target);
    this._uiBusy = false;
  },

  // This is a callback function to be provided to prompt.show(callBack).
  // It will update which Option elements in a Select have been selected
  // or unselected and fire the onChange event.
  _promptCallBack: function(data, element) {
    let selected = data.list;

    if (element instanceof Ci.nsIDOMXULMenuListElement) {
      if (element.selectedIndex != selected[0]) {
        element.selectedIndex = selected[0];
        this.fireOnCommand(element);
      }
    } else if (element instanceof HTMLSelectElement) {
      let changed = false;
      let i = 0; // The index for the element from `data.list` that we are currently examining.
      this.forVisibleOptions(element, function(node) {
        if (node.selected && selected.indexOf(i) == -1) {
          changed = true;
          node.selected = false;
        } else if (!node.selected && selected.indexOf(i) != -1) {
          changed = true;
          node.selected = true;
        }
        i++;
      });

      if (changed) {
        this.fireOnChange(element);
      }
    }
  },

  show: function(element) {
    let list = this.getListForElement(element);
    let p = new Prompt({
      window: element.contentDocument
    });

    if (element.multiple) {
      p.addButton({
        label: Strings.browser.GetStringFromName("selectHelper.closeMultipleSelectDialog")
      }).setMultiChoiceItems(list);
    } else {
      p.setSingleChoiceItems(list);
    }

    p.show((data) => {
      this._promptCallBack(data,element)
    });
  },

  _isMenu: function(element) {
    return (element instanceof HTMLSelectElement || element instanceof Ci.nsIDOMXULMenuListElement);
  },

  // Return a list of Option elements within a Select excluding
  // any that were not visible.
  getListForElement: function(element) {
    let index = 0;
    let items = [];
    this.forVisibleOptions(element, function(node, options,parent) {
      let item = {
        label: node.text || node.label,
        header: options.isGroup,
        disabled: node.disabled,
        id: index,
        selected: node.selected,
      };

      if (parent) {
        item.child = true;
        item.disabled = item.disabled || parent.disabled;
      }
      items.push(item);
      index++;
    });
    return items;
  },

  // Apply a function to all visible Option elements in a Select
  forVisibleOptions: function(element, aFunction, parent = null) {
    if (element instanceof Ci.nsIDOMXULMenuListElement) {
      element = element.menupopup;
    }
    let children = element.children;
    let numChildren = children.length;


    // if there are no children in this select, we add a dummy row so that at least something appears
    if (numChildren == 0) {
      aFunction.call(this, {label: ""}, {isGroup: false}, parent);
    }

    for (let i = 0; i < numChildren; i++) {
      let child = children[i];
      let style = window.getComputedStyle(child, null);
      if (style.display !== "none") {
        if (child instanceof HTMLOptionElement ||
            child instanceof Ci.nsIDOMXULSelectControlItemElement) {
          aFunction.call(this, child, {isGroup: false}, parent);
        } else if (child instanceof HTMLOptGroupElement) {
          aFunction.call(this, child, {isGroup: true});
          this.forVisibleOptions(child, aFunction, child);
        }
      }
    }
  },

  fireOnChange: function(element) {
    let event = element.ownerDocument.createEvent("Events");
    event.initEvent("change", true, true, element.defaultView, 0,
        false, false, false, false, null);
    setTimeout(function() {
      element.dispatchEvent(event);
    }, 0);
  },

  fireOnCommand: function(element) {
    let event = element.ownerDocument.createEvent("XULCommandEvent");
    event.initCommandEvent("command", true, true, element.defaultView, 0,
        false, false, false, false, null);
    setTimeout(function() {
      element.dispatchEvent(event);
    }, 0);
  },

  _isDisabledElement : function(element) {
    let currentElement = element;
    while (currentElement) {
      // Must test with === in case a form has a field named "disabled". See bug 1263589.
      if (currentElement.disabled === true) {
        return true;
      }
      currentElement = currentElement.parentElement;
    }
    return false;
  }
};
