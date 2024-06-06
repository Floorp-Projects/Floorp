/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Injects the form autofill section into about:preferences.
 */

// Add addresses enabled flag in telemetry environment for recording the number of
// users who disable/enable the address autofill feature.
const BUNDLE_URI = "chrome://formautofill/locale/formautofill.properties";
const MANAGE_ADDRESSES_URL =
  "chrome://formautofill/content/manageAddresses.xhtml";
const MANAGE_CREDITCARDS_URL =
  "chrome://formautofill/content/manageCreditCards.xhtml";

import { FormAutofill } from "resource://autofill/FormAutofill.sys.mjs";
import { FormAutofillUtils } from "resource://gre/modules/shared/FormAutofillUtils.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  OSKeyStore: "resource://gre/modules/OSKeyStore.sys.mjs",
});
ChromeUtils.defineLazyGetter(
  lazy,
  "l10n",
  () =>
    new Localization(
      ["branding/brand.ftl", "browser/preferences/preferences.ftl"],
      true
    )
);

const {
  ENABLED_AUTOFILL_ADDRESSES_PREF,
  ENABLED_AUTOFILL_CREDITCARDS_PREF,
  AUTOFILL_CREDITCARDS_REAUTH_PREF,
} = FormAutofill;
const {
  MANAGE_ADDRESSES_L10N_IDS,
  EDIT_ADDRESS_L10N_IDS,
  MANAGE_CREDITCARDS_L10N_IDS,
  EDIT_CREDITCARD_L10N_IDS,
} = FormAutofillUtils;
// Add credit card enabled flag in telemetry environment for recording the number of
// users who disable/enable the credit card autofill feature.

const HTML_NS = "http://www.w3.org/1999/xhtml";

export function FormAutofillPreferences() {
  this.bundle = Services.strings.createBundle(BUNDLE_URI);
}

