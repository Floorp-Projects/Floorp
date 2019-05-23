/* -*- mode: js; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["KeyPressEventModelCheckerChild"];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
const {AppConstants} = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

class KeyPressEventModelCheckerChild extends ActorChild {
  // Currently, the event is dispatched only when the document becomes editable
  // because of contenteditable.  If you need to add new editor which is in
  // designMode, you need to change MaybeDispatchCheckKeyPressEventModelEvent()
  // of nsHTMLDocument.
  handleEvent(aEvent) {
    if (!AppConstants.DEBUG) {
      // Stop propagation in opt build to save the propagation cost.
      // However, the event is necessary for running test_bug1514940.html.
      // Therefore, we need to keep propagating it at least on debug build.
      aEvent.stopImmediatePropagation();
    }

    // Currently, even if we set HTMLDocument.KEYPRESS_EVENT_MODEL_CONFLATED
    // here, conflated model isn't used forcibly.  If you need it, you need
    // to change WidgetKeyboardEvent, dom::KeyboardEvent and PresShell.
    let model = HTMLDocument.KEYPRESS_EVENT_MODEL_DEFAULT;
    if (this._isOldOfficeOnlineServer(aEvent.target) ||
        this._isOldConfluence(aEvent.target.ownerGlobal)) {
      model = HTMLDocument.KEYPRESS_EVENT_MODEL_SPLIT;
    }
    aEvent.target.setKeyPressEventModel(model);
  }

  _isOldOfficeOnlineServer(aDocument) {
    let editingElement =
        aDocument.getElementById("WACViewPanel_EditingElement");
    if (!editingElement) {
      return false;
    }
    let isOldVersion =
        !editingElement.classList.contains(
            "WACViewPanel_DisableLegacyKeyCodeAndCharCode");
    return isOldVersion;
  }

  _isOldConfluence(aWindow) {
    if (!aWindow) {
      return false;
    }
    // aWindow should be an editor window in <iframe>.  However, we don't know
    // whether it can be without <iframe>.  Anyway, there should be tinyMCE
    // object in the parent window or in the window.
    let tinyMCEObject;
    // First, try to retrieve tinyMCE object from parent window.
    try {
      tinyMCEObject = ChromeUtils.waiveXrays(aWindow.parent).tinyMCE;
    } catch (e) {
      // Ignore the exception for now.
    }
    // Next, if there is no tinyMCE object in the parent window, let's check
    // the window.
    if (!tinyMCEObject) {
      try {
        tinyMCEObject = ChromeUtils.waiveXrays(aWindow).tinyMCE;
      } catch (e) {
        // Fallthrough to return false below.
      }
      // If we couldn't find tinyMCE object, let's assume that it's not
      // Confluence instance.
      if (!tinyMCEObject) {
        return false;
      }
    }
    // If there is tinyMCE object, we can assume that we loaded Confluence
    // instance.  So, let's check the version whether it allows conflated
    // keypress event model.
    try {
      let {author, version} =
          new tinyMCEObject.plugins.CursorTargetPlugin().getInfo();
      // If it's not Confluence, don't include it into the telemetry because
      // we just need to collect percentage of old version in all loaded
      // Confluence instances.
      if (author !== "Atlassian") {
        return false;
      }
      let isOldVersion = version === "1.0";
      Services.telemetry.keyedScalarAdd("dom.event.confluence_load_count",
                                        isOldVersion ? "old" : "new", 1);
      return isOldVersion;
    } catch (e) {
      return false;
    }
  }
}
