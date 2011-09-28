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
let HTMLIFrameElement = Ci.nsIDOMHTMLIFrameElement;
let HTMLDocument = Ci.nsIDOMHTMLDocument;
let HTMLHtmlElement = Ci.nsIDOMHTMLHtmlElement;
let HTMLBodyElement = Ci.nsIDOMHTMLBodyElement;
let HTMLLabelElement = Ci.nsIDOMHTMLLabelElement;
let HTMLButtonElement = Ci.nsIDOMHTMLButtonElement;
let HTMLOptGroupElement = Ci.nsIDOMHTMLOptGroupElement;
let HTMLOptionElement = Ci.nsIDOMHTMLOptionElement;
let XULMenuListElement = Ci.nsIDOMXULMenuListElement;

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
  addMessageListener("Content:SetWindowSize", this);

  /* Listen text events in order to update the autocomplete suggestions as soon
   * a key is entered on device
   */
  addEventListener("text", this, false);

  addEventListener("keypress", this, true);
  addEventListener("keyup", this, false);
  addEventListener("focus", this, true);
  addEventListener("blur", this, true);
  addEventListener("pageshow", this, false);
  addEventListener("pagehide", this, false);
  addEventListener("submit", this, false);

  this._enabled = Services.prefs.prefHasUserValue("formhelper.enabled") ?
                    Services.prefs.getBoolPref("formhelper.enabled") : false;
};

