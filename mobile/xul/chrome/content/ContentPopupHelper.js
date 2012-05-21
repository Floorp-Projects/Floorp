/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var ContentPopupHelper = {
  _popup: null,
  get popup() {
    return this._popup;
  },

  set popup(aPopup) {
    // If there is nothing to do then bail out
    if (!this._popup && !aPopup)
      return;

    if (this._popup) {
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

    if (this._popup) {
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
    }
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
        popup.style.visibility = "hidden";
        break;

      case "PanFinished":
      case "AnimatedZoomEnd":
        this.anchorTo();
        break;

      case "MozBeforeResize":
        popup.style.visibility = "hidden";

        // When screen orientation changes, we have to ensure that
        // the popup width doesn't overflow the content's visible
        // area.
        popup.firstChild.style.maxWidth = "0px";
        break;

      case "resize":
        window.setTimeout(function(self) {
          self.anchorTo();
        }, 0, this);
        break;
    }
  }
};

