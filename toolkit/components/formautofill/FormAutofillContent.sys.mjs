/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Form Autofill content process module.
 */

/* eslint-disable no-use-before-define */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  FormAutofill: "resource://autofill/FormAutofill.sys.mjs",
  ProfileAutocomplete:
    "resource://autofill/AutofillProfileAutoComplete.sys.mjs",
});

const formFillController = Cc[
  "@mozilla.org/satchel/form-fill-controller;1"
].getService(Ci.nsIFormFillController);

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

  init() {
    this.log = lazy.FormAutofill.defineLogGetter(this, "FormAutofillContent");
    this.debug("init");

    // eslint-disable-next-line mozilla/balanced-listeners
    Services.cpmm.sharedData.addEventListener("change", this);

    const autofillEnabled = Services.cpmm.sharedData.get(
      "FormAutofill:enabled"
    );
    // If storage hasn't be initialized yet autofillEnabled is undefined but we need to ensure
    // autocomplete is registered before the focusin so register it in this case as long as the
    // pref is true.
    const shouldEnableAutofill =
      autofillEnabled === undefined &&
      (lazy.FormAutofill.isAutofillAddressesEnabled ||
        lazy.FormAutofill.isAutofillCreditCardsEnabled);
    if (autofillEnabled || shouldEnableAutofill) {
      lazy.ProfileAutocomplete.ensureRegistered();
    }

    this.activeAutofillChild = null;
  },

  get activeFieldDetail() {
    return this.activeAutofillChild?.activeFieldDetail;
  },

  get activeFormDetails() {
    return this.activeAutofillChild?.activeFormDetails;
  },

  get activeInput() {
    return this.activeAutofillChild?.activeInput;
  },

  get activeHandler() {
    return this.activeAutofillChild?.activeHandler;
  },

  get activeSection() {
    return this.activeAutofillChild?.activeSection;
  },

  set autofillPending(flag) {
    if (this.activeAutofillChild) {
      this.activeAutofillChild.autofillPending = flag;
    }
  },

  updateActiveAutofillChild(autofillChild) {
    this.activeAutofillChild = autofillChild;
  },

  showPopup() {
    if (Services.cpmm.sharedData.get("FormAutofill:enabled")) {
      this.debug("updateActiveElement: opening pop up");
      formFillController.showPopup();
    } else {
      this.debug(
        "updateActiveElement: Deferring pop-up until Autofill is ready"
      );
      this._popupPending = true;
    }
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
            formFillController.showPopup();
          }
        } else {
          lazy.ProfileAutocomplete.ensureUnregistered();
        }
        break;
      }
    }
  },
};

FormAutofillContent.init();
