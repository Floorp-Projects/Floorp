/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { GeckoViewActorChild } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewActorChild.jsm"
);
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  FormLikeFactory: "resource://gre/modules/FormLikeFactory.jsm",
  GeckoViewAutofill: "resource://gre/modules/GeckoViewAutofill.jsm",
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.jsm",
  LoginManagerChild: "resource://gre/modules/LoginManagerChild.jsm",
});

const EXPORTED_SYMBOLS = ["GeckoViewAutoFillChild"];

class GeckoViewAutoFillChild extends GeckoViewActorChild {
  constructor() {
    super();

    this._autofillElements = undefined;
    this._autofillInfos = undefined;
  }

  // eslint-disable-next-line complexity
  handleEvent(aEvent) {
    debug`handleEvent: ${aEvent.type}`;
    switch (aEvent.type) {
      case "DOMFormHasPassword": {
        this.addElement(
          lazy.FormLikeFactory.createFromForm(aEvent.composedTarget)
        );
        break;
      }
      case "DOMInputPasswordAdded": {
        const input = aEvent.composedTarget;
        if (!input.form) {
          this.addElement(lazy.FormLikeFactory.createFromField(input));
        }
        break;
      }
      case "focusin": {
        if (
          this.contentWindow.HTMLInputElement.isInstance(aEvent.composedTarget)
        ) {
          this.onFocus(aEvent.composedTarget);
        }
        break;
      }
      case "focusout": {
        if (
          this.contentWindow.HTMLInputElement.isInstance(aEvent.composedTarget)
        ) {
          this.onFocus(null);
        }
        break;
      }
      case "pagehide": {
        if (aEvent.target === this.document) {
          this.clearElements(this.browsingContext);
        }
        break;
      }
      case "pageshow": {
        if (aEvent.target === this.document) {
          this.scanDocument(this.document);
        }
        break;
      }
      case "PasswordManager:ShowDoorhanger": {
        const { form: formLike } = aEvent.detail;
        this.commitAutofill(formLike);
        break;
      }
    }
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
  async addElement(aFormLike) {
    debug`Adding auto-fill ${aFormLike.rootElement.tagName}`;

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

    const loginManagerChild = lazy.LoginManagerChild.forWindow(window);
    const docState = loginManagerChild.stateForDocument(
      passwordField.ownerDocument
    );
    const [usernameField] = docState.getUserNameAndPasswordFields(
      passwordField || aFormLike.elements[0]
    );

    const focusedElement = aFormLike.rootElement.ownerDocument.activeElement;
    let sendFocusEvent = aFormLike.rootElement === focusedElement;

    const rootInfo = this._getInfo(
      aFormLike.rootElement,
      null,
      undefined,
      null
    );

    rootInfo.rootUuid = rootInfo.uuid;
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
        return this._getInfo(
          element,
          rootInfo.uuid,
          rootInfo.uuid,
          usernameField
        );
      });

    try {
      // We don't await here so that we can send a focus event immediately
      // after this as the app might not know which element is focused.
      const responsePromise = this.sendQuery("Add", {
        node: rootInfo,
      });

      if (sendFocusEvent) {
        // We might have missed sending a focus event for the active element.
        this.onFocus(aFormLike.ownerDocument.activeElement);
      }

      const responses = await responsePromise;
      // `responses` is an object with global IDs as keys.
      debug`Performing auto-fill ${Object.keys(responses)}`;

      const AUTOFILL_STATE = "autofill";
      const winUtils = window.windowUtils;

      for (const uuid in responses) {
        const entry =
          this._autofillElements && this._autofillElements.get(uuid);
        const element = entry && entry.get();
        const value = responses[uuid] || "";

        if (
          window.HTMLInputElement.isInstance(element) &&
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
              _ => winUtils.removeManuallyManagedState(element, AUTOFILL_STATE),
              { mozSystemGroup: true, once: true }
            );
          }
        } else if (element) {
          warn`Don't know how to auto-fill ${element.tagName}`;
        }
      }
    } catch (error) {
      warn`Cannot perform autofill ${error}`;
    }
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
    const isInputElement = window.HTMLInputElement.isInstance(aElement);

    info = {
      isInputElement,
      uuid: Services.uuid
        .generateUUID()
        .toString()
        .slice(1, -1), // discard the surrounding curly braces
      parentUuid: aParent,
      rootUuid: aRoot,
      tag: aElement.tagName,
      type: isInputElement ? aElement.type : null,
      value: isInputElement ? aElement.value : null,
      editable:
        isInputElement &&
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
      disabled: isInputElement ? aElement.disabled : null,
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
    } else if (isInputElement) {
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
    this._autofillElements.set(info.uuid, Cu.getWeakReference(aElement));
    return info;
  }

  _updateInfoValues(aElements) {
    if (!this._autofillInfos) {
      return [];
    }

    const updated = [];
    for (const element of aElements) {
      const info = this._autofillInfos.get(element);

      if (!info?.isInputElement || info.value === element.value) {
        continue;
      }
      debug`Updating value ${info.value} to ${element.value}`;

      info.value = element.value;
      this._autofillInfos.set(element, info);
      updated.push(info);
    }
    return updated;
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
      this.sendAsyncMessage("Focus", {
        node: info,
      });
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
      this.sendAsyncMessage("Update", {
        node: updatedInfo,
      });
    }

    const info = this._getInfo(aFormLike.rootElement);
    if (info) {
      debug`Committing node ${info}`;
      this.sendAsyncMessage("Commit", {
        node: info,
      });
    }
  }

  /**
   * Clear all tracked auto-fill forms and notify Java.
   */
  clearElements(browsingContext) {
    this._autofillInfos = undefined;
    this._autofillElements = undefined;

    if (browsingContext === browsingContext.top) {
      this.sendAsyncMessage("Clear");
    }
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
        // Let addElement coalesce multiple calls for the same form.
        this.addElement(lazy.FormLikeFactory.createFromForm(inputs[i].form));
      } else if (!inputAdded) {
        // Treat inputs without forms as one unit, and process them only once.
        inputAdded = true;
        this.addElement(lazy.FormLikeFactory.createFromField(inputs[i]));
      }
    }
  }
}

const { debug, warn } = GeckoViewAutoFillChild.initLogging("GeckoViewAutoFill");
