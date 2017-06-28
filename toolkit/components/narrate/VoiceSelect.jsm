/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["VoiceSelect"];

function VoiceSelect(win, label) {
  this._winRef = Cu.getWeakReference(win);

  let element = win.document.createElement("div");
  element.classList.add("voiceselect");
  element.innerHTML =
   `<button class="select-toggle" aria-controls="voice-options">
      <span class="label">${label}</span> <span class="current-voice"></span>
    </button>
    <div class="options" id="voice-options" role="listbox"></div>`;

  this._elementRef = Cu.getWeakReference(element);

  let button = this.selectToggle;
  button.addEventListener("click", this);
  button.addEventListener("keypress", this);

  let listbox = this.listbox;
  listbox.addEventListener("click", this);
  listbox.addEventListener("mousemove", this);
  listbox.addEventListener("keypress", this);
  listbox.addEventListener("wheel", this, true);

  win.addEventListener("resize", () => {
    this._updateDropdownHeight();
  });
}

VoiceSelect.prototype = {
  add(label, value) {
    let option = this._doc.createElement("button");
    option.dataset.value = value;
    option.classList.add("option");
    option.tabIndex = "-1";
    option.setAttribute("role", "option");
    option.textContent = label;
    this.listbox.appendChild(option);
    return option;
  },

  addOptions(options) {
    let selected = null;
    for (let option of options) {
      if (option.selected) {
        selected = this.add(option.label, option.value);
      } else {
        this.add(option.label, option.value);
      }
    }

    this._select(selected || this.options[0], true);
  },

  clear() {
    this.listbox.innerHTML = "";
  },

  toggleList(force, focus = true) {
    if (this.element.classList.toggle("open", force)) {
      if (focus) {
        (this.selected || this.options[0]).focus();
      }

      this._updateDropdownHeight(true);
      this.listbox.setAttribute("aria-expanded", true);
      this._win.addEventListener("focus", this, true);
    } else {
      if (focus) {
        this.element.querySelector(".select-toggle").focus();
      }

      this.listbox.setAttribute("aria-expanded", false);
      this._win.removeEventListener("focus", this, true);
    }
  },

  handleEvent(evt) {
    let target = evt.target;

    switch (evt.type) {
      case "click":
        if (target.classList.contains("option")) {
          if (!target.classList.contains("selected")) {
            this.selected = target;
          }

          this.toggleList(false);
        } else if (target.classList.contains("select-toggle")) {
          this.toggleList();
        }
        break;

      case "mousemove":
        this.listbox.classList.add("hovering");
        break;

      case "keypress":
        if (target.classList.contains("select-toggle")) {
          if (evt.altKey) {
            this.toggleList(true);
          } else {
            this._keyPressedButton(evt);
          }
        } else {
          this.listbox.classList.remove("hovering");
          this._keyPressedInBox(evt);
        }
        break;

      case "wheel":
        // Don't let wheel events bubble to document. It will scroll the page
        // and close the entire narrate dialog.
        evt.stopPropagation();
        break;

      case "focus":
        if (!target.closest(".voiceselect")) {
          this.toggleList(false, false);
        }
        break;
    }
  },

  _getPagedOption(option, up) {
    let height = elem => elem.getBoundingClientRect().height;
    let listboxHeight = height(this.listbox);

    let next = option;
    for (let delta = 0; delta < listboxHeight; delta += height(next)) {
      let sibling = up ? next.previousElementSibling : next.nextElementSibling;
      if (!sibling) {
        break;
      }

      next = sibling;
    }

    return next;
  },

  _keyPressedButton(evt) {
    if (evt.altKey && (evt.key === "ArrowUp" || evt.key === "ArrowUp")) {
      this.toggleList(true);
      return;
    }

    let toSelect;
    switch (evt.key) {
      case "PageUp":
      case "ArrowUp":
        toSelect = this.selected.previousElementSibling;
        break;
      case "PageDown":
      case "ArrowDown":
        toSelect = this.selected.nextElementSibling;
        break;
      case "Home":
        toSelect = this.selected.parentNode.firstElementChild;
        break;
      case "End":
        toSelect = this.selected.parentNode.lastElementChild;
        break;
    }

    if (toSelect && toSelect.classList.contains("option")) {
      evt.preventDefault();
      this.selected = toSelect;
    }
  },

  _keyPressedInBox(evt) {
    let toFocus;
    let cur = this._doc.activeElement;

    switch (evt.key) {
      case "ArrowUp":
        toFocus = cur.previousElementSibling || this.listbox.lastElementChild;
        break;
      case "ArrowDown":
        toFocus = cur.nextElementSibling || this.listbox.firstElementChild;
        break;
      case "PageUp":
        toFocus = this._getPagedOption(cur, true);
        break;
      case "PageDown":
        toFocus = this._getPagedOption(cur, false);
        break;
      case "Home":
        toFocus = cur.parentNode.firstElementChild;
        break;
      case "End":
        toFocus = cur.parentNode.lastElementChild;
        break;
      case "Escape":
        this.toggleList(false);
        break;
    }

    if (toFocus && toFocus.classList.contains("option")) {
      evt.preventDefault();
      toFocus.focus();
    }
  },

  _select(option, suppressEvent = false) {
    let oldSelected = this.selected;
    if (oldSelected) {
      oldSelected.removeAttribute("aria-selected");
      oldSelected.classList.remove("selected");
    }

    if (option) {
      option.setAttribute("aria-selected", true);
      option.classList.add("selected");
      this.element.querySelector(".current-voice").textContent =
        option.textContent;
    }

    if (!suppressEvent) {
      let evt = this.element.ownerDocument.createEvent("Event");
      evt.initEvent("change", true, true);
      this.element.dispatchEvent(evt);
    }
  },

  _updateDropdownHeight(now) {
    let updateInner = () => {
      let winHeight = this._win.innerHeight;
      let listbox = this.listbox;
      let listboxTop = listbox.getBoundingClientRect().top;
      listbox.style.maxHeight = (winHeight - listboxTop - 10) + "px";
    };

    if (now) {
      updateInner();
    } else if (!this._pendingDropdownUpdate) {
      this._pendingDropdownUpdate = true;
      this._win.requestAnimationFrame(() => {
        updateInner();
        delete this._pendingDropdownUpdate;
      });
    }
  },

  _getOptionFromValue(value) {
    return Array.from(this.options).find(o => o.dataset.value === value);
  },

  get element() {
    return this._elementRef.get();
  },

  get listbox() {
    return this._elementRef.get().querySelector(".options");
  },

  get selectToggle() {
    return this._elementRef.get().querySelector(".select-toggle");
  },

  get _win() {
    return this._winRef.get();
  },

  get _doc() {
    return this._win.document;
  },

  set selected(option) {
    this._select(option);
  },

  get selected() {
    return this.element.querySelector(".options > .option.selected");
  },

  get options() {
    return this.element.querySelectorAll(".options > .option");
  },

  set value(value) {
    this._select(this._getOptionFromValue(value));
  },

  get value() {
    let selected = this.selected;
    return selected ? selected.dataset.value : "";
  }
};
