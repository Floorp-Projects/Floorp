// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/*
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *   Vivien Nicolas <vnicolas@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

let Ci = Components.interfaces;
let Cc = Components.classes;

dump("###################################### forms.js loaded\n");

let HTMLTextAreaElement = Ci.nsIDOMHTMLTextAreaElement;
let HTMLInputElement = Ci.nsIDOMHTMLInputElement;
let HTMLSelectElement = Ci.nsIDOMHTMLSelectElement;
let HTMLLabelElement = Ci.nsIDOMHTMLLabelElement;
let HTMLButtonElement = Ci.nsIDOMHTMLButtonElement;
let HTMLOptGroupElement = Ci.nsIDOMHTMLOptGroupElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;

/**
 * Responsible of navigation between forms fields and of the opening of the assistant
 */
function FormAssistant() {
  addMessageListener("FormAssist:Closed", this);
  addMessageListener("FormAssist:Previous", this);
  addMessageListener("FormAssist:Next", this);
  addMessageListener("FormAssist:ChoiceSelect", this);
  addMessageListener("FormAssist:ChoiceChange", this);
  addMessageListener("FormAssist:AutoComplete", this);
};

FormAssistant.prototype = {
  _enabled: false,

  open: function(aElement) {
    if (!aElement)
      return false;

    let wrapper = new BasicWrapper(aElement);
    if (!wrapper.canAssist())
      return false;

    this._enabled = gPrefService.getBoolPref("formhelper.enabled");
    if (!this._enabled && !wrapper.hasChoices()) {
      return false;
    }
    else if (this._enabled) {
      this._wrappers = [];
      this._currentIndex = -1;
      this._getAllWrappers(aElement);
    }
    else {
      this._wrappers = [aElement];
      this._currentIndex = 0;
    }

    if (!this.getCurrent())
      return false;

    addEventListener("keyup", this, false);
    sendAsyncMessage("FormAssist:Show", this.getJSON());

    return true;
  },

  close: function() {
    removeEventListener("keyup", this, false);
    sendAsyncMessage("FormAssist:Hide", { });
  },

  receiveMessage: function receiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "FormAssist:Close":
        this.close();
        break;

      case "FormAssist:Previous":
        this.goToPrevious();
        sendAsyncMessage("FormAssist:Show", this.getJSON());
        break;

      case "FormAssist:Next":
        this.goToNext();
        sendAsyncMessage("FormAssist:Show", this.getJSON());
        break;

      case "FormAssist:ChoiceSelect": {
        let current = this.getCurrent();
        if (!current)
          return;

        current.choiceSelect(json.index, json.selected, json.clearAll);
        break;
      }

      case "FormAssist:ChoiceChange": {
        let current = this.getCurrent();
        if (!current)
          return;

        current.choiceChange();
        break;
      }

      case "FormAssist:AutoComplete":
        let current = this.getCurrent();
        if (!current)
          return;

        current.autocomplete(json.value);
        break;
    }
  },

  handleEvent: function formHelperHandleEvent(aEvent) {
    let currentWrapper = this.getCurrent();
    let currentElement = currentWrapper.element;

    switch (aEvent.keyCode) {
      case aEvent.DOM_VK_DOWN:
        if (currentElement instanceof HTMLTextAreaElement) {
          let existSelection = currentElement.selectionEnd - currentElement.selectionStart;
          let isEnd = (currentElement.textLength == currentElement.selectionEnd);
          if (!isEnd || existSelection)
            return;
        }

        this.goToNext();
        sendAsyncMessage("FormAssist:Show", this.getJSON());
        break;

      case aEvent.DOM_VK_UP:
        if (currentElement instanceof HTMLTextAreaElement) {
          let existSelection = currentElement.selectionEnd - currentElement.selectionStart;
          let isStart = (currentElement.selectionEnd == 0);
          if (!isStart || existSelection)
            return;
        }

        this.goToPrevious();
        sendAsyncMessage("FormAssist:Show", this.getJSON());
        break;

      case aEvent.DOM_VK_RETURN:
        break;

      default:
        let target = aEvent.target;
        if (currentWrapper.canAutocomplete())
          sendAsyncMessage("FormAssist:AutoComplete", this.getJSON());
        break;
    }

    let caretRect = currentWrapper.getCaretRect();
    if (!caretRect.isEmpty()) {
      sendAsyncMessage("FormAssist:Update", { caretRect: caretRect });
    }
  },

  _getRectForCaret: function _getRectForCaret() {
    let currentElement = this.getCurrent();
    let rect = currentElement.getCaretRect();
    return null;
  },

  _getAllWrappers: function getAllWrappers(aElement) {
    // XXX good candidate for tracing if possible.
    // The tough ones are lenght and canNavigateTo / isVisible.
    let document = aElement.ownerDocument;
    if (!document)
      return;

    let elements = this._wrappers;

    // get all the documents
    let documents = Util.getAllDocuments(document);

    for (let i = 0; i < documents.length; i++) {
      let nodes = documents[i].querySelectorAll("input, button, select, textarea, [role=button]");
      nodes = this._filterRadioButtons(nodes);

      for (let j = 0; j < nodes.length; j++) {
        let node = nodes[j];
        let wrapper = new BasicWrapper(node);
        if (wrapper.canNavigateTo() && wrapper.isVisible()) {
          elements.push(wrapper);
          if (node == aElement)
            this._setIndex(elements.length - 1);
        }
      }
    }

    function orderByTabIndex(a, b) {
      // for an explanation on tabbing navigation see
      // http://www.w3.org/TR/html401/interact/forms.html#h-17.11.1
      // In resume tab index navigation order is 1, 2, 3, ..., 32767, 0
      if (a.tabIndex == 0 || b.tabIndex == 0)
        return b.tabIndex;

      return a.tabIndex > b.tabIndex;
    }
    elements = elements.sort(orderByTabIndex);
  },

  getCurrent: function() {
    return this._wrappers[this._currentIndex];
  },

  getJSON: function() {
    return {
      hasNext: !!this.getNext(),
      hasPrevious: !!this.getPrevious(),
      current: this.getCurrent().getJSON(),
      showNavigation: this._enabled
    };
  },

  getPrevious: function getPrevious() {
    return this._wrappers[this._currentIndex - 1];
  },

  getNext: function getNext() {
    return this._wrappers[this._currentIndex + 1];
  },

  goToPrevious: function goToPrevious() {
    return this._setIndex(this._currentIndex - 1);
  },

  goToNext: function goToNext() {
    return this._setIndex(this._currentIndex + 1);
  },

  /**
   * For each radio button group, remove all but the checked button
   * if there is one, or the first button otherwise.
   */
  _filterRadioButtons: function(aNodes) {
    // First pass: Find the checked or first element in each group.
    let chosenRadios = {};
    for (let i=0; i < aNodes.length; i++) {
      let node = aNodes[i];
      if (node.type == "radio" && (!chosenRadios.hasOwnProperty(node.name) || node.checked))
        chosenRadios[node.name] = node;
    }

    // Second pass: Exclude all other radio buttons from the list.
    let result = [];
    for (let i=0; i < aNodes.length; i++) {
      let node = aNodes[i];
      if (node.type == "radio" && chosenRadios[node.name] != node)
        continue;
      result.push(node);
    }
    return result;
  },

  _setIndex: function(aIndex) {
    let element = this._wrappers[aIndex];
    if (element) {
      gFocusManager.setFocus(element.element, Ci.nsIFocusManager.FLAG_NOSCROLL);
      this._currentIndex = aIndex;
    }
    return element;
  }
};


