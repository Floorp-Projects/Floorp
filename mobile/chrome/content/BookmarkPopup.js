var BookmarkPopup = {
  get box() {
    delete this.box;
    this.box = document.getElementById("bookmark-popup");

    let [tabsSidebar, controlsSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
    this.box.setAttribute(tabsSidebar.left < controlsSidebar.left ? "right" : "left", controlsSidebar.width - this.box.offset);
    this.box.top = BrowserUI.starButton.getBoundingClientRect().top - this.box.offset;

    // Hide the popup if there is any new page loading
    let self = this;
    messageManager.addMessageListener("pagehide", function(aMessage) {
      self.hide();
    });

    return this.box;
  },

  _bookmarkPopupTimeout: -1,

  hide : function hide() {
    if (this._bookmarkPopupTimeout != -1) {
      clearTimeout(this._bookmarkPopupTimeout);
      this._bookmarkPopupTimeout = -1;
    }
    this.box.hidden = true;
    BrowserUI.popPopup(this);
  },

  show : function show() {
    this.box.hidden = false;
    this.box.anchorTo(BrowserUI.starButton);

    // include starButton here, so that click-to-dismiss works as expected
    BrowserUI.pushPopup(this, [this.box, BrowserUI.starButton]);
  },

  autoHide: function autoHide() {
    if (this._bookmarkPopupTimeout != -1 || this.box.hidden)
      return;

    this._bookmarkPopupTimeout = setTimeout(function (self) {
      self._bookmarkPopupTimeout = -1;
      self.hide();
    }, 2000, this);
  },

  toggle : function toggle() {
    if (this.box.hidden)
      this.show();
    else
      this.hide();
  }
};
