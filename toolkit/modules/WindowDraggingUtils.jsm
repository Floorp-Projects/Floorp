/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const HAVE_CSS_WINDOW_DRAG_SUPPORT = ["win", "macosx"].includes(AppConstants.platform);

var EXPORTED_SYMBOLS = [ "WindowDraggingElement" ];

function WindowDraggingElement(elem) {
  if (HAVE_CSS_WINDOW_DRAG_SUPPORT) {
    return;
  }
  this._elem = elem;
  this._window = elem.ownerGlobal;
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
  handleEvent(aEvent) {
    switch (aEvent.type) {
      case "mousedown":
        if (this.shouldDrag(aEvent)) {
          this._window.addEventListener("mousemove", this, { once: true });
          this._window.addEventListener("mouseup", this, { once: true });
        }
        break;
      case "mousemove":
        this._window.beginWindowMove(aEvent);
        this._window.removeEventListener("mouseup", this);
        break;
      case "mouseup":
        this._window.removeEventListener("mousemove", this);
        break;
    }
  }
};
