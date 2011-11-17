/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matt Brubeck <mbrubeck@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


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
    ViewableAreaObserver.update();
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