/******************************************************************************
 * The next classes wraps some specific forms elements to add helpers while
 * manipulating them.
 *  - BasicWrapper:     All forms elements except <html:select>, <xul:menulist>
 *  - SelectWrapper:    <html:select> elements
 *  - MenulistWrapper:  <xul:menulist> elements
 *****************************************************************************/
function BasicWrapper(aElement) {
  if (!aElement)
    throw "Instantiating BasicWrapper with null element";

  this.element = aElement;
}

BasicWrapper.prototype = {
  isVisible: function isVisible() {
    return this._isElementVisible(this.element);
  },

  canNavigateTo: function canNavigateTo() {
    let element = this.element;
    if (element.disabled)
      return false;

    if (element.getAttribute("role") == "button" && element.hasAttribute("tabindex"))
      return true;

    if (this.hasChoices() || element instanceof HTMLTextAreaElement)
      return true;

    if (element instanceof HTMLInputElement || element instanceof HTMLButtonElement) {
      return !(element.type == "hidden")
    }

    return false;
  },

  /** Should assistant act when user taps on element? */
  canAssist: function canAssist() {
    let element = this.element;

    let formExceptions = {button: true, checkbox: true, file: true, image: true, radio: true, reset: true, submit: true};
    if (element instanceof HTMLInputElement && formExceptions[element.type])
      return false;

    if (element instanceof HTMLButtonElement ||
        (element.getAttribute("role") == "button" && element.hasAttribute("tabindex")))
      return false;

    return this.canNavigateTo();
  },

  /** Gets a rect bounding important parts of the element that must be seen when assisting. */
  getRect: function getRect() {
    const kDistanceMax = 100;
    let element = this.element;
    let elRect = getBoundingContentRect(element);

    let labels = this._getLabelsFor(element);
    for (let i=0; i<labels.length; i++) {
      let labelRect = getBoundingContentRect(labels[i]);
      if (labelRect.left < elRect.left) {
        let isClose = Math.abs(labelRect.left - elRect.left) - labelRect.width < kDistanceMax &&
                      Math.abs(labelRect.top - elRect.top) - labelRect.height < kDistanceMax;
        if (isClose) {
          let width = labelRect.width + elRect.width + (elRect.left - labelRect.left - labelRect.width);
          return new Rect(labelRect.left, labelRect.top, width, elRect.height).expandToIntegers();
        }
      }
    }
    return elRect;
  },

  /** Element is capable of having autocomplete suggestions. */
  canAutocomplete: function() {
    return this.element instanceof HTMLInputElement;
  },

  autocomplete: function(aValue) {
    this.element.value = aValue;
  },

  /** Caret is used to input text for this element. */
  getCaretRect: function() {
    let element = this.element;
    if ((element instanceof HTMLTextAreaElement ||
        (element instanceof HTMLInputElement && element.type == "text")) &&
        gFocusManager.focusedElement == element) {
      let utils = Util.getWindowUtils(element.ownerDocument.defaultView);
      let rect = utils.sendQueryContentEvent(utils.QUERY_CARET_RECT, element.selectionEnd, 0, 0, 0);
      if (!rect)
        return new Rect(0, 0, 0, 0);

      let scroll = Util.getScrollOffset(element.ownerDocument.defaultView);
      let caret = new Rect(scroll.x + rect.left, scroll.y + rect.top, rect.width, rect.height);
      return caret;
    }

    return new Rect(0, 0, 0, 0);
  },

  /** Returns true if the choices interface needs to shown. */
  hasChoices: function hasChoices() {
    let element = this.element;
    return (element instanceof HTMLSelectElement) || (element instanceof Ci.nsIDOMXULMenuListElement);
  },

  choiceSelect: function(aIndex, aSelected, aClearAll) {
    let wrapper = this._getChoiceWrapper(this._currentIndex);
    if (wrapper)
      wrapper.select(aIndex, aSelected, aClearAll);
  },

  choiceChange: function() {
    let wrapper = this._getChoiceWrapper(this._currentIndex);
    if (wrapper)
      wrapper.fireOnChange();
  },

  getChoiceData: function() {
    let wrapper = this._getChoiceWrapper();
    if (!wrapper)
      return null;

    let optionIndex = 0;
    let result = {
      multiple: wrapper.getMultiple(),
      choices: []
    };

    // Build up a flat JSON array of the choices. In HTML, it's possible for select element choices
    // to be under a group header (but not recursively). We distinguish between headers and entries
    // using the boolean "choiceData.group".
    // XXX If possible, this would be a great candidate for tracing.
    let children = wrapper.getChildren();
    for (let i = 0; i < children.length; i++) {
      let child = children[i];
      if (wrapper.isGroup(child)) {
        // This is the group element. Add an entry in the choices that says that the following
        // elements are a member of this group.
        result.choices.push({ group: true, groupName: child.label });
        let subchildren = child.children;
        for (let ii = 0; i < subchildren.length; ii++) {
          let subchild = subchildren[ii];
          result.choices.push({
            group: false,
            inGroup: true,
            text: wrapper.getText(subchild),
            selected: subchild.selected,
            optionIndex: optionIndex++
          });
        }
      } else if (wrapper.isOption(child)) {
        // This is a regular choice under no group.
        result.choices.push({
          group: false,
          inGroup: false,
          text: wrapper.getText(child),
          selected: child.selected,
          optionIndex: optionIndex++
        });
      }
    }

    return result;
  },

  getJSON: function() {
    return {
      id: this.element.id,
      name: this.element.name,
      value: this.element.value,
      maxLength: this.element.maxLength,
      canAutocomplete: this.canAutocomplete(),
      choiceData: this.getChoiceData(),
      navigable: this.canNavigateTo(),
      assistable: this.canAssist(),
      rect: this.getRect(),
      caretRect: this.getCaretRect()
    };
  },

  _getChoiceWrapper: function() {
    let choiceWrapper = null;
    let element = this.element;
    if (element instanceof HTMLSelectElement)
      choiceWrapper = new SelectWrapper(element);
    else if (element instanceof Ci.nsIDOMXULMenuListElement)
      choiceWrapper = new MenulistWrapper(element);
    return choiceWrapper;
  },

  _getLabelsFor: function(aElement) {
    let associatedLabels = [];

    let labels = aElement.ownerDocument.getElementsByTagName("label");
    for (let i=0; i<labels.length; i++) {
      if (labels[i].getAttribute("for") == aElement.id)
        associatedLabels.push(labels[i]);
    }

    if (aElement.parentNode instanceof HTMLLabelElement)
      associatedLabels.push(aElement.parentNode);

    return associatedLabels.filter(this._isElementVisible);
  },

  _isElementVisible: function(aElement) {
    let style = aElement.ownerDocument.defaultView.getComputedStyle(aElement, null);
    if (!style)
      return false;

    let isVisible = (style.getPropertyValue("visibility") != "hidden");
    let isOpaque = (style.getPropertyValue("opacity") != 0);

    let rect = aElement.getBoundingClientRect();
    return isVisible && isOpaque && (rect.height != 0 || rect.width != 0);
  }
};


