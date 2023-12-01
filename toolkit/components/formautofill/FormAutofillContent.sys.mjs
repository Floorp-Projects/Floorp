/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Form Autofill content process module.
 */

/* eslint-disable no-use-before-define */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
  FormAutofillUtils: "resource://gre/modules/shared/FormAutofillUtils.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  FormStateManager: "resource://gre/modules/shared/FormStateManager.sys.mjs",
  ProfileAutocomplete:
    "resource://autofill/AutofillProfileAutoComplete.sys.mjs",
  AutofillTelemetry: "resource://autofill/AutofillTelemetry.sys.mjs",
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "DELEGATE_AUTOCOMPLETE",
  "toolkit.autocomplete.delegate",
  false
);

const formFillController = Cc[
  "@mozilla.org/satchel/form-fill-controller;1"
].getService(Ci.nsIFormFillController);

function getActorFromWindow(contentWindow, name = "FormAutofill") {
  // In unit tests, contentWindow isn't a real window.
  if (!contentWindow) {
    return null;
  }

  return contentWindow.windowGlobalChild
    ? contentWindow.windowGlobalChild.getActor(name)
    : null;
}

/**
 * Handles content's interactions for the process.
 *
 */
export var FormAutofillContent = {
  /**
   * @type {Set} Set of the fields with usable values in any saved profile.
   */
  get savedFieldNames() {
    return Services.cpmm.sharedData.get("FormAutofill:savedFieldNames");
  },

  /**
   * @type {boolean} Flag indicating whether a focus action requiring
   * the popup to be active is pending.
   */
  _popupPending: false,

  /**
   * @type {boolean} Flag indicating whether the form is waiting to be
   * filled by Autofill.
   */
  _autofillPending: false,

  init() {
    this.log = lazy.FormAutofill.defineLogGetter(this, "FormAutofillContent");
    this.debug("init");

    // eslint-disable-next-line mozilla/balanced-listeners
    Services.cpmm.sharedData.addEventListener("change", this);

    let autofillEnabled = Services.cpmm.sharedData.get("FormAutofill:enabled");
    // If storage hasn't be initialized yet autofillEnabled is undefined but we need to ensure
    // autocomplete is registered before the focusin so register it in this case as long as the
    // pref is true.
    let shouldEnableAutofill =
      autofillEnabled === undefined &&
      (lazy.FormAutofill.isAutofillAddressesEnabled ||
        lazy.FormAutofill.isAutofillCreditCardsEnabled);
    if (autofillEnabled || shouldEnableAutofill) {
      lazy.ProfileAutocomplete.ensureRegistered();
    }

    /**
     * @type {FormAutofillFieldDetailsManager} handling state management of current forms and handlers.
     */
    this._fieldDetailsManager = new lazy.FormStateManager(
      this.formSubmitted.bind(this),
      this._showPopup.bind(this)
    );
  },

  get activeFieldDetail() {
    return this._fieldDetailsManager.activeFieldDetail;
  },

  get activeFormDetails() {
    return this._fieldDetailsManager.activeFormDetails;
  },

  get activeInput() {
    return this._fieldDetailsManager.activeInput;
  },

  get activeHandler() {
    return this._fieldDetailsManager.activeHandler;
  },

  get activeSection() {
    return this._fieldDetailsManager.activeSection;
  },

  /**
   * Send the profile to parent for doorhanger and storage saving/updating.
   *
   * @param {object} profile Submitted form's address/creditcard guid and record.
   * @param {object} domWin Current content window.
   */
  _onFormSubmit(profile, domWin) {
    let actor = getActorFromWindow(domWin);
    actor.sendAsyncMessage("FormAutofill:OnFormSubmit", profile);
  },

  /**
   * Handle a form submission and early return when:
   * 1. In private browsing mode.
   * 2. Could not map any autofill handler by form element.
   * 3. Number of filled fields is less than autofill threshold
   *
   * @param {HTMLElement} formElement Root element which receives submit event.
   * @param {Window} domWin Content window; passed for unit tests and when
   *                 invoked by the FormAutofillSection
   * @param {object} handler FormAutofillHander, if known by caller
   */
  formSubmitted(
    formElement,
    domWin = formElement.ownerGlobal,
    handler = undefined
  ) {
    this.debug("Handling form submission");

    if (!lazy.FormAutofill.isAutofillEnabled) {
      this.debug("Form Autofill is disabled");
      return;
    }

    // The `domWin` truthiness test is used by unit tests to bypass this check.
    if (domWin && lazy.PrivateBrowsingUtils.isContentWindowPrivate(domWin)) {
      this.debug("Ignoring submission in a private window");
      return;
    }

    handler = handler || this._fieldDetailsManager._getFormHandler(formElement);
    const records = this._fieldDetailsManager.getRecords(formElement, handler);

    if (!records || !handler) {
      this.debug("Form element could not map to an existing handler");
      return;
    }

    [records.address, records.creditCard].forEach((rs, idx) => {
      lazy.AutofillTelemetry.recordSubmittedSectionCount(
        idx == 0
          ? lazy.AutofillTelemetry.ADDRESS
          : lazy.AutofillTelemetry.CREDIT_CARD,
        rs?.length
      );

      rs?.forEach(r => {
        lazy.AutofillTelemetry.recordFormInteractionEvent(
          "submitted",
          r.section,
          {
            record: r,
            form: handler.form,
          }
        );
        delete r.section;
      });
    });

    this._onFormSubmit(records, domWin);
  },

  _showPopup() {
    formFillController.showPopup();
  },

  handleEvent(evt) {
    switch (evt.type) {
      case "change": {
        if (!evt.changedKeys.includes("FormAutofill:enabled")) {
          return;
        }
        if (Services.cpmm.sharedData.get("FormAutofill:enabled")) {
          lazy.ProfileAutocomplete.ensureRegistered();
          if (this._popupPending) {
            this._popupPending = false;
            this.debug("handleEvent: Opening deferred popup");
            this._showPopup();
          }
        } else {
          lazy.ProfileAutocomplete.ensureUnregistered();
        }
        break;
      }
    }
  },

  /**
   * All active items should be updated according the active element of
   * `formFillController.focusedInput`. All of them including element,
   * handler, section, and field detail, can be retrieved by their own getters.
   *
   * @param {HTMLElement|null} element The active item should be updated based
   * on this or `formFillController.focusedInput` will be taken.
   */
  updateActiveInput(element) {
    element = element || formFillController.focusedInput;
    if (!element) {
      this.debug("updateActiveElement: no element selected");
      return;
    }
    this._fieldDetailsManager.updateActiveInput(element);
    this.debug("updateActiveElement: checking for popup-on-focus");
    // We know this element just received focus. If it's a credit card field,
    // open its popup.
    if (this._autofillPending) {
      this.debug("updateActiveElement: skipping check; autofill is imminent");
    } else if (element.value?.length !== 0) {
      this.debug(
        `updateActiveElement: Not opening popup because field is not empty.`
      );
    } else {
      this.debug(
        "updateActiveElement: checking if empty field is cc-*: ",
        this.activeFieldDetail?.fieldName
      );

      if (
        this.activeFieldDetail?.fieldName?.startsWith("cc-") ||
        AppConstants.platform === "android"
      ) {
        if (Services.cpmm.sharedData.get("FormAutofill:enabled")) {
          this.debug("updateActiveElement: opening pop up");
          this._showPopup();
        } else {
          this.debug(
            "updateActiveElement: Deferring pop-up until Autofill is ready"
          );
          this._popupPending = true;
        }
      }
    }
  },

  set autofillPending(flag) {
    this.debug("Setting autofillPending to", flag);
    this._autofillPending = flag;
  },

  identifyAutofillFields(element) {
    this.debug(
      `identifyAutofillFields: ${element.ownerDocument.location?.hostname}`
    );

    if (lazy.DELEGATE_AUTOCOMPLETE || !this.savedFieldNames) {
      this.debug("identifyAutofillFields: savedFieldNames are not known yet");
      let actor = getActorFromWindow(element.ownerGlobal);
      if (actor) {
        actor.sendAsyncMessage("FormAutofill:InitStorage");
      }
    }

    const validDetails =
      this._fieldDetailsManager.identifyAutofillFields(element);

    validDetails?.forEach(detail => this._markAsAutofillField(detail.element));
  },

  clearForm() {
    let focusedInput =
      this.activeInput ||
      lazy.ProfileAutocomplete._lastAutoCompleteFocusedInput;
    if (!focusedInput) {
      return;
    }

    this.activeSection.clearPopulatedForm();

    let fieldName = FormAutofillContent.activeFieldDetail?.fieldName;
    if (lazy.FormAutofillUtils.isCreditCardField(fieldName)) {
      lazy.AutofillTelemetry.recordFormInteractionEvent(
        "cleared",
        this.activeSection,
        { fieldName }
      );
    }
  },

  previewProfile(doc) {
    let docWin = doc.ownerGlobal;
    let selectedIndex = lazy.ProfileAutocomplete._getSelectedIndex(docWin);
    let lastAutoCompleteResult =
      lazy.ProfileAutocomplete.lastProfileAutoCompleteResult;
    let focusedInput = this.activeInput;
    let actor = getActorFromWindow(docWin);

    if (
      selectedIndex === -1 ||
      !focusedInput ||
      !lastAutoCompleteResult ||
      lastAutoCompleteResult.getStyleAt(selectedIndex) != "autofill-profile"
    ) {
      actor.sendAsyncMessage("FormAutofill:UpdateWarningMessage", {});

      lazy.ProfileAutocomplete._clearProfilePreview();
    } else {
      let focusedInputDetails = this.activeFieldDetail;
      let profile = JSON.parse(
        lastAutoCompleteResult.getCommentAt(selectedIndex)
      );
      let allFieldNames = FormAutofillContent.activeSection.allFieldNames;
      let profileFields = allFieldNames.filter(
        fieldName => !!profile[fieldName]
      );

      let focusedCategory = lazy.FormAutofillUtils.getCategoryFromFieldName(
        focusedInputDetails.fieldName
      );
      let categories =
        lazy.FormAutofillUtils.getCategoriesFromFieldNames(profileFields);
      actor.sendAsyncMessage("FormAutofill:UpdateWarningMessage", {
        focusedCategory,
        categories,
      });

      lazy.ProfileAutocomplete._previewSelectedProfile(selectedIndex);
    }
  },

  onPopupClosed(selectedRowStyle) {
    this.debug("Popup has closed.");
    lazy.ProfileAutocomplete._clearProfilePreview();

    let lastAutoCompleteResult =
      lazy.ProfileAutocomplete.lastProfileAutoCompleteResult;
    let focusedInput = FormAutofillContent.activeInput;
    if (
      lastAutoCompleteResult &&
      FormAutofillContent._keyDownEnterForInput &&
      focusedInput === FormAutofillContent._keyDownEnterForInput &&
      focusedInput ===
        lazy.ProfileAutocomplete.lastProfileAutoCompleteFocusedInput
    ) {
      if (selectedRowStyle == "autofill-footer") {
        let actor = getActorFromWindow(focusedInput.ownerGlobal);
        actor.sendAsyncMessage("FormAutofill:OpenPreferences");
      } else if (selectedRowStyle == "autofill-clear-button") {
        FormAutofillContent.clearForm();
      }
    }
  },

  onPopupOpened() {
    this.debug(
      "Popup has opened, automatic =",
      formFillController.passwordPopupAutomaticallyOpened
    );

    let fieldName = FormAutofillContent.activeFieldDetail?.fieldName;
    if (fieldName && this.activeSection) {
      lazy.AutofillTelemetry.recordFormInteractionEvent(
        "popup_shown",
        this.activeSection,
        { fieldName }
      );
    }
  },

  _markAsAutofillField(field) {
    // Since Form Autofill popup is only for input element, any non-Input
    // element should be excluded here.
    if (!HTMLInputElement.isInstance(field)) {
      return;
    }

    formFillController.markAsAutofillField(field);
  },

  _onKeyDown(e) {
    delete FormAutofillContent._keyDownEnterForInput;
    let lastAutoCompleteResult =
      lazy.ProfileAutocomplete.lastProfileAutoCompleteResult;
    let focusedInput = FormAutofillContent.activeInput;
    if (
      e.keyCode != e.DOM_VK_RETURN ||
      !lastAutoCompleteResult ||
      !focusedInput ||
      focusedInput !=
        lazy.ProfileAutocomplete.lastProfileAutoCompleteFocusedInput
    ) {
      return;
    }
    FormAutofillContent._keyDownEnterForInput = focusedInput;
  },

  didDestroy() {
    this._fieldDetailsManager.didDestroy();
  },
};

FormAutofillContent.init();
