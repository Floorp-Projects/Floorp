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
 * The Original Code is mozilla.org code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

var ContentPopupHelper = {
  _popup: null,
  get popup() {
    return this._popup;
  },

  set popup(aPopup) {
    // If there is nothing to do then bail out
    if (!this._popup && !aPopup)
      return;

    if (aPopup) {
      // Keeps the new popup element hidden until it is positioned, but using
      // visibility: 'hidden' instead of display: 'none' will allow to
      // workaround some bugs with arrowscrollbox's arrows that does not
      // update their hidden state correctly when used as a child (because of
      // missed underflow events)
      //
      // Also the popup is moved to the left border of the window otherwise the
      // sidebars position are messed up when the content of the popup's child
      // is too wide, but since the size of the popup can't be larger than 75%
      // of the window width (see the update method) then the problem does not
      // appear if the popup is positioned between 0-25% percent of the window
      // width
      aPopup.left = 0;
      aPopup.firstChild.style.maxWidth = "0px";
      aPopup.style.visibility = "hidden";
      aPopup.hidden = false;

      window.addEventListener("TabSelect", this, false);
      window.addEventListener("TabClose", this, false);
      window.addEventListener("AnimatedZoomBegin", this, false);
      window.addEventListener("AnimatedZoomEnd", this, false);
      window.addEventListener("MozBeforeResize", this, true);
      window.addEventListener("resize", this, false);
      Elements.browsers.addEventListener("PanBegin", this, false);
      Elements.browsers.addEventListener("PanFinished", this, false);
    } else {
      this._popup.hidden = true;
      this._anchorRect = null;

      window.removeEventListener("TabSelect", this, false);
      window.removeEventListener("TabClose", this, false);
      window.removeEventListener("AnimatedZoomBegin", this, false);
      window.removeEventListener("AnimatedZoomEnd", this, false);
      window.removeEventListener("MozBeforeResize", this, true);
      window.removeEventListener("resize", this, false);
      Elements.browsers.removeEventListener("PanBegin", this, false);
      Elements.browsers.removeEventListener("PanFinished", this, false);

      let event = document.createEvent("Events");
      event.initEvent("contentpopuphidden", true, false);
      this._popup.dispatchEvent(event);
    }

    this._popup = aPopup;
  },

  /**
   * This method positions an arrowbox on the screen using a 'virtual'
   * element as referrer that match the real content element
   * This method calls element.getBoundingClientRect() many times and can be
   * expensive, do not call it too many times.
   */
  anchorTo: function(aAnchorRect) {
    let popup = this._popup;
    if (!popup)
      return;

    // Use the old rect until we get a new one
    this._anchorRect = aAnchorRect ? aAnchorRect : this._anchorRect;

    // Calculate the maximum size of the arrowpanel by allowing it to live only
    // on the visible browser area
    let [leftVis, rightVis, leftW, rightW] = Browser.computeSidebarVisibility();
    let leftOffset = leftVis * leftW;
    let rightOffset = rightVis * rightW;
    let visibleAreaWidth = window.innerWidth - leftOffset - rightOffset;
    popup.firstChild.style.maxWidth = (visibleAreaWidth * 0.75) + "px";

    let browser = getBrowser();
    let rect = this._anchorRect.clone().scale(browser.scale, browser.scale);
    let scroll = browser.getRootView().getPosition();

    // The sidebars scroll needs to be taken into account, otherwise the arrows
    // can be misplaced if the sidebars are open
    let topOffset = (BrowserUI.toolbarH - Browser.getScrollboxPosition(Browser.pageScrollboxScroller).y);

    // Notifications take height _before_ the browser if there any
    let notification = Browser.getNotificationBox().currentNotification;
    if (notification)
      topOffset += notification.getBoundingClientRect().height;

    let virtualContentRect = {
      width: rect.width,
      height: rect.height,
      left: Math.ceil(rect.left - scroll.x + leftOffset - rightOffset),
      right: Math.floor(rect.left + rect.width - scroll.x + leftOffset - rightOffset),
      top: Math.ceil(rect.top - scroll.y + topOffset),
      bottom: Math.floor(rect.top + rect.height - scroll.y + topOffset)
    };

    // Translate the virtual rect inside the bounds of the viewable area if it
    // overflow
    if (virtualContentRect.left + virtualContentRect.width > visibleAreaWidth) {
      let offsetX = visibleAreaWidth - (virtualContentRect.left + virtualContentRect.width);
      virtualContentRect.width += offsetX;
      virtualContentRect.right -= offsetX;
    }

    if (virtualContentRect.left < leftOffset) {
      let offsetX = (virtualContentRect.right - virtualContentRect.width);
      virtualContentRect.width += offsetX;
      virtualContentRect.left -= offsetX;
    }

    // If the suggestions are out of view there is no need to display it
    let browserRect = Rect.fromRect(browser.getBoundingClientRect());
    if (BrowserUI.isToolbarLocked()) {
      // If the toolbar is locked, it can appear over the field in such a way
      // that the field is hidden
      let toolbarH = BrowserUI.toolbarH;
      browserRect = new Rect(leftOffset - rightOffset, Math.max(0, browserRect.top - toolbarH) + toolbarH,
                             browserRect.width + leftOffset - rightOffset, browserRect.height - toolbarH);
    }

    if (browserRect.intersect(Rect.fromRect(virtualContentRect)).isEmpty()) {
      popup.style.visibility = "hidden";
      return;
    }

    // Adding rect.height to the top moves the arrowbox below the virtual field
    let left = rect.left - scroll.x + leftOffset - rightOffset;
    let top = rect.top - scroll.y + topOffset + (rect.height);

    // Ensure parts of the arrowbox are not outside the window
    let arrowboxRect = Rect.fromRect(popup.getBoundingClientRect());
    if (left + arrowboxRect.width > window.innerWidth)
      left -= (left + arrowboxRect.width - window.innerWidth);
    else if (left < leftOffset)
      left += (leftOffset - left);
    popup.left = left;

    // Do not position the suggestions over the navigation buttons
    let buttonsHeight = Elements.contentNavigator.getBoundingClientRect().height;
    if (top + arrowboxRect.height >= window.innerHeight - buttonsHeight)
      top -= (rect.height + arrowboxRect.height);
    popup.top = top;

    // Create a virtual element to point to
    let virtualContentElement = {
      getBoundingClientRect: function() {
        return virtualContentRect;
      }
    };
    popup.anchorTo(virtualContentElement);
    popup.style.visibility = "visible";

    let event = document.createEvent("Events");
    event.initEvent("contentpopupshown", true, false);
    popup.dispatchEvent(event);
  },

  handleEvent: function(aEvent) {
    let popup = this._popup;
    if (!popup || !this._anchorRect)
      return;

    switch(aEvent.type) {
      case "TabSelect":
      case "TabClose":
        this.popup = null;
        break;

      case "PanBegin":
      case "AnimatedZoomBegin":
        popup.left = 0;
        popup.style.visibility = "hidden";
        break;

      case "PanFinished":
      case "AnimatedZoomEnd":
        this.anchorTo();
        break;

      case "MozBeforeResize":
        popup.left = 0;
        break;

      case "resize":
        window.setTimeout(function(self) {
          self.anchorTo();
        }, 0, this);
        break;
    }
  }
};