FormAssistant.prototype = {
  _selectWrapper: null,
  _currentIndex: -1,
  _elements: [],

  invalidSubmit: false,

  get currentElement() {
    return this._elements[this._currentIndex];
  },

  get currentIndex() {
    return this._currentIndex;
  },

  set currentIndex(aIndex) {
    let element = this._elements[aIndex];
    if (!element)
      return -1;

    if (this._isVisibleElement(element)) {
      this._currentIndex = aIndex;
      gFocusManager.setFocus(element, Ci.nsIFocusManager.FLAG_NOSCROLL);

      // To ensure we get the current caret positionning of the focused
      // element we need to delayed a bit the event
      this._executeDelayed(function(self) {
        // Bug 640870
        // Sometimes the element inner frame get destroyed while the element
        // receive the focus because the display is turned to 'none' for
        // example, in this "fun" case just do nothing if the element is hidden
        if (self._isVisibleElement(gFocusManager.focusedElement))
          sendAsyncMessage("FormAssist:Show", self._getJSON());
      });
    } else {
      // Repopulate the list of elements in the page, some could have gone
      // because of AJAX changes for example
      this._elements = [];
      let currentIndex = this._getAllElements(gFocusManager.focusedElement)

      if (aIndex < this._currentIndex)
        this.currentIndex = currentIndex - 1;
      else if (aIndex > this._currentIndex)
        this.currentIndex = currentIndex + 1;
      else if (this._currentIndex != currentIndex)
        this.currentIndex = currentIndex;
    }
    return element;
  },

  _open: false,
  open: function formHelperOpen(aElement) {
    // if the click is on an option element we want to check if the parent is a valid target
    if (aElement instanceof HTMLOptionElement && aElement.parentNode instanceof HTMLSelectElement && !aElement.disabled) {
      aElement = aElement.parentNode;
    }

    // bug 526045 - the form assistant will close if a click happen:
    // * outside of the scope of the form helper
    // * hover a button of type=[image|submit]
    // * hover a disabled element
    if (!this._isValidElement(aElement)) {
      let passiveButtons = { button: true, checkbox: true, file: true, radio: true, reset: true };
      if ((aElement instanceof HTMLInputElement || aElement instanceof HTMLButtonElement) &&
          passiveButtons[aElement.type] && !aElement.disabled)
        return false;

      return this.close();
    }

    // Look for a top editable element
    if (this._isEditable(aElement))
      aElement = this._getTopLevelEditable(aElement);

    // Checking if the element is the current focused one while the form assistant is open
    // allow the user to reposition the caret into an input element
    if (this._open && aElement == this.currentElement) {
      //hack bug 604351
      // if the element is the same editable element and the VKB is closed, reopen it
      let utils = Util.getWindowUtils(content);
      if (utils.IMEStatus == utils.IME_STATUS_DISABLED && aElement instanceof HTMLInputElement && aElement.mozIsTextField(false)) {
        aElement.blur();
        aElement.focus();
      }

      // If the element is a <select/> element and the user has manually click
      // it we need to inform the UI of such a change to keep in sync with the
      // new selected options once the event is finished
      if (aElement instanceof HTMLSelectElement) {
        this._executeDelayed(function(self) {
          sendAsyncMessage("FormAssist:Show", self._getJSON());
        });
      }

      return false;
    }

    // There is some case where we still want some data to be send to the
    // parent process even if form assistant is disabled:
    //  - the element is a choice list
    //  - the element has autocomplete suggestions
    this._enabled = Services.prefs.getBoolPref("formhelper.enabled");
    if (!this._enabled && !this._isSelectElement(aElement) && !this._isAutocomplete(aElement))
      return this.close();

    if (this._enabled) {
      this._elements = [];
      this.currentIndex = this._getAllElements(aElement);
    }
    else {
      this._elements = [aElement];
      this.currentIndex = 0;
    }

    return this._open = true;
  },

  close: function close() {
    if (this._open) {
      this._currentIndex = -1;
      this._elements = [];
      sendAsyncMessage("FormAssist:Hide", { });
      this._open = false;
    }

    return this._open;
  },

  receiveMessage: function receiveMessage(aMessage) {
    let currentElement = this.currentElement;
    if ((!this._enabled && !this._isAutocomplete(currentElement) && !getWrapperForElement(currentElement)) || !currentElement)
      return;

    let json = aMessage.json;
    switch (aMessage.name) {
      case "FormAssist:Previous":
        this.currentIndex--;
        break;

      case "FormAssist:Next":
        this.currentIndex++;
        break;

      case "Content:SetWindowSize":
        // If the CSS viewport change just show the current element to the new
        // position
        sendAsyncMessage("FormAssist:Resize", this._getJSON());
        break;

      case "FormAssist:ChoiceSelect": {
        this._selectWrapper = getWrapperForElement(currentElement);
        this._selectWrapper.select(json.index, json.selected, json.clearAll);
        break;
      }

      case "FormAssist:ChoiceChange": {
        // ChoiceChange could happened once we have move to an other element or
        // to nothing, so we should keep the used wrapper in mind
        this._selectWrapper.fireOnChange();

        // New elements can be shown when a select is updated so we need to
        // reconstruct the inner elements array and to take care of possible
        // focus change, this is why we use "self.currentElement" instead of 
        // using directly "currentElement".
        this._executeDelayed(function(self) {
          let currentElement = self.currentElement;
          if (!currentElement)
            return;

          self._elements = [];
          self._currentIndex = self._getAllElements(currentElement);
        });
        break;
      }

      case "FormAssist:AutoComplete": {
        try {
          currentElement = currentElement.QueryInterface(Ci.nsIDOMNSEditableElement);
          let imeEditor = currentElement.editor.QueryInterface(Ci.nsIEditorIMESupport);
          if (imeEditor.composing)
            imeEditor.forceCompositionEnd();
        }
        catch(e) {}

        currentElement.value = json.value;

        let event = currentElement.ownerDocument.createEvent("Events");
        event.initEvent("DOMAutoComplete", true, true);
        currentElement.dispatchEvent(event);
        break;
      }

      case "FormAssist:Closed":
        currentElement.blur();
        this._currentIndex = null;
        this._open = false;
        break;
    }
  },

  _els: Cc["@mozilla.org/eventlistenerservice;1"].getService(Ci.nsIEventListenerService),
  _hasKeyListener: function _hasKeyListener(aElement) {
    let els = this._els;
    let listeners = els.getListenerInfoFor(aElement, {});
    for (let i = 0; i < listeners.length; i++) {
      let listener = listeners[i];
      if (["keyup", "keydown", "keypress"].indexOf(listener.type) != -1
          && !listener.inSystemEventGroup) {
        return true;
      }
    }
    return false;
  },

  focusSync: false,
  handleEvent: function formHelperHandleEvent(aEvent) {
    // focus changes should be taken into account only if the user has done a
    // manual operation like manually clicking
    let shouldIgnoreFocus = (aEvent.type == "focus" && !this._open && !this.focusSync);
    if ((!this._open && aEvent.type != "focus") || shouldIgnoreFocus)
      return;

    let currentElement = this.currentElement;
    switch (aEvent.type) {
      case "submit":
        // submit is a final action and the form assistant should be closed
        this.close();
        break;

      case "pagehide":
      case "pageshow":
        // When reacting to a page show/hide, if the focus is different this
        // could mean the web page has dramatically changed because of
        // an Ajax change based on fragment identifier
        if (gFocusManager.focusedElement != currentElement)
          this.close();
        break;

      case "focus":
        let focusedElement = gFocusManager.getFocusedElementForWindow(content, true, {}) || aEvent.target;

        // If a body element is editable and the body is the child of an
        // iframe we can assume this is an advanced HTML editor, so let's
        // redirect the form helper selection to the iframe element
        if (focusedElement && this._isEditable(focusedElement)) {
          let editableElement = this._getTopLevelEditable(focusedElement);
          if (this._isValidElement(editableElement)) {
            this._executeDelayed(function(self) {
              self.open(editableElement);
            });
          }
          return;
        }

        // if an element is focused while we're closed but the element can be handle
        // by the assistant, try to activate it (only during mouseup)
        if (!currentElement) {
          if (focusedElement && this._isValidElement(focusedElement)) {
            this._executeDelayed(function(self) {
              self.open(focusedElement);
            });
          }
          return;
        }

        let focusedIndex = this._getIndexForElement(focusedElement);
        if (focusedIndex != -1 && this.currentIndex != focusedIndex)
          this.currentIndex = focusedIndex;
        break;

      case "blur":
        content.setTimeout(function(self) {
          if (!self._open)
            return;

          // If the blurring causes focus be in no other element,
          // we should close the form assistant.
          let focusedElement = gFocusManager.getFocusedElementForWindow(content, true, {});
          if (!focusedElement)
            self.close();
        }, 0, this);
        break;

      case "text":
        if (this._isValidatable(aEvent.target))
          sendAsyncMessage("FormAssist:ValidationMessage", this._getJSON());

        if (this._isAutocomplete(aEvent.target))
          sendAsyncMessage("FormAssist:AutoComplete", this._getJSON());
        break;

      // key processing inside a select element are done during the keypress
      // handler, preventing this one to be fired cancel the selection change
      case "keypress":
        // There is no need to handle keys if there is not element currently
        // used by the form assistant
        if (!currentElement)
          return;

        let formExceptions = { button: true, checkbox: true, file: true, image: true, radio: true, reset: true, submit: true };
        if (this._isSelectElement(currentElement) || formExceptions[currentElement.type] ||
            currentElement instanceof HTMLButtonElement || (currentElement.getAttribute("role") == "button" && currentElement.hasAttribute("tabindex"))) {
          switch (aEvent.keyCode) {
            case aEvent.DOM_VK_RIGHT:
              this._executeDelayed(function(self) {
                self.currentIndex++;
              });
              aEvent.stopPropagation();
              aEvent.preventDefault();
              break;

            case aEvent.DOM_VK_LEFT:
              this._executeDelayed(function(self) {
                self.currentIndex--;
              });
              aEvent.stopPropagation();
              aEvent.preventDefault();
              break;
          }
        }
        break;

      case "keyup":
        // There is no need to handle keys if there is not element currently
        // used by the form assistant
        if (!currentElement)
          return;

        switch (aEvent.keyCode) {
          case aEvent.DOM_VK_DOWN:
            if (currentElement instanceof HTMLInputElement && !this._isAutocomplete(currentElement)) {
              if (this._hasKeyListener(currentElement))
                return;
            } else if (currentElement instanceof HTMLTextAreaElement) {
              let existSelection = currentElement.selectionEnd - currentElement.selectionStart;
              let isEnd = (currentElement.textLength == currentElement.selectionEnd);
              if (!isEnd || existSelection)
                return;
            } else if (getListForElement(currentElement)) {
              this.currentIndex = this.currentIndex;
              return;
            }

            this.currentIndex++;
            break;

          case aEvent.DOM_VK_UP:
            if (currentElement instanceof HTMLInputElement && !this._isAutocomplete(currentElement)) {
              if (this._hasKeyListener(currentElement))
                return;
            } else if (currentElement instanceof HTMLTextAreaElement) {
              let existSelection = currentElement.selectionEnd - currentElement.selectionStart;
              let isStart = (currentElement.selectionEnd == 0);
              if (!isStart || existSelection)
                return;
            } else if (this._isSelectElement(currentElement)) {
              this.currentIndex = this.currentIndex;
              return;
            }

            this.currentIndex--;
            break;

          case aEvent.DOM_VK_RETURN:
            if (!this._isVisibleElement(currentElement))
              this.close();
            break;

          case aEvent.DOM_VK_ESCAPE:
          case aEvent.DOM_VK_TAB:
            break;

          default:
            if (this._isValidatable(aEvent.target))
              sendAsyncMessage("FormAssist:ValidationMessage", this._getJSON());

            if (this._isAutocomplete(aEvent.target))
              sendAsyncMessage("FormAssist:AutoComplete", this._getJSON());
            else if (currentElement && this._isSelectElement(currentElement))
              this.currentIndex = this.currentIndex;
            break;
        }

        let caretRect = this._getCaretRect();
        if (!caretRect.isEmpty())
          sendAsyncMessage("FormAssist:Update", { caretRect: caretRect });
    }
  },

  _executeDelayed: function formHelperExecuteSoon(aCallback) {
    let self = this;
    let timer = new Util.Timeout(function() {
      aCallback(self);
    });
    timer.once(0);
  },

  _filterEditables: function formHelperFilterEditables(aNodes) {
    let result = [];
    for (let i = 0; i < aNodes.length; i++) {
      let node = aNodes[i];

      // Avoid checking the top level editable element of each node
      if (this._isEditable(node)) {
        let editableElement = this._getTopLevelEditable(node);
        if (result.indexOf(editableElement) == -1)
          result.push(editableElement);
      }
      else {
        result.push(node);
      }
    }
    return result;
  },

  _isEditable: function formHelperIsEditable(aElement) {
    let canEdit = false;

    if (aElement.isContentEditable || aElement.designMode == "on") {
      canEdit = true;
    } else if (aElement instanceof HTMLIFrameElement && (aElement.contentDocument.body.isContentEditable || aElement.contentDocument.designMode == "on")) {
      canEdit = true;
    } else {
      canEdit = aElement.ownerDocument && aElement.ownerDocument.designMode == "on";
    }

    return canEdit;
  },

  _getTopLevelEditable: function formHelperGetTopLevelEditable(aElement) {
    if (!(aElement instanceof HTMLIFrameElement)) {
      let element = aElement;

      // Retrieve the top element that is editable
      if (element instanceof HTMLHtmlElement)
        element = element.ownerDocument.body;
      else if (element instanceof HTMLDocument)
        element = element.body;
    
      while (element && !this._isEditable(element))
        element = element.parentNode;

      // Return the container frame if we are into a nested editable frame
      if (element && element instanceof HTMLBodyElement && element.ownerDocument.defaultView != content.document.defaultView)
        return element.ownerDocument.defaultView.frameElement;
    }

    return aElement;
  },

  _isValidatable: function(aElement) {
    return this.invalidSubmit &&
           (aElement instanceof HTMLInputElement ||
            aElement instanceof HTMLTextAreaElement ||
            aElement instanceof HTMLSelectElement ||
            aElement instanceof HTMLButtonElement);
  },

  _isAutocomplete: function formHelperIsAutocomplete(aElement) {
    if (aElement instanceof HTMLInputElement) {
      if (aElement.getAttribute("type") == "password")
        return false;

      let autocomplete = aElement.getAttribute("autocomplete");
      let allowedValues = ["off", "false", "disabled"];
      if (allowedValues.indexOf(autocomplete) == -1)
        return true;
    }

    return false;
  },

  /*
   * This function is similar to getListSuggestions from
   * components/satchel/src/nsInputListAutoComplete.js but sadly this one is
   * used by the autocomplete.xml binding which is not in used in fennec
   */
  _getListSuggestions: function formHelperGetListSuggestions(aElement) {
    if (!(aElement instanceof HTMLInputElement) || !aElement.list)
      return [];

    let suggestions = [];
    let filter = !aElement.hasAttribute("mozNoFilter");
    let lowerFieldValue = aElement.value.toLowerCase();

    let options = aElement.list.options;
    let length = options.length;
    for (let i = 0; i < length; i++) {
      let item = options.item(i);

      let label = item.value;
      if (item.label)
        label = item.label;
      else if (item.text)
        label = item.text;

      if (filter && label.toLowerCase().indexOf(lowerFieldValue) == -1)
        continue;
       suggestions.push({ label: label, value: item.value });
    }

    return suggestions;
  },

  _isValidElement: function formHelperIsValidElement(aElement) {
    if (!aElement.getAttribute)
      return false;

    let formExceptions = { button: true, checkbox: true, file: true, image: true, radio: true, reset: true, submit: true };
    if (aElement instanceof HTMLInputElement && formExceptions[aElement.type])
      return false;

    if (aElement instanceof HTMLButtonElement ||
        (aElement.getAttribute("role") == "button" && aElement.hasAttribute("tabindex")))
      return false;

    return this._isNavigableElement(aElement) && this._isVisibleElement(aElement);
  },

  _isNavigableElement: function formHelperIsNavigableElement(aElement) {
    if (aElement.disabled || aElement.getAttribute("tabindex") == "-1")
      return false;

    if (aElement.getAttribute("role") == "button" && aElement.hasAttribute("tabindex"))
      return true;

    if (this._isSelectElement(aElement) || aElement instanceof HTMLTextAreaElement)
      return true;

    if (aElement instanceof HTMLInputElement || aElement instanceof HTMLButtonElement)
      return !(aElement.type == "hidden");

    return this._isEditable(aElement);
  },

  _isVisibleElement: function formHelperIsVisibleElement(aElement) {
    let style = aElement ? aElement.ownerDocument.defaultView.getComputedStyle(aElement, null) : null;
    if (!style)
      return false;

    let isVisible = (style.getPropertyValue("visibility") != "hidden");
    let isOpaque = (style.getPropertyValue("opacity") != 0);

    let rect = aElement.getBoundingClientRect();

    // Since the only way to show a drop-down menu for a select when the form
    // assistant is enabled is to return true here, a select is allowed to have
    // an opacity to 0 in order to let web developpers add a custom design on
    // top of it. This is less important to use the form assistant for the
    // other types of fields because even if the form assistant won't fired,
    // the focus will be in and a VKB will popup if needed
    return isVisible && (isOpaque || this._isSelectElement(aElement)) && (rect.height != 0 || rect.width != 0);
  },

  _isSelectElement: function formHelperIsSelectElement(aElement) {
    return (aElement instanceof HTMLSelectElement || aElement instanceof XULMenuListElement);
  },

  /** Caret is used to input text for this element. */
  _getCaretRect: function _formHelperGetCaretRect() {
    let element = this.currentElement;
    let focusedElement = gFocusManager.getFocusedElementForWindow(content, true, {});
    if (element && (element.mozIsTextField && element.mozIsTextField(false) ||
        element instanceof HTMLTextAreaElement) && focusedElement == element && this._isVisibleElement(element)) {
      let utils = Util.getWindowUtils(element.ownerDocument.defaultView);
      let rect = utils.sendQueryContentEvent(utils.QUERY_CARET_RECT, element.selectionEnd, 0, 0, 0);
      if (rect) {
        let scroll = ContentScroll.getScrollOffset(element.ownerDocument.defaultView);
        return new Rect(scroll.x + rect.left, scroll.y + rect.top, rect.width, rect.height);
      }
    }

    return new Rect(0, 0, 0, 0);
  },

  /** Gets a rect bounding important parts of the element that must be seen when assisting. */
  _getRect: function _formHelperGetRect() {
    const kDistanceMax = 100;
    let element = this.currentElement;
    let elRect = getBoundingContentRect(element);

    let labels = this._getLabels();
    for (let i=0; i<labels.length; i++) {
      let labelRect = labels[i].rect;
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

  _getLabels: function formHelperGetLabels() {
    let associatedLabels = [];

    let element = this.currentElement;
    let labels = element.ownerDocument.getElementsByTagName("label");
    for (let i=0; i<labels.length; i++) {
      let label = labels[i];
      if ((label.control == element || label.getAttribute("for") == element.id) && this._isVisibleElement(label)) {
        associatedLabels.push({
          rect: getBoundingContentRect(label),
          title: label.textContent
        });
      }
    }

    return associatedLabels;
  },

  _getAllElements: function getAllElements(aElement) {
    // XXX good candidate for tracing if possible.
    // The tough ones are lenght and isVisibleElement.
    let document = aElement.ownerDocument;
    if (!document)
      return;

    let documents = Util.getAllDocuments(document);

    let elements = this._elements;
    for (let i = 0; i < documents.length; i++) {
      let selector = "input, button, select, textarea, [role=button], iframe, [contenteditable=true]";
      let nodes = documents[i].querySelectorAll(selector);
      nodes = this._filterRadioButtons(nodes);

      for (let j = 0; j < nodes.length; j++) {
        let node = nodes[j];
        if (!this._isNavigableElement(node) || !this._isVisibleElement(node))
          continue;

        elements.push(node);
      }
    }
    this._elements = this._filterEditables(elements);

    function orderByTabIndex(a, b) {
      // for an explanation on tabbing navigation see
      // http://www.w3.org/TR/html401/interact/forms.html#h-17.11.1
      // In resume tab index navigation order is 1, 2, 3, ..., 32767, 0
      if (a.tabIndex == 0 || b.tabIndex == 0)
        return b.tabIndex;

      return a.tabIndex > b.tabIndex;
    }
    this._elements = this._elements.sort(orderByTabIndex);

    // retrieve the correct index
    let currentIndex = this._getIndexForElement(aElement);
    return currentIndex;
  },

  _getIndexForElement: function(aElement) {
    let currentIndex = -1;
    let elements = this._elements;
    for (let i = 0; i < elements.length; i++) {
      if (elements[i] == aElement)
        return i;
    }
    return -1;
  },

  _getJSON: function() {
    let element = this.currentElement;
    let choices = getListForElement(element);
    let editable = (element instanceof HTMLInputElement && element.mozIsTextField(false)) || this._isEditable(element);

    let labels = this._getLabels();
    return {
      current: {
        id: element.id,
        name: element.name,
        title: labels.length ? labels[0].title : "",
        value: element.value,
        maxLength: element.maxLength,
        type: (element.getAttribute("type") || "").toLowerCase(),
        choices: choices,
        isAutocomplete: this._isAutocomplete(element),
        validationMessage: this.invalidSubmit ? element.validationMessage : null,
        list: this._getListSuggestions(element),
        rect: this._getRect(),
        caretRect: this._getCaretRect(),
        editable: editable
      },
      hasPrevious: !!this._elements[this._currentIndex - 1],
      hasNext: !!this._elements[this._currentIndex + 1]
    };
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
  }
};


/******************************************************************************
 * The next classes wraps some forms elements such as different type of list to
 * abstract the difference between html and xul element while manipulating them
 *  - SelectWrapper   : <html:select>
 *  - MenulistWrapper : <xul:menulist>
 *****************************************************************************/

function getWrapperForElement(aElement) {
  let wrapper = null;
  if (aElement instanceof HTMLSelectElement) {
    wrapper = new SelectWrapper(aElement);
  }
  else if (aElement instanceof XULMenuListElement) {
    wrapper = new MenulistWrapper(aElement);
  }

  return wrapper;
}

function getListForElement(aElement) {
  let wrapper = getWrapperForElement(aElement);
  if (!wrapper)
    return null;

  let optionIndex = 0;
  let result = {
    multiple: wrapper.getMultiple(),
    choices: []
  };

  // Build up a flat JSON array of the choices. In HTML, it's possible for select element choices
  // to be under a group header (but not recursively). We distinguish between headers and entries
  // using the boolean "list.group".
  // XXX If possible, this would be a great candidate for tracing.
  let children = wrapper.getChildren();
  for (let i = 0; i < children.length; i++) {
    let child = children[i];
    if (wrapper.isGroup(child)) {
      // This is the group element. Add an entry in the choices that says that the following
      // elements are a member of this group.
      result.choices.push({ group: true,
                            text: child.label || child.firstChild.data,
                            disabled: child.disabled
                          });
      let subchildren = child.children;
      for (let j = 0; j < subchildren.length; j++) {
        let subchild = subchildren[j];
        result.choices.push({
          group: false,
          inGroup: true,
          text: wrapper.getText(subchild),
          disabled: child.disabled || subchild.disabled,
          selected: subchild.selected,
          optionIndex: optionIndex++
        });
      }
    }
    else if (wrapper.isOption(child)) {
      // This is a regular choice under no group.
      result.choices.push({
        group: false,
        inGroup: false,
        text: wrapper.getText(child),
        disabled: child.disabled,
        selected: child.selected,
        optionIndex: optionIndex++
      });
    }
  }

  return result;
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
    let options = this._control.options;
    options[aIndex].selected = aSelected;

    if (aClearAll) {
      for (let i = 0; i < options.length; i++) {
        if (i != aIndex)
          options.item(i).selected = false;
      }
    }
  },

  fireOnChange: function() {
    let control = this._control;
    let evt = this._control.ownerDocument.createEvent("Events");
    evt.initEvent("change", true, true, this._control.ownerDocument.defaultView, 0,
                  false, false,
                  false, false, null);
    content.setTimeout(function() {
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
    let result = control.selectedIndex;
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
    return false;
  },

  select: function(aIndex, aSelected, aClearAll) {
    let control = this._control.wrappedJSObject || this._control;
    control.selectedIndex = aIndex;
  },

  fireOnChange: function() {
    let control = this._control;
    let evt = document.createEvent("XULCommandEvent");
    evt.initCommandEvent("command", true, true, window, 0,
                         false, false,
                         false, false, null);
    content.setTimeout(function() {
      control.dispatchEvent(evt);
    }, 0);
  }
};

