var AppMenu = {
  get panel() {
    delete this.panel;
    return this.panel = document.getElementById("appmenu");
  },

  show: function show() {
    if (BrowserUI.activePanel || BrowserUI.isPanelVisible())
      return;
    this.panel.setAttribute("count", this.panel.childNodes.length);
    this.panel.hidden = false;

    addEventListener("keypress", this, true);

    BrowserUI.lockToolbar();
    BrowserUI.pushPopup(this, [this.panel, Elements.toolbarContainer]);
  },

  hide: function hide() {
    this.panel.hidden = true;

    removeEventListener("keypress", this, true);

    BrowserUI.unlockToolbar();
    BrowserUI.popPopup(this);
  },

  toggle: function toggle() {
    this.panel.hidden ? this.show() : this.hide();
  },

  handleEvent: function handleEvent(aEvent) {
    this.hide();
  }
};
