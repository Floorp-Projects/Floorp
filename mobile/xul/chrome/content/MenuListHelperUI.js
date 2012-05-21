/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var MenuListHelperUI = {
  get _container() {
    delete this._container;
    return this._container = document.getElementById("menulist-container");
  },

  get _popup() {
    delete this._popup;
    return this._popup = document.getElementById("menulist-popup");
  },

  get _title() {
    delete this._title;
    return this._title = document.getElementById("menulist-title");
  },

  _firePopupEvent: function firePopupEvent(aEventName) {
    let menupopup = this._currentList.menupopup;
    if (menupopup.hasAttribute(aEventName)) {
      let func = new Function("event", menupopup.getAttribute(aEventName));
      func.call(this);
    }
  },

  _currentList: null,
  show: function mn_show(aMenulist) {
    this._currentList = aMenulist;
    this._container.setAttribute("for", aMenulist.id);
    this._title.value = aMenulist.title || "";
    this._firePopupEvent("onpopupshowing");

    let container = this._container;
    let listbox = this._popup.lastChild;
    while (listbox.firstChild)
      listbox.removeChild(listbox.firstChild);

    let children = this._currentList.menupopup.children;
    for (let i = 0; i < children.length; i++) {
      let child = children[i];
      let item = document.createElement("richlistitem");
      if (child.disabled)
        item.setAttribute("disabled", "true");
      if (child.hidden)
        item.setAttribute("hidden", "true");

      // Add selected as a class name instead of an attribute to not being overidden
      // by the richlistbox behavior (it sets the "current" and "selected" attribute
      item.setAttribute("class", "option-command action-button" + (child.selected ? " selected" : ""));

      let image = document.createElement("image");
      image.setAttribute("src", child.image || "");
      item.appendChild(image);

      let label = document.createElement("label");
      label.setAttribute("value", child.label);
      item.appendChild(label);

      listbox.appendChild(item);
    }

    window.addEventListener("resize", this, true);
    this.sizeToContent();
    container.hidden = false;
    BrowserUI.pushPopup(this, [this._popup]);
  },

  hide: function mn_hide() {
    this._currentList = null;
    this._container.removeAttribute("for");
    this._container.hidden = true;
    window.removeEventListener("resize", this, true);
    BrowserUI.popPopup(this);
  },

  selectByIndex: function mn_selectByIndex(aIndex) {
    this._currentList.selectedIndex = aIndex;

    // Dispatch a xul command event to the attached menulist
    if (this._currentList.dispatchEvent) {
      let evt = document.createEvent("XULCommandEvent");
      evt.initCommandEvent("command", true, true, window, 0, false, false, false, false, null);
      this._currentList.dispatchEvent(evt);
    }

    this.hide();
  },

  sizeToContent: function sizeToContent() {
    let style = document.defaultView.getComputedStyle(this._container, null);
    this._popup.width = window.innerWidth - (parseInt(style.paddingLeft) + parseInt(style.paddingRight));
  },

  handleEvent: function handleEvent(aEvent) {
    this.sizeToContent();
  }
};
