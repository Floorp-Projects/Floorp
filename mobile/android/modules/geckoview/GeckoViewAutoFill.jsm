// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewAutoFill"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {GeckoViewUtils} = ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.jsm",
  LoginManagerContent: "resource://gre/modules/LoginManagerContent.jsm",
});

const {debug, warn} = GeckoViewUtils.initLogging("AutoFill"); // eslint-disable-line no-unused-vars

class GeckoViewAutoFill {
  constructor(aEventDispatcher) {
    this._eventDispatcher = aEventDispatcher;
    this._autoFillId = 0;
    this._autoFillElements = undefined;
    this._autoFillInfos = undefined;
    this._autoFillTasks = undefined;
  }

  /**
   * Process an auto-fillable form and send the relevant details of the form
   * to Java. Multiple calls within a short time period for the same form are
   * coalesced, so that, e.g., if multiple inputs are added to a form in
   * succession, we will only perform one processing pass. Note that for inputs
   * without forms, FormLikeFactory treats the document as the "form", but
   * there is no difference in how we process them.
   *
   * @param aFormLike A FormLike object produced by FormLikeFactory.
   */
  addElement(aFormLike) {
    this._addElement(aFormLike, /* fromDeferredTask */ false);
  }

  _addElement(aFormLike, aFromDeferredTask) {
    let task = this._autoFillTasks &&
               this._autoFillTasks.get(aFormLike.rootElement);
    if (task && !aFromDeferredTask) {
      // We already have a pending task; cancel that and start a new one.
      debug `Canceling previous auto-fill task`;
      task.disarm();
      task = null;
    }

    if (!task) {
      if (aFromDeferredTask) {
        // Canceled before we could run the task.
        debug `Auto-fill task canceled`;
        return;
      }
      // Start a new task so we can coalesce adding elements in one batch.
      debug `Deferring auto-fill task`;
      task = new DeferredTask(
        () => this._addElement(aFormLike, true), 100);
      task.arm();
      if (!this._autoFillTasks) {
        this._autoFillTasks = new WeakMap();
      }
      this._autoFillTasks.set(aFormLike.rootElement, task);
      return;
    }

    debug `Adding auto-fill ${aFormLike.rootElement.tagName}`;

    this._autoFillTasks.delete(aFormLike.rootElement);

    if (!this._autoFillInfos) {
      this._autoFillInfos = new WeakMap();
      this._autoFillElements = new Map();
    }

    let sendFocusEvent = false;
    const window = aFormLike.rootElement.ownerGlobal;
    const getInfo = (element, parent, root, usernameField) => {
      let info = this._autoFillInfos.get(element);
      if (info) {
        return info;
      }
      const bounds = element.getBoundingClientRect();
      info = {
        id: ++this._autoFillId,
        parent,
        root,
        tag: element.tagName,
        type: element instanceof window.HTMLInputElement ? element.type : null,
        editable: (element instanceof window.HTMLInputElement) &&
                  ["color", "date", "datetime-local", "email", "month",
                   "number", "password", "range", "search", "tel", "text",
                   "time", "url", "week"].includes(element.type),
        disabled: element instanceof window.HTMLInputElement
          ? element.disabled
          : null,
        attributes: Object.assign({}, ...Array.from(element.attributes)
            .filter(attr => attr.localName !== "value")
            .map(attr => ({[attr.localName]: attr.value}))),
        origin: element.ownerDocument.location.origin,
        autofillhint: "",
        bounds: {
          left: bounds.left,
          top: bounds.top,
          right: bounds.right,
          bottom: bounds.bottom,
        },
      };

      if (element === usernameField) {
        info.autofillhint = "username"; // AUTOFILL_HINT_USERNAME
      }

      this._autoFillInfos.set(element, info);
      this._autoFillElements.set(info.id, Cu.getWeakReference(element));
      sendFocusEvent |= (element === element.ownerDocument.activeElement);
      return info;
    };

    // Get password field to get better form data via LoginManagerContent.
    let passwordField;
    for (const field of aFormLike.elements) {
      if (ChromeUtils.getClassName(field) === "HTMLInputElement" &&
          field.type == "password") {
        passwordField = field;
        break;
      }
    }

    const [usernameField] =
      LoginManagerContent.getUserNameAndPasswordFields(
        passwordField || aFormLike.elements[0]);

    const rootInfo = getInfo(aFormLike.rootElement, null, undefined, null);
    rootInfo.root = rootInfo.id;
    rootInfo.children = aFormLike.elements
        .filter(element => (!usernameField || element.type != "text" ||
                            element == usernameField))
        .map(element => getInfo(element, rootInfo.id, rootInfo.id, usernameField));

    this._eventDispatcher.dispatch("GeckoView:AddAutoFill", rootInfo, {
      onSuccess: responses => {
        // `responses` is an object with IDs as keys.
        debug `Performing auto-fill ${Object.keys(responses)}`;

        const AUTOFILL_STATE = "-moz-autofill";
        const winUtils = window.windowUtils;

        for (const id in responses) {
          const entry = this._autoFillElements &&
                        this._autoFillElements.get(+id);
          const element = entry && entry.get();
          const value = responses[id] || "";

          if (element instanceof window.HTMLInputElement &&
              !element.disabled && element.parentElement) {
            element.setUserInput(value);
            if (winUtils && element.value === value) {
              // Add highlighting for autofilled fields.
              winUtils.addManuallyManagedState(element, AUTOFILL_STATE);

              // Remove highlighting when the field is changed.
              element.addEventListener(
                "input",
                _ => winUtils.removeManuallyManagedState(element,
                                                         AUTOFILL_STATE),
                { mozSystemGroup: true, once: true });
            }
          } else if (element) {
            warn `Don't know how to auto-fill ${element.tagName}`;
          }
        }
      },
      onError: error => {
        warn `Cannot perform autofill ${error}`;
      },
    });

    if (sendFocusEvent) {
      // We might have missed sending a focus event for the active element.
      this.onFocus(aFormLike.ownerDocument.activeElement);
    }
  }

