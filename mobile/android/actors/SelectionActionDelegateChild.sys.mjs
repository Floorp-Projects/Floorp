/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { GeckoViewActorChild } from "resource://gre/modules/GeckoViewActorChild.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  LayoutUtils: "resource://gre/modules/LayoutUtils.sys.mjs",
});

const MAGNIFIER_PREF = "layout.accessiblecaret.magnifier.enabled";
const ACCESSIBLECARET_HEIGHT_PREF = "layout.accessiblecaret.height";
const PREFS = [MAGNIFIER_PREF, ACCESSIBLECARET_HEIGHT_PREF];

// Dispatches GeckoView:ShowSelectionAction and GeckoView:HideSelectionAction to
// the GeckoSession on accessible caret changes.
export class SelectionActionDelegateChild extends GeckoViewActorChild {
  constructor(aModuleName, aMessageManager) {
    super(aModuleName, aMessageManager);

    this._actionCallback = () => {};
    this._isActive = false;
    this._previousMessage = "";

    // Bug 1570744 - JSWindowActorChild's cannot be used as nsIObserver's
    // directly, so we create a new function here instead to act as our
    // nsIObserver, which forwards the notification to the observe method.
    this._observerFunction = (subject, topic, data) => {
      this.observe(subject, topic, data);
    };
    for (const pref of PREFS) {
      Services.prefs.addObserver(pref, this._observerFunction);
    }

    this._magnifierEnabled = Services.prefs.getBoolPref(MAGNIFIER_PREF);
    this._accessiblecaretHeight = parseFloat(
      Services.prefs.getCharPref(ACCESSIBLECARET_HEIGHT_PREF, "0")
    );
  }

  didDestroy() {
    for (const pref of PREFS) {
      Services.prefs.removeObserver(pref, this._observerFunction);
    }
  }

  _actions = [
    {
      id: "org.mozilla.geckoview.HIDE",
      predicate: _ => true,
      perform: _ => this.handleEvent({ type: "pagehide" }),
    },
    {
      id: "org.mozilla.geckoview.CUT",
      predicate: e =>
        !e.collapsed && e.selectionEditable && !this._isPasswordField(e),
      perform: _ => this.docShell.doCommand("cmd_cut"),
    },
    {
      id: "org.mozilla.geckoview.COPY",
      predicate: e => !e.collapsed && !this._isPasswordField(e),
      perform: _ => this.docShell.doCommand("cmd_copy"),
    },
    {
      id: "org.mozilla.geckoview.PASTE",
      predicate: e =>
        (this._isContentHtmlEditable(e) &&
          Services.clipboard.hasDataMatchingFlavors(
            /* The following image types are considered by editor */
            ["image/gif", "image/jpeg", "image/png"],
            Ci.nsIClipboard.kGlobalClipboard
          )) ||
        (e.selectionEditable &&
          Services.clipboard.hasDataMatchingFlavors(
            ["text/plain"],
            Ci.nsIClipboard.kGlobalClipboard
          )),
      perform: _ => this._performPaste(),
    },
    {
      id: "org.mozilla.geckoview.PASTE_AS_PLAIN_TEXT",
      predicate: e =>
        this._isContentHtmlEditable(e) &&
        Services.clipboard.hasDataMatchingFlavors(
          ["text/html"],
          Ci.nsIClipboard.kGlobalClipboard
        ),
      perform: _ => this._performPasteAsPlainText(),
    },
    {
      id: "org.mozilla.geckoview.DELETE",
      predicate: e => !e.collapsed && e.selectionEditable,
      perform: _ => this.docShell.doCommand("cmd_delete"),
    },
    {
      id: "org.mozilla.geckoview.COLLAPSE_TO_START",
      predicate: e => !e.collapsed && e.selectionEditable,
      perform: e => this.docShell.doCommand("cmd_moveLeft"),
    },
    {
      id: "org.mozilla.geckoview.COLLAPSE_TO_END",
      predicate: e => !e.collapsed && e.selectionEditable,
      perform: e => this.docShell.doCommand("cmd_moveRight"),
    },
    {
      id: "org.mozilla.geckoview.UNSELECT",
      predicate: e => !e.collapsed && !e.selectionEditable,
      perform: e => this.docShell.doCommand("cmd_selectNone"),
    },
    {
      id: "org.mozilla.geckoview.SELECT_ALL",
      predicate: e => {
        if (e.reason === "longpressonemptycontent") {
          return false;
        }
        // When on design mode, focusedElement will be null.
        const element =
          Services.focus.focusedElement || e.target?.activeElement;
        if (e.selectionEditable && e.target && element) {
          let value = "";
          if (element.value) {
            value = element.value;
          } else if (
            element.isContentEditable ||
            e.target.designMode === "on"
          ) {
            value = element.innerText;
          }
          // Do not show SELECT_ALL if the editable is empty
          // or all the editable text is already selected.
          return value !== "" && value !== e.selectedTextContent;
        }
        return true;
      },
      perform: e => this.docShell.doCommand("cmd_selectAll"),
    },
  ];

