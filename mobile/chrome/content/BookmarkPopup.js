var BookmarkPopup = {
  get box() {
    delete this.box;
    this.box = document.getElementById("bookmark-popup");

    // Hide the popup if there is any new page loading
    let self = this;
    messageManager.addMessageListener("pagehide", function(aMessage) {
      self.hide();
    });
    return this.box;
  },

  hide : function hide() {
    this.box.hidden = true;
    BrowserUI.popPopup(this);
  },

  show : function show() {
    // Set the box position.
    let button = document.getElementById("tool-star");
    if (getComputedStyle(button).visibility == "visible") {
      let [tabsSidebar, controlsSidebar] = [Elements.tabs.getBoundingClientRect(), Elements.controls.getBoundingClientRect()];
      this.box.setAttribute(tabsSidebar.left < controlsSidebar.left ? "right" : "left", controlsSidebar.width - this.box.offset);
      this.box.top = button.getBoundingClientRect().top - this.box.offset;
    } else {
      button = document.getElementById("tool-star2");
      this.box.top = button.getBoundingClientRect().bottom - this.box.offset;

      let chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry);
      this.box.setAttribute(chromeReg.isLocaleRTL("global") ? "left" : "right", this.box.offset);
    }

    this.box.hidden = false;
    this.box.anchorTo(button);

    // include the star button here, so that click-to-dismiss works as expected
    BrowserUI.pushPopup(this, [this.box, button]);
  },

  toggle : function toggle() {
    if (this.box.hidden)
      this.show();
    else
      this.hide();
  }
};