function SelectWrapper(aControl) {
  this._control = aControl;
}

SelectWrapper.prototype = {
  getSelectedIndex: function() {
    return this._control.selectedIndex;
  },

  getMultiple: function() {
    return this._control.multiple;
  },

  getOptions: function() {
    return this._control.options;
  },

  getChildren: function() {
    return this._control.children;
  },

  getText: function(aChild) {
    return aChild.text;
  },

  isOption: function(aChild) {
    return aChild instanceof HTMLOptionElement;
  },

  isGroup: function(aChild) {
    return aChild instanceof HTMLOptGroupElement;
  },

  select: function(aIndex, aSelected, aClearAll) {
    let selectElement = this._control.QueryInterface(Ci.nsISelectElement);
    selectElement.setOptionsSelectedByIndex(aIndex, aIndex, aSelected, aClearAll, false, true);
  },

  focus: function() {
    this._control.focus();
  },

  fireOnChange: function() {
    let control = this._control;
    let evt = this._control.ownerDocument.createEvent("Events");
    evt.initEvent("change", true, true, this._control.ownerDocument.defaultView, 0,
                  false, false,
                  false, false, null);
    content.document.defaultView.setTimeout(function() {
      control.dispatchEvent(evt);
    }, 0);
  }
};


// bug 559792
// Use wrappedJSObject when control is in content for extra protection
function MenulistWrapper(aControl) {
  this._control = aControl;
}