  receiveMessage({ name, data }) {
    debug`receiveMessage ${name}`;

    switch (name) {
      case "ExecuteSelectionAction": {
        this._actionCallback(data);
      }
    }
  }

  _performPaste() {
    this.handleEvent({ type: "pagehide" });
    this.docShell.doCommand("cmd_paste");
  }

  _performPasteAsPlainText() {
    this.handleEvent({ type: "pagehide" });
    this.docShell.doCommand("cmd_pasteNoFormatting");
  }

  _isPasswordField(aEvent) {
    if (!aEvent.selectionEditable) {
      return false;
    }

    const win = aEvent.target.defaultView;
    const focus = aEvent.target.activeElement;
    return (
      win &&
      win.HTMLInputElement &&
      win.HTMLInputElement.isInstance(focus) &&
      !focus.mozIsTextField(/* excludePassword */ true)
    );
  }

  _isContentHtmlEditable(aEvent) {
    if (!aEvent.selectionEditable) {
      return false;
    }

    if (aEvent.target.designMode == "on") {
      return true;
    }

    // focused element isn't <input> nor <textarea>
    const win = aEvent.target.defaultView;
    const focus = Services.focus.focusedElement;
    return (
      win &&
      win.HTMLInputElement &&
      win.HTMLTextAreaElement &&
      !win.HTMLInputElement.isInstance(focus) &&
      !win.HTMLTextAreaElement.isInstance(focus)
    );
  }

  _getDefaultMagnifierPoint(aEvent) {
    const rect = lazy.LayoutUtils.rectToScreenRect(aEvent.target.ownerGlobal, {
      left: aEvent.clientX,
      top: aEvent.clientY - this._accessiblecaretHeight,
      width: 0,
      height: 0,
    });
    return { x: rect.left, y: rect.top };
  }

  _getBetterMagnifierPoint(aEvent) {
    const win = aEvent.target.defaultView;
    if (!win) {
      return this._getDefaultMagnifierPoint(aEvent);
    }

    const focus = aEvent.target.activeElement;
    if (
      win.HTMLInputElement?.isInstance(focus) &&
      focus.mozIsTextField(false)
    ) {
      // <input> element. Use vertical center position of input element.
      const bounds = focus.getBoundingClientRect();
      const rect = lazy.LayoutUtils.rectToScreenRect(
        aEvent.target.ownerGlobal,
        {
          left: aEvent.clientX,
          top: bounds.top,
          width: 0,
          height: bounds.height,
        }
      );
      return { x: rect.left, y: rect.top + rect.height / 2 };
    }

    if (win.HTMLTextAreaElement?.isInstance(focus)) {
      // TODO:
      // <textarea> element. How to get better selection bounds?
      return this._getDefaultMagnifierPoint(aEvent);
    }

    const selection = win.getSelection();
    if (selection.rangeCount != 1) {
      // When selecting text using accessible caret, selection count will be 1.
      // This situation means that current selection isn't into text.
      return this._getDefaultMagnifierPoint(aEvent);
    }

    // We are looking for better selection bounds, then use it.
    const bounds = (() => {
      const range = selection.getRangeAt(0);
      let distance = Number.MAX_SAFE_INTEGER;
      let y = aEvent.clientY;
      const rectList = range.getClientRects();
      for (const rect of rectList) {
        const newDistance = Math.abs(aEvent.clientY - rect.bottom);
        if (distance > newDistance) {
          y = rect.top + rect.height / 2;
          distance = newDistance;
        }
      }
      return { left: aEvent.clientX, top: y, width: 0, height: 0 };
    })();

    const rect = lazy.LayoutUtils.rectToScreenRect(
      aEvent.target.ownerGlobal,
      bounds
    );
    return { x: rect.left, y: rect.top };
  }

