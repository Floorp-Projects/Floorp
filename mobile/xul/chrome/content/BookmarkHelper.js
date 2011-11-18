var BookmarkHelper = {
  _editor: null,

  logging: false,

  log: function(msg) {
    if (this.logging) {
      Services.console.logStringMessage(msg);
    }
  },

  get box() {
    delete this.box;
    this.box = document.getElementById("bookmark-container");
    return this.box;
  },

  edit: function BH_edit(aURI) {
    if (!aURI)
      aURI = getBrowser().currentURI;

    let itemId = PlacesUtils.getMostRecentBookmarkForURI(aURI);
    if (itemId == -1)
      return;

    // When opening the bookmark helper dialog be sure there is not others
    // popup opened like the bookmark popup
    BookmarkPopup.hide();

    let title = PlacesUtils.bookmarks.getItemTitle(itemId);
    let tags = PlacesUtils.tagging.getTagsForURI(aURI, {});

    const XULNS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
    this._editor = document.createElementNS(XULNS, "placeitem");
    this._editor.setAttribute("id", "bookmark-item");
    this._editor.setAttribute("flex", "1");
    this._editor.setAttribute("type", "bookmark");
    this._editor.setAttribute("ui", "manage");
    this._editor.setAttribute("title", title);
    this._editor.setAttribute("uri", aURI.spec);
    this._editor.setAttribute("itemid", itemId);
    this._editor.setAttribute("tags", tags.join(", "));
    this._editor.setAttribute("onclose", "BookmarkHelper.close()");
    document.getElementById("bookmark-form").appendChild(this._editor);

    this.box.hidden = false;
    BrowserUI.pushDialog(this);

    function waitForWidget(self) {
      try {
        self._editor.startEditing();
      } catch(e) {
        setTimeout(waitForWidget, 0, self);
      }
    }
    setTimeout(waitForWidget, 0, this);
  },

  save: function BH_save() {
    this._editor.stopEditing(true);
  },

  close: function BH_close() {
    this.log("Bookmark helper: closing");
    BrowserUI.updateStar();

    // Note: the _editor will have already saved the data, if needed, by the time
    // this method is called, since this method is called via the "close" event.
    this._editor.parentNode.removeChild(this._editor);
    this._editor = null;

    BrowserUI.popDialog();
    this.box.hidden = true;
  },

  removeBookmarksForURI: function BH_removeBookmarksForURI(aURI) {
    //XXX blargle xpconnect! might not matter, but a method on
    // nsINavBookmarksService that takes an array of items to
    // delete would be faster. better yet, a method that takes a URI!
    let itemIds = PlacesUtils.getBookmarksForURI(aURI);
    itemIds.forEach(PlacesUtils.bookmarks.removeItem);

    BrowserUI.updateStar();
  }
};
