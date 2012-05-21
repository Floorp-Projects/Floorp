/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var TabletSidebar = {
  _grabbed: false, // true while the user is dragging the sidebar
  _offset: 0, // tracks how far the sidebar has been dragged
  _slideMultiplier: 1, // set greater than 1 to amplify sidebar drags (makes swiping easier)

  get visible() {
    return Elements.urlbarState.getAttribute("tablet_sidebar") == "true";
  },

  toggle: function toggle() {
    if (this.visible)
      this.hide();
    else
      this.show();
  },

  show: function show() {
    Elements.urlbarState.setAttribute("tablet_sidebar", "true");
    ViewableAreaObserver.update();
  },

  hide: function hide() {
    Elements.urlbarState.setAttribute("tablet_sidebar", "false");
    ViewableAreaObserver.update({ setIgnoreTabletSidebar: false });
  },

  /**
   * If scrolling by aDx pixels should grab the sidebar, grab and return true.
   * Otherwise return false.
   */
  tryGrab: function tryGrab(aDx) {
    if (aDx == 0)
      return false;

    let ltr = (Util.localeDir == Util.LOCALE_DIR_LTR);
    let willShow = ltr ? (aDx < 0) : (aDx > 0);

    if (willShow != this.visible) {
      this.grab();
      return true;
    }
    return false;
  },

  /**
   * Call this function in landscape tablet mode to begin dragging the tab sidebar.
   * Hiding the sidebar makes the viewable area grow; showing the sidebar makes it shrink.
   */
  grab: function grab() {
    this._grabbed = true;
    // While sliding the sidebar, tell ViewableAreaObserver to size the browser
    // as if the sidebar weren't there.  This avoids resizing the browser while dragging.
    ViewableAreaObserver.update({ setIgnoreTabletSidebar: true });

    let ltr = (Util.localeDir == Util.LOCALE_DIR_LTR);

    if (this.visible) {
      this._setOffset(ltr ? 0 : ViewableAreaObserver.sidebarWidth);
      this._slideMultiplier = 3;
    } else {
      // If the tab bar is hidden, un-collapse it but scroll it offscreen.
      this.show();
      this._setOffset(ltr ? ViewableAreaObserver.sidebarWidth : 0);
      this._slideMultiplier = 6;
    }
  },

  /** Move the tablet sidebar by aX pixels. */
  slideBy: function slideBy(aX) {
    this._setOffset(this._offset + (aX * this._slideMultiplier));
  },

  /** Call this when tablet sidebar dragging is finished. */
  ungrab: function ungrab() {
    if (!this._grabbed)
      return;
    this._grabbed = false;

    let finalOffset = this._offset;
    this._setOffset(0);

    let rtl = (Util.localeDir == Util.LOCALE_DIR_RTL);
    if (finalOffset > (ViewableAreaObserver.sidebarWidth / 2) ^ rtl)
      this.hide();
    else
      // we already called show() in grab; just need to update the width again.
      ViewableAreaObserver.update({ setIgnoreTabletSidebar: false });
  },

  /** Move the tablet sidebar. */
  _setOffset: function _setOffset(aOffset) {
    this._offset = aOffset;
    let scrollX = Util.clamp(aOffset, 0, ViewableAreaObserver.sidebarWidth);
    Browser.controlsScrollboxScroller.scrollTo(scrollX, 0);
  }
}
