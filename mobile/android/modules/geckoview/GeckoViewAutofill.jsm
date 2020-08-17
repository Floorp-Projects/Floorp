// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewAutofill"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.jsm",
  LoginManagerChild: "resource://gre/modules/LoginManagerChild.jsm",
});

const { debug, warn } = GeckoViewUtils.initLogging("Autofill");

class GeckoViewAutofill {
  constructor(aEventDispatcher) {
    this._eventDispatcher = aEventDispatcher;
    this._autofillId = 0;
    this._autofillElements = undefined;
    this._autofillInfos = undefined;
    this._autofillTasks = undefined;
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

  _getInfo(aElement, aParent, aRoot, aUsernameField) {
    if (!this._autofillInfos) {
      this._autofillInfos = new WeakMap();
      this._autofillElements = new Map();
    }

    let info = this._autofillInfos.get(aElement);
    if (info) {
      return info;
    }

    const window = aElement.ownerGlobal;
    const bounds = aElement.getBoundingClientRect();

    info = {
      id: ++this._autofillId,
      parent: aParent,
      root: aRoot,
      tag: aElement.tagName,
      type: aElement instanceof window.HTMLInputElement ? aElement.type : null,
      value: aElement.value,
      editable:
        aElement instanceof window.HTMLInputElement &&
        [
          "color",
          "date",
          "datetime-local",
          "email",
          "month",
          "number",
          "password",
          "range",
          "search",
          "tel",
          "text",
          "time",
          "url",
          "week",
        ].includes(aElement.type),
      disabled:
        aElement instanceof window.HTMLInputElement ? aElement.disabled : null,
      attributes: Object.assign(
        {},
        ...Array.from(aElement.attributes)
          .filter(attr => attr.localName !== "value")
          .map(attr => ({ [attr.localName]: attr.value }))
      ),
      origin: aElement.ownerDocument.location.origin,
      autofillhint: "",
      bounds: {
        left: bounds.left,
        top: bounds.top,
        right: bounds.right,
        bottom: bounds.bottom,
      },
    };

    if (aElement === aUsernameField) {
      info.autofillhint = "username"; // AUTOFILL.HINT.USERNAME
    } else if (aElement instanceof window.HTMLInputElement) {
      // Using autocomplete attribute if it is email.
      const autocompleteInfo = aElement.getAutocompleteInfo();
      if (autocompleteInfo) {
        const autocompleteAttr = autocompleteInfo.fieldName;
        if (autocompleteAttr == "email") {
          info.type = "email";
        }
      }
    }

    this._autofillInfos.set(aElement, info);
    this._autofillElements.set(info.id, Cu.getWeakReference(aElement));
    return info;
  }

  _updateInfoValues(aElements) {
    if (!this._autofillInfos) {
      return [];
    }

    const updated = [];
    for (const element of aElements) {
      const info = this._autofillInfos.get(element);
      if (!info || info.value === element.value) {
        continue;
      }
      debug`Updating value ${info.value} to ${element.value}`;

      info.value = element.value;
      this._autofillInfos.set(element, info);
      updated.push(info);
    }
    return updated;
  }

  _addElement(aFormLike, aFromDeferredTask) {
    let task =
      this._autofillTasks && this._autofillTasks.get(aFormLike.rootElement);
    if (task && !aFromDeferredTask) {
      // We already have a pending task; cancel that and start a new one.
      debug`Canceling previous auto-fill task`;
      task.disarm();
      task = null;
    }

    if (!task) {
      if (aFromDeferredTask) {
        // Canceled before we could run the task.
        debug`Auto-fill task canceled`;
        return;
      }
      // Start a new task so we can coalesce adding elements in one batch.
      debug`Deferring auto-fill task`;
      task = new DeferredTask(() => this._addElement(aFormLike, true), 100);
      task.arm();
      if (!this._autofillTasks) {
        this._autofillTasks = new WeakMap();
      }
      this._autofillTasks.set(aFormLike.rootElement, task);
      return;
    }

    debug`Adding auto-fill ${aFormLike.rootElement.tagName}`;

    this._autofillTasks.delete(aFormLike.rootElement);

    const window = aFormLike.rootElement.ownerGlobal;
    // Get password field to get better form data via LoginManagerChild.
    let passwordField;
    for (const field of aFormLike.elements) {
      if (
        ChromeUtils.getClassName(field) === "HTMLInputElement" &&
        field.type == "password"
      ) {
        passwordField = field;
        break;
      }
    }

    const [usernameField] = LoginManagerChild.forWindow(
      window
    ).getUserNameAndPasswordFields(passwordField || aFormLike.elements[0]);

    const focusedElement = aFormLike.rootElement.ownerDocument.activeElement;
    let sendFocusEvent = aFormLike.rootElement === focusedElement;

    const rootInfo = this._getInfo(
      aFormLike.rootElement,
      null,
      undefined,
      null
    );

    rootInfo.root = rootInfo.id;
    rootInfo.children = aFormLike.elements
      .filter(
        element =>
          element.type != "hidden" &&
          (!usernameField ||
            element.type != "text" ||
            element == usernameField ||
            (element.getAutocompleteInfo() &&
              element.getAutocompleteInfo().fieldName == "email"))
      )
      .map(element => {
        sendFocusEvent |= element === focusedElement;
        return this._getInfo(element, rootInfo.id, rootInfo.id, usernameField);
      });

    this._eventDispatcher.dispatch("GeckoView:AddAutofill", rootInfo, {
      onSuccess: responses => {
        // `responses` is an object with IDs as keys.
        debug`Performing auto-fill ${Object.keys(responses)}`;

        const AUTOFILL_STATE = "-moz-autofill";
        const winUtils = window.windowUtils;

        for (const id in responses) {
          const entry =
            this._autofillElements && this._autofillElements.get(+id);
          const element = entry && entry.get();
          const value = responses[id] || "";

          if (
            element instanceof window.HTMLInputElement &&
            !element.disabled &&
            element.parentElement
          ) {
            element.setUserInput(value);
            if (winUtils && element.value === value) {
              // Add highlighting for autofilled fields.
              winUtils.addManuallyManagedState(element, AUTOFILL_STATE);

              // Remove highlighting when the field is changed.
              element.addEventListener(
                "input",
                _ =>
                  winUtils.removeManuallyManagedState(element, AUTOFILL_STATE),
                { mozSystemGroup: true, once: true }
              );
            }
          } else if (element) {
            warn`Don't know how to auto-fill ${element.tagName}`;
          }
        }
      },
      onError: error => {
        warn`Cannot perform autofill ${error}`;
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
    debug`Auto-fill focus on ${aTarget && aTarget.tagName}`;

    const info =
      aTarget && this._autofillInfos && this._autofillInfos.get(aTarget);
    if (!aTarget || info) {
      this._eventDispatcher.dispatch("GeckoView:OnAutofillFocus", info);
    }
  }

  commitAutofill(aFormLike) {
    if (!aFormLike) {
      throw new Error("null-form on autofill commit");
    }

    debug`Committing auto-fill for ${aFormLike.rootElement.tagName}`;

    const updatedNodeInfos = this._updateInfoValues([
      aFormLike.rootElement,
      ...aFormLike.elements,
    ]);

    for (const updatedInfo of updatedNodeInfos) {
      debug`Updating node ${updatedInfo}`;
      this._eventDispatcher.dispatch("GeckoView:UpdateAutofill", updatedInfo);
    }

    const info = this._getInfo(aFormLike.rootElement);
    if (info) {
      debug`Committing node ${info}`;
      this._eventDispatcher.dispatch("GeckoView:CommitAutofill", info);
    }
  }

  /**
   * Clear all tracked auto-fill forms and notify Java.
   */
  clearElements() {
    debug`Clearing auto-fill`;

    this._autofillTasks = undefined;
    this._autofillInfos = undefined;
    this._autofillElements = undefined;

    this._eventDispatcher.sendRequest({
      type: "GeckoView:ClearAutofill",
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
        // Let _addElement coalesce multiple calls for the same form.
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
