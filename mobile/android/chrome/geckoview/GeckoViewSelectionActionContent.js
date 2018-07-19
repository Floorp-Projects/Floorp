/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/GeckoViewContentModule.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

// Dispatches GeckoView:ShowSelectionAction and GeckoView:HideSelectionAction to
// the GeckoSession on accessible caret changes.
class GeckoViewSelectionActionContent extends GeckoViewContentModule {
  constructor(aModuleName, aMessageManager) {
    super(aModuleName, aMessageManager);

    this._seqNo = 0;
    this._isActive = false;
    this._previousMessage = "";

    this._actions = [{
      id: "org.mozilla.geckoview.HIDE",
      predicate: _ => true,
      perform: _ => this.handleEvent({type: "pagehide"}),
    }, {
      id: "org.mozilla.geckoview.CUT",
      predicate: e => !e.collapsed && e.selectionEditable && !this._isPasswordField(e),
      perform: _ => docShell.doCommand("cmd_cut"),
    }, {
      id: "org.mozilla.geckoview.COPY",
      predicate: e => !e.collapsed && !this._isPasswordField(e),
      perform: _ => docShell.doCommand("cmd_copy"),
    }, {
      id: "org.mozilla.geckoview.PASTE",
      predicate: e => e.selectionEditable &&
                      Services.clipboard.hasDataMatchingFlavors(
                          ["text/unicode"], 1, Ci.nsIClipboard.kGlobalClipboard),
      perform: _ => docShell.doCommand("cmd_paste"),
    }, {
      id: "org.mozilla.geckoview.DELETE",
      predicate: e => !e.collapsed && e.selectionEditable,
      perform: _ => docShell.doCommand("cmd_delete"),
    }, {
      id: "org.mozilla.geckoview.COLLAPSE_TO_START",
      predicate: e => !e.collapsed && e.selectionEditable,
      perform: e => docShell.doCommand("cmd_moveLeft"),
    }, {
      id: "org.mozilla.geckoview.COLLAPSE_TO_END",
      predicate: e => !e.collapsed && e.selectionEditable,
      perform: e => docShell.doCommand("cmd_moveRight"),
    }, {
      id: "org.mozilla.geckoview.UNSELECT",
      predicate: e => !e.collapsed && !e.selectionEditable,
      perform: e => docShell.doCommand("cmd_selectNone"),
    }, {
      id: "org.mozilla.geckoview.SELECT_ALL",
      predicate: e => e.reason !== "longpressonemptycontent",
      perform: e => docShell.doCommand("cmd_selectAll"),
    }];
  }

  _isPasswordField(aEvent) {
    if (!aEvent.selectionEditable) {
      return false;
    }

    const win = aEvent.target.defaultView;
    const focus = aEvent.target.activeElement;
    return win && win.HTMLInputElement &&
           focus instanceof win.HTMLInputElement &&
           !focus.mozIsTextField(/* excludePassword */ true);
  }

  _getFrameOffset(aEvent) {
    // Get correct offset in case of nested iframe.
    const offset = {
      left: 0,
      top: 0,
    };

    let currentWindow = aEvent.target.defaultView;
    while (currentWindow.realFrameElement) {
      const currentRect = currentWindow.realFrameElement.getBoundingClientRect();
      currentWindow = currentWindow.realFrameElement.ownerGlobal;

      offset.left += currentRect.left;
      offset.top += currentRect.top;

      let targetDocShell = currentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                        .getInterface(Ci.nsIWebNavigation);
      if (targetDocShell.isMozBrowser) {
        break;
      }
    }

    return offset;
  }

  onEnable() {
    debug `onEnable`;
    addEventListener("mozcaretstatechanged", this, { mozSystemGroup: true });
    addEventListener("pagehide", this, { capture: true, mozSystemGroup: true });
  }

  onDisable() {
    debug `onDisable`;
    removeEventListener("mozcaretstatechanged", this, { mozSystemGroup: true });
    removeEventListener("pagehide", this, { capture: true, mozSystemGroup: true });
  }