  _handleMagnifier(aEvent) {
    if (["presscaret", "dragcaret"].includes(aEvent.reason)) {
      debug`_handleMagnifier: ${aEvent.reason}`;
      const screenPoint = this._getBetterMagnifierPoint(aEvent);
      this.eventDispatcher.sendRequest({
        type: "GeckoView:ShowMagnifier",
        screenPoint,
      });
    } else if (aEvent.reason == "releasecaret") {
      debug`_handleMagnifier: ${aEvent.reason}`;
      this.eventDispatcher.sendRequest({
        type: "GeckoView:HideMagnifier",
      });
    }
  }

  /**
   * Receive and act on AccessibleCarets caret state-change
   * (mozcaretstatechanged and pagehide) events.
   */
  handleEvent(aEvent) {
    if (aEvent.type === "pagehide" || aEvent.type === "deactivate") {
      // Hide any selection actions on page hide or deactivate.
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
    } else if (!aEvent.collapsed && !aEvent.selectionVisible) {
      reason = "invisibleselection";
    } else if (
      !this._isActive &&
      aEvent.selectionEditable &&
      aEvent.collapsed &&
      reason !== "longpressonemptycontent" &&
      reason !== "taponcaret" &&
      !Services.prefs.getBoolPref(
        "geckoview.selection_action.show_on_focus",
        false
      )
    ) {
      // Don't show selection actions when merely focusing on an editor or
      // repositioning the cursor. Wait until long press or the caret is tapped
      // in order to match Android behavior.
      reason = "visibilitychange";
    }

    debug`handleEvent: ${reason}`;

    if (this._magnifierEnabled) {
      this._handleMagnifier(aEvent);
    }

    if (
      [
        "longpressonemptycontent",
        "releasecaret",
        "taponcaret",
        "updateposition",
      ].includes(reason)
    ) {
      const actions = this._actions.filter(action =>
        action.predicate.call(this, aEvent)
      );

      const screenRect = (() => {
        const boundingRect = aEvent.boundingClientRect;
        if (!boundingRect) {
          return null;
        }
        const rect = lazy.LayoutUtils.rectToScreenRect(
          aEvent.target.ownerGlobal,
          boundingRect
        );
        return {
          left: rect.left,
          top: rect.top,
          right: rect.right,
          bottom: rect.bottom + this._accessiblecaretHeight,
        };
      })();

      const password = this._isPasswordField(aEvent);

      const msg = {
        collapsed: aEvent.collapsed,
        editable: aEvent.selectionEditable,
        password,
        selection: password ? "" : aEvent.selectedTextContent,
        screenRect,
        actions: actions.map(action => action.id),
      };

      if (this._isActive && JSON.stringify(msg) === this._previousMessage) {
        // Don't call again if we're already active and things haven't changed.
        return;
      }

      this._isActive = true;
      this._previousMessage = JSON.stringify(msg);

      // We can't just listen to the response of the message because we accept
      // multiple callbacks.
      this._actionCallback = data => {
        const action = actions.find(action => action.id === data.id);
        if (action) {
          debug`Performing ${data.id}`;
          action.perform.call(this, aEvent);
        } else {
          warn`Invalid action ${data.id}`;
        }
      };
      this.sendAsyncMessage("ShowSelectionAction", msg);
    } else if (
      [
        "invisibleselection",
        "presscaret",
        "scroll",
        "visibilitychange",
      ].includes(reason)
    ) {
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

      this.sendAsyncMessage("HideSelectionAction", { reason });
    } else if (reason == "dragcaret") {
      // nothing for selection action
    } else {
      warn`Unknown reason: ${reason}`;
    }
  }

  observe(aSubject, aTopic, aData) {
    if (aTopic != "nsPref:changed") {
      return;
    }

    switch (aData) {
      case ACCESSIBLECARET_HEIGHT_PREF:
        this._accessiblecaretHeight = parseFloat(
          Services.prefs.getCharPref(ACCESSIBLECARET_HEIGHT_PREF, "0")
        );
        break;
      case MAGNIFIER_PREF:
        this._magnifierEnabled = Services.prefs.getBoolPref(MAGNIFIER_PREF);
        break;
    }
    // Reset magnifier
    this.eventDispatcher.sendRequest({
      type: "GeckoView:HideMagnifier",
    });
  }
}

const { debug, warn } = SelectionActionDelegateChild.initLogging(
  "SelectionActionDelegate"
);