FormAutofillPreferences.prototype = {
  /**
   * Create the Form Autofill preference group.
   *
   * @param   {HTMLDocument} document
   * @returns {XULElement}
   */
  init(document) {
    this.createPreferenceGroup(document);
    this.attachEventListeners();

    return this.refs.formAutofillFragment;
  },

  /**
   * Remove event listeners and the preference group.
   */
  uninit() {
    this.detachEventListeners();
    this.refs.formAutofillGroup.remove();
  },

  /**
   * Create Form Autofill preference group
   *
   * @param  {HTMLDocument} document
   */
  createPreferenceGroup(document) {
    let formAutofillFragment = document.createDocumentFragment();
    let formAutofillGroupBoxLabel = document.createXULElement("label");
    let formAutofillGroupBoxLabelHeading = document.createElementNS(
      HTML_NS,
      "h2"
    );
    let formAutofillGroup = document.createXULElement("vbox");
    // Wrappers are used to properly compute the search tooltip positions
    // let savedAddressesBtnWrapper = document.createXULElement("hbox");
    // let savedCreditCardsBtnWrapper = document.createXULElement("hbox");
    this.refs = {};
    this.refs.formAutofillGroup = formAutofillGroup;
    this.refs.formAutofillFragment = formAutofillFragment;

    formAutofillGroupBoxLabel.appendChild(formAutofillGroupBoxLabelHeading);
    formAutofillFragment.appendChild(formAutofillGroupBoxLabel);
    formAutofillFragment.appendChild(formAutofillGroup);

    let showAddressUI = FormAutofill.isAutofillAddressesAvailable;
    let showCreditCardUI = FormAutofill.isAutofillCreditCardsAvailable;

    if (!showAddressUI && !showCreditCardUI) {
      return;
    }

    formAutofillGroupBoxLabelHeading.textContent = lazy.l10n.formatValueSync(
      "pane-privacy-autofill-header"
    );

    if (showAddressUI) {
      let savedAddressesBtnWrapper = document.createXULElement("hbox");
      let addressAutofill = document.createXULElement("hbox");
      let addressAutofillCheckboxGroup = document.createXULElement("hbox");
      let addressAutofillCheckbox = document.createXULElement("checkbox");
      let addressAutofillLearnMore = document.createElement("a", {
        is: "moz-support-link",
      });
      let savedAddressesBtn = document.createXULElement("button", {
        is: "highlightable-button",
      });
      savedAddressesBtn.className = "accessory-button";
      addressAutofillCheckbox.className = "tail-with-learn-more";

      formAutofillGroup.id = "formAutofillGroup";
      addressAutofill.id = "addressAutofill";
      addressAutofillLearnMore.id = "addressAutofillLearnMore";

      addressAutofill.setAttribute("data-subcategory", "address-autofill");
      addressAutofillCheckbox.setAttribute(
        "label",
        lazy.l10n.formatValueSync("autofill-addresses-checkbox")
      );
      savedAddressesBtn.setAttribute(
        "label",
        lazy.l10n.formatValueSync("autofill-saved-addresses-button")
      );
      // Align the start to keep the savedAddressesBtn as original size
      // when addressAutofillCheckboxGroup's height is changed by a longer l10n string
      savedAddressesBtnWrapper.setAttribute("align", "start");

      addressAutofillLearnMore.setAttribute(
        "support-page",
        "autofill-card-address"
      );

      // Add preferences search support
      savedAddressesBtn.setAttribute(
        "search-l10n-ids",
        MANAGE_ADDRESSES_L10N_IDS.concat(EDIT_ADDRESS_L10N_IDS).join(",")
      );

      // Manually set the checked state
      if (FormAutofill.isAutofillAddressesEnabled) {
        addressAutofillCheckbox.setAttribute("checked", true);
      }
      if (FormAutofill.isAutofillAddressesLocked) {
        addressAutofillCheckbox.disabled = true;
      }

      addressAutofillCheckboxGroup.setAttribute("align", "center");
      addressAutofillCheckboxGroup.setAttribute("flex", "1");

      formAutofillGroup.appendChild(addressAutofill);
      addressAutofill.appendChild(addressAutofillCheckboxGroup);
      addressAutofillCheckboxGroup.appendChild(addressAutofillCheckbox);
      addressAutofillCheckboxGroup.appendChild(addressAutofillLearnMore);
      addressAutofill.appendChild(savedAddressesBtnWrapper);
      savedAddressesBtnWrapper.appendChild(savedAddressesBtn);

      this.refs.formAutofillFragment = formAutofillFragment;
      this.refs.addressAutofillCheckbox = addressAutofillCheckbox;
      this.refs.savedAddressesBtn = savedAddressesBtn;
    }

    if (showCreditCardUI) {
      let savedCreditCardsBtnWrapper = document.createXULElement("hbox");
      let creditCardAutofill = document.createXULElement("hbox");
      let creditCardAutofillCheckboxGroup = document.createXULElement("hbox");
      let creditCardAutofillCheckbox = document.createXULElement("checkbox");
      let creditCardAutofillLearnMore = document.createElement("a", {
        is: "moz-support-link",
      });
      let savedCreditCardsBtn = document.createXULElement("button", {
        is: "highlightable-button",
      });
      savedCreditCardsBtn.className = "accessory-button";
      creditCardAutofillCheckbox.className = "tail-with-learn-more";

      creditCardAutofill.id = "creditCardAutofill";
      creditCardAutofillLearnMore.id = "creditCardAutofillLearnMore";

      creditCardAutofill.setAttribute(
        "data-subcategory",
        "credit-card-autofill"
      );
      creditCardAutofillCheckbox.setAttribute(
        "label",
        lazy.l10n.formatValueSync("autofill-payment-methods-checkbox-message")
      );

      savedCreditCardsBtn.setAttribute(
        "label",
        lazy.l10n.formatValueSync("autofill-saved-payment-methods-button")
      );
      // Align the start to keep the savedCreditCardsBtn as original size
      // when creditCardAutofillCheckboxGroup's height is changed by a longer l10n string
      savedCreditCardsBtnWrapper.setAttribute("align", "start");

      creditCardAutofillLearnMore.setAttribute(
        "support-page",
        "credit-card-autofill"
      );

      let creditCardsAutofillDescription =
        document.createXULElement("description");

      creditCardsAutofillDescription.setAttribute("flex", "1");
      creditCardsAutofillDescription.className = "indent tip-caption";
      creditCardsAutofillDescription.setAttribute("data-l10n-attrs", "hidden");
      creditCardsAutofillDescription.setAttribute(
        "data-l10n-id",
        "autofill-payment-methods-checkbox-submessage"
      );

      // Add preferences search support
      savedCreditCardsBtn.setAttribute(
        "search-l10n-ids",
        MANAGE_CREDITCARDS_L10N_IDS.concat(EDIT_CREDITCARD_L10N_IDS).join(",")
      );

      // Manually set the checked state
      if (FormAutofill.isAutofillCreditCardsEnabled) {
        creditCardAutofillCheckbox.setAttribute("checked", true);
      }
      if (FormAutofill.isAutofillCreditCardsLocked) {
        creditCardAutofillCheckbox.disabled = true;
      }

      creditCardAutofillCheckboxGroup.setAttribute("align", "center");
      creditCardAutofillCheckboxGroup.setAttribute("flex", "1");

      formAutofillGroup.appendChild(creditCardAutofill);
      creditCardAutofill.appendChild(creditCardAutofillCheckboxGroup);
      creditCardAutofillCheckboxGroup.appendChild(creditCardAutofillCheckbox);
      creditCardAutofillCheckboxGroup.appendChild(creditCardAutofillLearnMore);
      creditCardAutofill.appendChild(savedCreditCardsBtnWrapper);
      savedCreditCardsBtnWrapper.appendChild(savedCreditCardsBtn);
      formAutofillGroup.appendChild(creditCardsAutofillDescription);

      this.refs.creditCardAutofillCheckbox = creditCardAutofillCheckbox;
      this.refs.savedCreditCardsBtn = savedCreditCardsBtn;

      if (
        lazy.OSKeyStore.canReauth() &&
        !Services.prefs.getBoolPref("security.nocertdb", false)
      ) {
        let reauth = document.createXULElement("hbox");
        let reauthCheckboxGroup = document.createXULElement("hbox");
        let reauthCheckbox = document.createXULElement("checkbox");
        let reauthLearnMore = document.createElement("a", {
          is: "moz-support-link",
        });

        reauthCheckboxGroup.classList.add("indent");
        reauthCheckbox.classList.add("tail-with-learn-more");
        reauthCheckbox.disabled = !FormAutofill.isAutofillCreditCardsEnabled;

        reauth.id = "creditCardReauthenticate";
        reauthLearnMore.id = "creditCardReauthenticateLearnMore";

        reauth.setAttribute("data-subcategory", "reauth-credit-card-autofill");

        reauthCheckbox.setAttribute(
          "label",
          lazy.l10n.formatValueSync("autofill-reauth-payment-methods-checkbox")
        );

        // If target.checked is checked, enable OSAuth. Otherwise, reset the pref value.
        reauthCheckbox.setAttribute(
          "checked",
          FormAutofillUtils.getOSAuthEnabled(AUTOFILL_CREDITCARDS_REAUTH_PREF)
        );

        reauthLearnMore.setAttribute(
          "support-page",
          "credit-card-autofill#w_require-authentication-for-autofill"
        );

        reauthCheckboxGroup.setAttribute("align", "center");
        reauthCheckboxGroup.setAttribute("flex", "1");

        formAutofillGroup.appendChild(reauth);
        reauth.appendChild(reauthCheckboxGroup);
        reauthCheckboxGroup.appendChild(reauthCheckbox);
        reauthCheckboxGroup.appendChild(reauthLearnMore);
        this.refs.reauthCheckbox = reauthCheckbox;
      }
    }
  },

  /**
   * Handle events
   *
   * @param  {DOMEvent} event
   */
  async handleEvent(event) {
    switch (event.type) {
      case "command": {
        let target = event.target;

        if (target == this.refs.addressAutofillCheckbox) {
          // Set preference directly instead of relying on <Preference>
          Services.prefs.setBoolPref(
            ENABLED_AUTOFILL_ADDRESSES_PREF,
            target.checked
          );
        } else if (target == this.refs.creditCardAutofillCheckbox) {
          Services.prefs.setBoolPref(
            ENABLED_AUTOFILL_CREDITCARDS_PREF,
            target.checked
          );
          if (this.refs.reauthCheckbox) {
            this.refs.reauthCheckbox.disabled = !target.checked;
          }
        } else if (target == this.refs.reauthCheckbox) {
          if (!lazy.OSKeyStore.canReauth()) {
            break;
          }

          let messageText = await lazy.l10n.formatValueSync(
            "autofill-creditcard-os-dialog-message"
          );
          let captionText = await lazy.l10n.formatValueSync(
            "autofill-creditcard-os-auth-dialog-caption"
          );
          let win = target.ownerGlobal.docShell.chromeEventHandler.ownerGlobal;
          // Calling OSKeyStore.ensureLoggedIn() instead of FormAutofillUtils.verifyOSAuth()
          // since we want to authenticate user each time this stting is changed.
          let isAuthorized = (
            await lazy.OSKeyStore.ensureLoggedIn(
              messageText,
              captionText,
              win,
              false
            )
          ).authenticated;
          if (!isAuthorized) {
            target.checked = !target.checked;
            break;
          }

          // If target.checked is checked, enable OSAuth. Otherwise, reset the pref value.
          FormAutofillUtils.setOSAuthEnabled(
            AUTOFILL_CREDITCARDS_REAUTH_PREF,
            target.checked
          );
        } else if (target == this.refs.savedAddressesBtn) {
          target.ownerGlobal.gSubDialog.open(MANAGE_ADDRESSES_URL);
        } else if (target == this.refs.savedCreditCardsBtn) {
          target.ownerGlobal.gSubDialog.open(MANAGE_CREDITCARDS_URL);
        }
        break;
      }
      case "click": {
        let target = event.target;

        if (target == this.refs.addressAutofillCheckboxLabel) {
          let pref = FormAutofill.isAutofillAddressesEnabled;
          Services.prefs.setBoolPref(ENABLED_AUTOFILL_ADDRESSES_PREF, !pref);
          this.refs.addressAutofillCheckbox.checked = !pref;
        } else if (target == this.refs.creditCardAutofillCheckboxLabel) {
          let pref = FormAutofill.isAutofillCreditCardsEnabled;
          Services.prefs.setBoolPref(ENABLED_AUTOFILL_CREDITCARDS_PREF, !pref);
          this.refs.creditCardAutofillCheckbox.checked = !pref;
          this.refs.reauthCheckbox.disabled = pref;
        }
        break;
      }
    }
  },

  /**
   * Attach event listener
   */
  attachEventListeners() {
    this.refs.formAutofillGroup.addEventListener("command", this);
    this.refs.formAutofillGroup.addEventListener("click", this);
  },

  /**
   * Remove event listener
   */
  detachEventListeners() {
    this.refs.formAutofillGroup.removeEventListener("command", this);
    this.refs.formAutofillGroup.removeEventListener("click", this);
  },
};