  /**
   * Receive and act on AccessibleCarets caret state-change
   * (mozcaretstatechanged and pagehide) events.
   */
  handleEvent(aEvent) {
    if (aEvent.type === "pagehide") {
      // Hide any selection actions on page hide.
      aEvent = {
        reason: "visibilitychange",
        caretVisibile: false,
        selectionVisible: false,
        collapsed: true,
        selectionEditable: false,
      };
    }

    let reason = aEvent.reason;

    if (this._isActive && !aEvent.caretVisible) {
      // For mozcaretstatechanged, "visibilitychange" means the caret is hidden.
      reason = "visibilitychange";
    } else if (!aEvent.collapsed &&
               !aEvent.selectionVisible) {
      reason = "invisibleselection";
    } else if (!this._isActive &&
               aEvent.selectionEditable &&
               aEvent.collapsed &&
               reason !== "longpressonemptycontent" &&
               reason !== "taponcaret" &&
               !Services.prefs.getBoolPref(
                   "geckoview.selection_action.show_on_focus", false)) {
      // Don't show selection actions when merely focusing on an editor or
      // repositioning the cursor. Wait until long press or the caret is tapped
      // in order to match Android behavior.
      reason = "visibilitychange";
    }

    debug `handleEvent: ${reason}`;

    if (["longpressonemptycontent",
         "releasecaret",
         "taponcaret",
         "updateposition"].includes(reason)) {

      const actions = this._actions.filter(
          action => action.predicate.call(this, aEvent));

      const offset = this._getFrameOffset(aEvent);
      const password = this._isPasswordField(aEvent);

      const msg = {
        type: "GeckoView:ShowSelectionAction",
        seqNo: this._seqNo,
        collapsed: aEvent.collapsed,
        editable: aEvent.selectionEditable,
        password,
        selection: password ? "" : aEvent.selectedTextContent,
        clientRect: !aEvent.boundingClientRect ? null : {
          left: aEvent.boundingClientRect.left + offset.left,
          top: aEvent.boundingClientRect.top + offset.top,
          right: aEvent.boundingClientRect.right + offset.left,
          bottom: aEvent.boundingClientRect.bottom + offset.top,
        },
        actions: actions.map(action => action.id),
      };

      try {
        if (msg.clientRect) {
          msg.clientRect.bottom += parseFloat(Services.prefs.getCharPref(
              "layout.accessiblecaret.height", "0"));
        }
      } catch (e) {
      }

      if (this._isActive && JSON.stringify(msg) === this._previousMessage) {
        // Don't call again if we're already active and things haven't changed.
        return;
      }

      msg.seqNo = ++this._seqNo;
      this._isActive = true;
      this._previousMessage = JSON.stringify(msg);

      // This event goes to GeckoViewSelectionAction.jsm, where the data is
      // further transformed and then sent to GeckoSession.
      this.eventDispatcher.sendRequest(msg, {
        onSuccess: response => {
          if (response.seqNo !== this._seqNo) {
            // Stale action.
            warn `Stale response ${response.id}`;
            return;
          }
          let action = actions.find(action => action.id === response.id);
          if (action) {
            debug `Performing ${response.id}`;
            action.perform.call(this, aEvent, response);
          } else {
            warn `Invalid action ${response.id}`;
          }
        },
        onError: _ => {
          // Do nothing; we can get here if the delegate was just unregistered.
        },
      });

    } else if (["invisibleselection",
                "presscaret",
                "scroll",
                "visibilitychange"].includes(reason)) {

      if (!this._isActive) {
        return;
      }

      this._isActive = false;

      // Mark previous actions as stale. Don't do this for "invisibleselection"
      // or "scroll" because previous actions should still be valid even after
      // these events occur.
      if (reason !== "invisibleselection" && reason !== "scroll") {
        this._seqNo++;
      }

      this.eventDispatcher.sendRequest({
        type: "GeckoView:HideSelectionAction",
        reason: reason,
      });

    } else {
      warn `Unknown reason: ${reason}`;
    }
  }
}

let {debug, warn} =
    GeckoViewSelectionActionContent.initLogging("GeckoViewSelectionAction");
let module = GeckoViewSelectionActionContent.create(this);
