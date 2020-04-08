/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This is loaded into all XUL windows. Wrap in a block to prevent
// leaking to window scope.
{
  class MozSearchTextbox extends MozXULElement {
    constructor() {
      super();

      this.inputField = document.createElement("input");

      const METHODS = [
        "focus",
        "blur",
        "select",
        "setUserInput",
        "setSelectionRange",
      ];
      for (const method of METHODS) {
        this[method] = (...args) => this.inputField[method](...args);
      }

      const READ_WRITE_PROPERTIES = [
        "defaultValue",
        "placeholder",
        "readOnly",
        "size",
        "selectionStart",
        "selectionEnd",
      ];
      for (const property of READ_WRITE_PROPERTIES) {
        Object.defineProperty(this, property, {
          enumerable: true,
          get() {
            return this.inputField[property];
          },
          set(val) {
            return (this.inputField[property] = val);
          },
        });
      }

      this.attachShadow({ mode: "open" });
      this.addEventListener("input", this);
      this.addEventListener("keypress", this);
      this.addEventListener("mousedown", this);
    }

    static get inheritedAttributes() {
      return {
        input:
          "value,maxlength,disabled,size,readonly,placeholder,tabindex,accesskey,mozactionhint,spellcheck",
        ".textbox-search-icon": "label=searchbuttonlabel,disabled",
        ".textbox-search-clear": "disabled",
      };
    }

    static get markup() {
      // TODO: Bug 1534799 - Convert string to Fluent and use manual DOM construction
      return `
      <image class="textbox-search-clear" label="&searchTextBox.clear.label;"/>
      `;
    }

    static get entities() {
      return ["chrome://global/locale/textcontext.dtd"];
    }

    connectedCallback() {
      if (this.delayConnectedCallback()) {
        return;
      }
      this.textContent = "";

      const stylesheet = document.createElement("link");
      stylesheet.rel = "stylesheet";
      stylesheet.href = "chrome://global/skin/search-textbox.css";

      const textboxSign = document.createXULElement("image");
      textboxSign.className = "textbox-search-sign";
      textboxSign.part = "search-sign";

      const input = this.inputField;
      input.setAttribute("mozactionhint", "search");
      input.addEventListener("focus", this);
      input.addEventListener("blur", this);

      const searchBtn = (this._searchButtonIcon = document.createXULElement(
        "image"
      ));
      searchBtn.className = "textbox-search-icon";
      searchBtn.addEventListener("click", e => this._iconClick(e));

      let clearBtn = this.constructor.fragment;
      clearBtn = this._searchClearIcon = clearBtn.querySelector(
        ".textbox-search-clear"
      );
      clearBtn.addEventListener("click", () => this._clearSearch());

      const deck = (this._searchIcons = document.createXULElement("deck"));
      deck.className = "textbox-search-icons";
      deck.append(searchBtn, clearBtn);
      this.shadowRoot.append(stylesheet, textboxSign, input, deck);

      this._timer = null;

      // Ensure the button state is up to date:
      // eslint-disable-next-line no-self-assign
      this.searchButton = this.searchButton;

      this.initializeAttributeInheritance();
    }

    set timeout(val) {
      this.setAttribute("timeout", val);
      return val;
    }

    get timeout() {
      return parseInt(this.getAttribute("timeout")) || 500;
    }

    set searchButton(val) {
      if (val) {
        this.setAttribute("searchbutton", "true");
        this.removeAttribute("aria-autocomplete");
        this._searchButtonIcon.setAttribute("role", "button");
      } else {
        this.removeAttribute("searchbutton");
        this.setAttribute("aria-autocomplete", "list");
        this._searchButtonIcon.setAttribute("role", "none");
      }
      return val;
    }

    get searchButton() {
      return this.getAttribute("searchbutton") == "true";
    }

    set value(val) {
      this.inputField.value = val;

      if (val) {
        this._searchIcons.selectedIndex = this.searchButton ? 0 : 1;
      } else {
        this._searchIcons.selectedIndex = 0;
      }

      if (this._timer) {
        clearTimeout(this._timer);
      }
      return val;
    }

    get value() {
      return this.inputField.value;
    }

    get editor() {
      return this.inputField.editor;
    }

    set disabled(val) {
      this.inputField.disabled = val;
      if (val) {
        this.setAttribute("disabled", "true");
      } else {
        this.removeAttribute("disabled");
      }
      return val;
    }

    get disabled() {
      return this.inputField.disabled;
    }

    on_blur() {
      this.removeAttribute("focused");
    }

    on_focus() {
      this.setAttribute("focused", "true");
    }

    on_input() {
      if (this.searchButton) {
        this._searchIcons.selectedIndex = 0;
        return;
      }
      if (this._timer) {
        clearTimeout(this._timer);
      }
      this._timer =
        this.timeout && setTimeout(this._fireCommand, this.timeout, this);
      this._searchIcons.selectedIndex = this.value ? 1 : 0;
    }

    on_keypress(event) {
      switch (event.keyCode) {
        case KeyEvent.DOM_VK_ESCAPE:
          if (this._clearSearch()) {
            event.preventDefault();
            event.stopPropagation();
          }
          break;
        case KeyEvent.DOM_VK_RETURN:
          this._enterSearch();
          event.preventDefault();
          event.stopPropagation();
          break;
      }
    }

    on_mousedown(event) {
      if (!this.hasAttribute("focused")) {
        this.setSelectionRange(0, 0);
        this.focus();
      }
    }

    _fireCommand(me) {
      if (me._timer) {
        clearTimeout(me._timer);
      }
      me._timer = null;
      me.doCommand();
    }

    _iconClick() {
      if (this.searchButton) {
        this._enterSearch();
      } else {
        this.focus();
      }
    }

    _enterSearch() {
      if (this.disabled) {
        return;
      }
      if (this.searchButton && this.value && !this.readOnly) {
        this._searchIcons.selectedIndex = 1;
      }
      this._fireCommand(this);
    }

    _clearSearch() {
      if (!this.disabled && !this.readOnly && this.value) {
        this.value = "";
        this._fireCommand(this);
        this._searchIcons.selectedIndex = 0;
        return true;
      }
      return false;
    }
  }

  customElements.define("search-textbox", MozSearchTextbox);
}
