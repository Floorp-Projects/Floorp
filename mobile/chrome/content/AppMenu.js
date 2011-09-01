var AppMenu = {
  offset: 10,
  get panel() {
    delete this.panel;
    return this.panel = document.getElementById("appmenu");
  },

  get popup() {
    delete this.popup;
    return this.popup = document.getElementById("appmenu-popup");
  },

  shouldShow: function appmenu_shouldShow(aElement) {
    return !aElement.hidden;
  },

  overflowMenu : [],

  show: function show() {
    let modals = document.getElementsByClassName("modal-block").length;
    if (AwesomeScreen.activePanel || BrowserUI.isPanelVisible() || modals > 0 || BrowserUI.activeDialog)
      return;

    // Figure if we should show a menu-list or a pop-up menu
    let menuButton = document.getElementById("tool-menu");
    let listFormat = (getComputedStyle(menuButton).visibility == "visible");

    addEventListener("keypress", this, true);

    if (listFormat) {
      let listbox = document.getElementById("appmenu-popup-appcommands");
      while (listbox.firstChild)
        listbox.removeChild(listbox.firstChild);

      let siteCommandsBox = document.getElementById("appmenu-popup-sitecommands");
      while (siteCommandsBox.firstChild)
        siteCommandsBox.removeChild(siteCommandsBox.firstChild);

      let childrenCount = this.panel.childElementCount;
      for (let i = 0; i < childrenCount; i++) {
        let child = this.panel.children[i];

        if (!this.shouldShow(child))
          continue;

        child.setAttribute("show", true);

        let item = document.createElement("richlistitem");
        item.setAttribute("class", child.className);
        item.onclick = function() { child.click(); }

        let label = document.createElement("label");
        label.setAttribute("value", child.label);
        item.appendChild(label);

        if (item.classList.contains("appmenu-pageaction"))
          siteCommandsBox.appendChild(item);
        else
          listbox.appendChild(item);
      }

      this.popup.top = menuButton.getBoundingClientRect().bottom;

      let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
      this.popup.setAttribute(chromeReg.isLocaleRTL("global") ? "left" : "right", this.offset);

      this.popup.hidden = false;
      this.popup.anchorTo(menuButton);

      BrowserUI.lockToolbar();
      BrowserUI.pushPopup(this, [this.popup, menuButton]);
    } else {
      let shown = 0;
      let lastShown = null;
      this.overflowMenu = [];
      let childrenCount = this.panel.childElementCount;
      for (let i = 0; i < childrenCount; i++) {
        if (this.shouldShow(this.panel.children[i])) {
          if (shown == 6 && this.overflowMenu.length == 0) {
            // if we are trying to show more than 6 elements fall back to showing a more button
            lastShown.removeAttribute("show");
            this.overflowMenu.push(lastShown);
            this.panel.appendChild(this.createMoreButton());
          }
          if (this.overflowMenu.length > 0) {
            this.overflowMenu.push(this.panel.children[i]);
          } else {
            lastShown = this.panel.children[i];
            lastShown.setAttribute("show", shown);
            shown++;
          }
        }
      }

      this.panel.setAttribute("count", shown);
      this.panel.hidden = false;

      BrowserUI.lockToolbar();
      BrowserUI.pushPopup(this, [this.panel, Elements.toolbarContainer]);
    }
  },

  hide: function hide() {
    this.panel.hidden = true;
    this.popup.hidden = true;
    let moreButton = document.getElementById("appmenu-more-button");
    if (moreButton)
      moreButton.parentNode.removeChild(moreButton);

    for (let i = 0; i < this.panel.childElementCount; i++) {
      if (this.panel.children[i].hasAttribute("show"))
        this.panel.children[i].removeAttribute("show");
    }

    removeEventListener("keypress", this, true);

    BrowserUI.unlockToolbar();
    BrowserUI.popPopup(this);
  },

  toggle: function toggle() {
    (this.panel.hidden && this.popup.hidden) ? this.show() : this.hide();
  },

  handleEvent: function handleEvent(aEvent) {
    this.hide();
  },

  showAsList: function showAsList() {
    // allow menu to hide to remove the more button before we show the menulist
    setTimeout((function() {
      AppMenuOverflow.show(this.overflowMenu);
    }).bind(this), 0)
  },

  createMoreButton: function() {
    let button = document.createElement("toolbarbutton");
    button.setAttribute("id", "appmenu-more-button");
    button.setAttribute("class", "appmenu-button");
    button.setAttribute("label", Strings.browser.GetStringFromName("appMenu.more"));
    button.setAttribute("show", 6);
    button.setAttribute("oncommand", "AppMenu.showAsList();");
    return button;
  }
};


var AppMenuOverflow = {
  get container() {
    delete this.container;
    return this.container = document.getElementById("appmenu-overflow");
  },
  
  get list() {
    delete this.list;
    return this.list = document.getElementById("appmenu-overflow-commands");
  },
  
  show: function show(aList) {
    let container = this.container;
    let listbox = this.list;
    while (listbox.firstChild)
      listbox.removeChild(listbox.firstChild);

    let children = aList;
    for (let i = 0; i < children.length; i++) {
      let child = children[i];
      let item = document.createElement("richlistitem");
      item.setAttribute("class", "appmenu-button");
      item.onclick = function() { child.click(); }

      let label = document.createElement("label");
      label.setAttribute("value", child.label);
      item.appendChild(label);

      listbox.appendChild(item);
    }

    container.hidden = false;
    BrowserUI.pushPopup(this, [this.container]);
  },

  hide: function hide() {
    this.container.hidden = true;
    BrowserUI.popPopup(this);
  }
};
