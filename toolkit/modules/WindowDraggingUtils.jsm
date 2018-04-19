/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const HAVE_CSS_WINDOW_DRAG_SUPPORT = ["win", "macosx"].includes(AppConstants.platform);

var EXPORTED_SYMBOLS = [ "WindowDraggingElement" ];

function WindowDraggingElement(elem) {
  this._elem = elem;
  this._window = elem.ownerGlobal;
  if (HAVE_CSS_WINDOW_DRAG_SUPPORT && !this.isPanel()) {
    return;
  }

  this._elem.addEventListener("mousedown", this);
}

WindowDraggingElement.prototype = {
  mouseDownCheck(e) { return true; },
  dragTags: ["box", "hbox", "vbox", "spacer", "label", "statusbarpanel", "stack",
             "toolbaritem", "toolbarseparator", "toolbarspring", "toolbarspacer",
             "radiogroup", "deck", "scrollbox", "arrowscrollbox", "tabs"],
  shouldDrag(aEvent) {
    if (aEvent.button != 0 ||
        this._window.fullScreen ||
        !this.mouseDownCheck.call(this._elem, aEvent) ||
        aEvent.defaultPrevented)
      return false;

    let target = aEvent.originalTarget, parent = aEvent.originalTarget;

    // The target may be inside an embedded iframe or browser. (bug 615152)
    if (target.ownerGlobal != this._window)
      return false;

    while (parent != this._elem) {
      let mousethrough = parent.getAttribute("mousethrough");
      if (mousethrough == "always")
        target = parent.parentNode;
      else if (mousethrough == "never")
        break;
      parent = parent.parentNode;
    }
    while (target != this._elem) {
      if (!this.dragTags.includes(target.localName))
        return false;
      target = target.parentNode;
    }
    return true;
  },
  isPanel() {
    return ChromeUtils.getClassName(this._elem) == "XULElement" &&
           this._elem.localName == "panel";
  },
  handleEvent(aEvent) {
    let isPanel = this.isPanel();
    switch (aEvent.type) {
      case "mousedown":
        if (!this.shouldDrag(aEvent))
          return;
        if (!/^gtk/i.test(AppConstants.MOZ_WIDGET_TOOLKIT)) {
          if (isPanel) {
            let screenRect = this._elem.getOuterScreenRect();
            this._deltaX = aEvent.screenX - screenRect.left;
            this._deltaY = aEvent.screenY - screenRect.top;
          } else {
            this._deltaX = aEvent.screenX - this._window.screenX;
            this._deltaY = aEvent.screenY - this._window.screenY;
          }
        }
        this._draggingWindow = true;
        this._window.addEventListener("mousemove", this);
        this._window.addEventListener("mouseup", this);
        break;
      case "mousemove":
        if (/^gtk/i.test(AppConstants.MOZ_WIDGET_TOOLKIT)) {
          // On GTK, there is a toolkit-level function which handles
          // window dragging. We want to start moving the window
          // on the first mousemove event after mousedown.
          this._window.beginWindowMove(aEvent, isPanel ? this._elem : null);
          this._window.removeEventListener("mousemove", this);
          break;
        }
        if (this._draggingWindow) {
          let toDrag = this.isPanel() ? this._elem : this._window;
          toDrag.moveTo(aEvent.screenX - this._deltaX, aEvent.screenY - this._deltaY);
        }
        break;
      case "mouseup":
        if (this._draggingWindow) {
          this._draggingWindow = false;
          this._window.removeEventListener("mousemove", this);
          this._window.removeEventListener("mouseup", this);
        }
        break;
    }
  }
};
