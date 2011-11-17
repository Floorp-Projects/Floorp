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
 *   Mark Finkle <mfinkle@mozilla.com>
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

var SelectionHelper = {
  enabled: true,
  popupState: null,
  target: null,
  deltaX: -1,
  deltaY: -1,

  get _start() {
    delete this._start;
    return this._start = document.getElementById("selectionhandle-start");
  },

  get _end() {
    delete this._end;
    return this._end = document.getElementById("selectionhandle-end");
  },

  showPopup: function sh_showPopup(aMessage) {
    if (!this.enabled || aMessage.json.types.indexOf("content-text") == -1)
      return false;

    this.popupState = aMessage.json;
    this.popupState.target = aMessage.target;

    this._start.customDragger = {
      isDraggable: function isDraggable(target, content) { return { x: true, y: false }; },
      dragStart: function dragStart(cx, cy, target, scroller) {},
      dragStop: function dragStop(dx, dy, scroller) { return false; },
      dragMove: function dragMove(dx, dy, scroller) { return false; }
    };

    this._end.customDragger = {
      isDraggable: function isDraggable(target, content) { return { x: true, y: false }; },
      dragStart: function dragStart(cx, cy, target, scroller) {},
      dragStop: function dragStop(dx, dy, scroller) { return false; },
      dragMove: function dragMove(dx, dy, scroller) { return false; }
    };

    this._start.addEventListener("TapUp", this, true);
    this._end.addEventListener("TapUp", this, true);

    messageManager.addMessageListener("Browser:SelectionRange", this);
    messageManager.addMessageListener("Browser:SelectionCopied", this);

    this.popupState.target.messageManager.sendAsyncMessage("Browser:SelectionStart", { x: this.popupState.x, y: this.popupState.y });

    // Hide the selection handles
    window.addEventListener("TapDown", this, true);
    window.addEventListener("resize", this, true);
    window.addEventListener("keypress", this, true);
    Elements.browsers.addEventListener("URLChanged", this, true);
    Elements.browsers.addEventListener("SizeChanged", this, true);
    Elements.browsers.addEventListener("ZoomChanged", this, true);

    let event = document.createEvent("Events");
    event.initEvent("CancelTouchSequence", true, false);
    this.popupState.target.dispatchEvent(event);

    return true;
  },

  hide: function sh_hide(aEvent) {
    if (this._start.hidden)
      return;

    let pos = this.popupState.target.transformClientToBrowser(aEvent.clientX || 0, aEvent.clientY || 0);
    let json = {
      x: pos.x,
      y: pos.y
    };

    try {
      this.popupState.target.messageManager.sendAsyncMessage("Browser:SelectionEnd", json);
    } catch (e) {
      Cu.reportError(e);
    }

    this.popupState = null;

    this._start.hidden = true;
    this._end.hidden = true;

    this._start.removeEventListener("TapUp", this, true);
    this._end.removeEventListener("TapUp", this, true);

    messageManager.removeMessageListener("Browser:SelectionRange", this);

    window.removeEventListener("TapDown", this, true);
    window.removeEventListener("resize", this, true);
    window.removeEventListener("keypress", this, true);
    Elements.browsers.removeEventListener("URLChanged", this, true);
    Elements.browsers.removeEventListener("SizeChanged", this, true);
    Elements.browsers.removeEventListener("ZoomChanged", this, true);
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "TapDown":
        if (aEvent.target == this._start || aEvent.target == this._end) {
          this.target = aEvent.target;
          this.deltaX = (aEvent.clientX - this.target.left);
          this.deltaY = (aEvent.clientY - this.target.top);
          window.addEventListener("TapMove", this, true);
        } else {
          this.hide(aEvent);
        }
        break;
      case "TapUp":
        window.removeEventListener("TapMove", this, true);
        this.target = null;
        this.deltaX = -1;
        this.deltaY = -1;
        break;
      case "TapMove":
        if (this.target) {
          this.target.left = aEvent.clientX - this.deltaX;
          this.target.top = aEvent.clientY - this.deltaY;
          let rect = this.target.getBoundingClientRect();
          let data = this.target == this._start ? { x: rect.right, y: rect.top, type: "start" } : { x: rect.left, y: rect.top, type: "end" };
          let pos = this.popupState.target.transformClientToBrowser(data.x || 0, data.y || 0);
          let json = {
            type: data.type,
            x: pos.x,
            y: pos.y
          };
          this.popupState.target.messageManager.sendAsyncMessage("Browser:SelectionMove", json);
        }
        break;
      case "resize":
      case "SizeChanged":
      case "ZoomChanged":
      case "URLChanged":
      case "keypress":
        this.hide(aEvent);
        break;
    }
  },

  receiveMessage: function sh_receiveMessage(aMessage) {
    let json = aMessage.json;
    switch (aMessage.name) {
      case "Browser:SelectionRange": {
        let pos = this.popupState.target.transformBrowserToClient(json.start.x || 0, json.start.y || 0);
        this._start.left = pos.x - 32;
        this._start.top = pos.y + this.deltaY;
        this._start.hidden = false;

        pos = this.popupState.target.transformBrowserToClient(json.end.x || 0, json.end.y || 0);
        this._end.left = pos.x;
        this._end.top = pos.y;
        this._end.hidden = false;
        break;
      }

      case "Browser:SelectionCopied": {
        messageManager.removeMessageListener("Browser:SelectionCopied", this);
        if (json.succeeded) {
          let toaster = Cc["@mozilla.org/toaster-alerts-service;1"].getService(Ci.nsIAlertsService);
          toaster.showAlertNotification(null, Strings.browser.GetStringFromName("selectionHelper.textCopied"), "", false, "", null);
        }
        break;
      }
    }
  }
};