MenulistWrapper.prototype = {
  getSelectedIndex: function() {
    let control = this._control.wrappedJSObject || this._control;
    let result = control.selectedIndex
    return ((typeof result == "number" && !isNaN(result)) ? result : -1);
  },

  getMultiple: function() {
    return false;
  },

  getOptions: function() {
    let control = this._control.wrappedJSObject || this._control;
    return control.menupopup.children;
  },

  getChildren: function() {
    let control = this._control.wrappedJSObject || this._control;
    return control.menupopup.children;
  },

  getText: function(aChild) {
    return aChild.label;
  },

  isOption: function(aChild) {
    return aChild instanceof Ci.nsIDOMXULSelectControlItemElement;
  },

  isGroup: function(aChild) {
    return false
  },

  select: function(aIndex, aSelected, aClearAll) {
    let control = this._control.wrappedJSObject || this._control;
    control.selectedIndex = aIndex;
  },

  focus: function() {
    this._control.focus();
  },

  fireOnChange: function() {
    let control = this._control;
    let evt = document.createEvent("XULCommandEvent");
    evt.initCommandEvent("command", true, true, window, 0,
                         false, false,
                         false, false, null);
    content.document.defaultView.setTimeout(function() {
      control.dispatchEvent(evt);
    }, 0);
  }
};

