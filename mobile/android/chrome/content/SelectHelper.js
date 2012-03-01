/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var SelectHelper = {
  _uiBusy: false,

  handleClick: function(aTarget) {
    // if we're busy looking at a select we want to eat any clicks that
    // come to us, but not to process them
    if (this._uiBusy)
        return true;

    let target = aTarget;
    while (target) {
      if (this._isSelectElement(target) && !target.disabled) {
        this._uiBusy = true;
        target.focus();
        let list = this.getListForElement(target);
        this.show(list, target);
        target = null;
        this._uiBusy = false;
        return true;
      }
      if (target)
        target = target.parentNode;
    }
    return false;
  },

  show: function(aList, aElement) {
    let data = JSON.parse(sendMessageToJava({ gecko: aList }));
    let selected = data.button;
    if (selected == -1)
        return;

    if (!(selected instanceof Array)) {
      let temp = [];
      for (let i = 0; i < aList.listitems.length; i++) {
        temp[i] = (i == selected);
      }
      selected = temp;
    }
    this.forOptions(aElement, function(aNode, aIndex) {
      aNode.selected = selected[aIndex];
    });
    this.fireOnChange(aElement);
  },

  _isSelectElement: function(aElement) {
    return (aElement instanceof HTMLSelectElement);
  },

  getListForElement: function(aElement) {
    let result = {
      type: "Prompt:Show",
      multiple: aElement.multiple,
      selected: [],
      listitems: []
    };

    if (aElement.multiple) {
      result.buttons = [
        { label: Strings.browser.GetStringFromName("selectHelper.closeMultipleSelectDialog") },
      ];
    }

    this.forOptions(aElement, function(aNode, aIndex, aIsGroup, aInGroup) {
      let item = {
        label: aNode.text || aNode.label,
        isGroup: aIsGroup,
        inGroup: aInGroup,
        disabled: aNode.disabled,
        id: aIndex
      }
      if (aInGroup)
        item.disabled = item.disabled || aNode.parentNode.disabled;

      result.listitems[aIndex] = item;
      result.selected[aIndex] = aNode.selected;
    });
    return result;
  },

  forOptions: function(aElement, aFunction) {
    let optionIndex = 0;
    let children = aElement.children;
    let numChildren = children.length;
    // if there are no children in this select, we add a dummy row so that at least something appears
    if (numChildren == 0)
      aFunction.call(this, {label:""}, optionIndex);
    for (let i = 0; i < numChildren; i++) {
      let child = children[i];
      if (child instanceof HTMLOptionElement) {
        // This is a regular choice under no group.
        aFunction.call(this, child, optionIndex, false, false);
        optionIndex++;
      } else if (child instanceof HTMLOptGroupElement) {
        aFunction.call(this, child, optionIndex, true, false);
        optionIndex++;

        let subchildren = child.children;
        let numSubchildren = subchildren.length;
        for (let j = 0; j < numSubchildren; j++) {
          let subchild = subchildren[j];
          aFunction.call(this, subchild, optionIndex, false, true);
          optionIndex++;
        }
      }
    }
  },

  fireOnChange: function(aElement) {
    let evt = aElement.ownerDocument.createEvent("Events");
    evt.initEvent("change", true, true, aElement.defaultView, 0,
                  false, false,
                  false, false, null);
    setTimeout(function() {
      aElement.dispatchEvent(evt);
    }, 0);
  }
};
