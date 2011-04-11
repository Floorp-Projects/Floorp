var AppMenu = {
  get panel() {
    delete this.panel;
    return this.panel = document.getElementById("appmenu");
  },

  shouldShow: function appmenu_shouldShow(aElement) {
    return !aElement.hidden;
  },

  overflowMenu : [],

  show: function show() {
    if (BrowserUI.activePanel || BrowserUI.isPanelVisible())
      return;

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

    addEventListener("keypress", this, true);

    BrowserUI.lockToolbar();
    BrowserUI.pushPopup(this, [this.panel, Elements.toolbarContainer]);
  },

  hide: function hide() {
    this.panel.hidden = true;
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
    this.panel.hidden ? this.show() : this.hide();
  },

  handleEvent: function handleEvent(aEvent) {
    this.hide();
  },

  showAsList: function showAsList() {
    // allow menu to hide to remove the more button before we show the menulist
    setTimeout((function() {
      this.menu.menupopup.children = this.overflowMenu;
      MenuListHelperUI.show(this.menu);
    }).bind(this), 0)
  },

  menu : {
    id: "appmenu-menulist",
    dispatchEvent: function(aEvent) {
      let menuitem = AppMenu.overflowMenu[this.selectedIndex];
      if (menuitem)
        menuitem.click();
    },
    menupopup: {
      hasAttribute: function(aAttr) { return false; }
    },
    selectedIndex: -1
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