  /**
   * Called when an auto-fillable field is focused or blurred.
   *
   * @param aTarget Focused element, or null if an element has lost focus.
   */
  onFocus(aTarget) {
    debug `Auto-fill focus on ${aTarget && aTarget.tagName}`;

    const info = aTarget && this._autoFillInfos &&
                 this._autoFillInfos.get(aTarget);
    if (!aTarget || info) {
      this._eventDispatcher.dispatch("GeckoView:OnAutoFillFocus", info);
    }
  }

  /**
   * Clear all tracked auto-fill forms and notify Java.
   */
  clearElements() {
    debug `Clearing auto-fill`;

    this._autoFillTasks = undefined;
    this._autoFillInfos = undefined;
    this._autoFillElements = undefined;

    this._eventDispatcher.sendRequest({
      type: "GeckoView:ClearAutoFill",
    });
  }

  /**
   * Scan for auto-fillable forms and add them if necessary. Called when a page
   * is navigated to through history, in which case we don't get our typical
   * "input added" notifications.
   *
   * @param aDoc Document to scan.
   */
  scanDocument(aDoc) {
    // Add forms first; only check forms with password inputs.
    const inputs = aDoc.querySelectorAll("input[type=password]");
    let inputAdded = false;
    for (let i = 0; i < inputs.length; i++) {
      if (inputs[i].form) {
        // Let _addAutoFillElement coalesce multiple calls for the same form.
        this._addElement(FormLikeFactory.createFromForm(inputs[i].form));
      } else if (!inputAdded) {
        // Treat inputs without forms as one unit, and process them only once.
        inputAdded = true;
        this._addElement(FormLikeFactory.createFromField(inputs[i]));
      }
    }

    // Finally add frames.
    const frames = aDoc.defaultView.frames;
    for (let i = 0; i < frames.length; i++) {
      this.scanDocument(frames[i].document);
    }
  }
}
