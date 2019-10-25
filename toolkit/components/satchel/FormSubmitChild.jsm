/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["FormSubmitChild"];

const { ActorChild } = ChromeUtils.import(
  "resource://gre/modules/ActorChild.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "CreditCard",
  "resource://gre/modules/CreditCard.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

class FormSubmitChild extends ActorChild {
  constructor(dispatcher) {
    super(dispatcher);

    this.QueryInterface = ChromeUtils.generateQI([
      Ci.nsIObserver,
      Ci.nsISupportsWeakReference,
    ]);

    Services.prefs.addObserver("browser.formfill.", this);
    this.updatePrefs();
  }

  cleanup() {
    Services.prefs.removeObserver("browser.formfill.", this);
  }

  updatePrefs() {
    this.debug = Services.prefs.getBoolPref("browser.formfill.debug");
    this.enabled = Services.prefs.getBoolPref("browser.formfill.enable");
  }

  log(message) {
    if (!this.debug) {
      return;
    }
    dump("satchelFormListener: " + message + "\n");
    Services.console.logStringMessage("satchelFormListener: " + message);
  }

  /* ---- nsIObserver interface ---- */

  observe(subject, topic, data) {
    if (topic == "nsPref:changed") {
      this.updatePrefs();
    } else {
      this.log("Oops! Unexpected notification: " + topic);
    }
  }

  handleEvent(event) {
    switch (event.type) {
      case "DOMFormBeforeSubmit": {
        this.onDOMFormBeforeSubmit(event);
        break;
      }
      default: {
        throw new Error("Unexpected event");
      }
    }
  }

  onDOMFormBeforeSubmit(event) {
    let form = event.target;
    if (
      !this.enabled ||
      PrivateBrowsingUtils.isContentWindowPrivate(form.ownerGlobal)
    ) {
      return;
    }

    this.log("Form submit observer notified.");

    if (
      form.hasAttribute("autocomplete") &&
      form.getAttribute("autocomplete").toLowerCase() == "off"
    ) {
      return;
    }

    let entries = [];
    for (let input of form.elements) {
      if (ChromeUtils.getClassName(input) !== "HTMLInputElement") {
        continue;
      }

      // Only use inputs that hold text values (not including type="password")
      if (!input.mozIsTextField(true)) {
        continue;
      }

      // Don't save fields that were previously type=password such as on sites
      // that allow the user to toggle password visibility.
      if (input.hasBeenTypePassword) {
        continue;
      }

      // Bug 394612: If Login Manager marked this input, don't save it.
      // The login manager will deal with remembering it.

      // Don't save values when @autocomplete is "off" or has a sensitive field name.
      let autocompleteInfo = input.getAutocompleteInfo();
      if (autocompleteInfo && !autocompleteInfo.canAutomaticallyPersist) {
        continue;
      }

      let value = input.value.trim();

      // Don't save empty or unchanged values.
      if (!value || value == input.defaultValue.trim()) {
        continue;
      }

      // Don't save credit card numbers.
      if (CreditCard.isValidNumber(value)) {
        this.log("skipping saving a credit card number");
        continue;
      }

      let name = input.name || input.id;
      if (!name) {
        continue;
      }

      if (name == "searchbar-history") {
        this.log('addEntry for input name "' + name + '" is denied');
        continue;
      }

      // Limit stored data to 200 characters.
      if (name.length > 200 || value.length > 200) {
        this.log("skipping input that has a name/value too large");
        continue;
      }

      // Limit number of fields stored per form.
      if (entries.length >= 100) {
        this.log("not saving any more entries for this form.");
        break;
      }

      entries.push({ name, value });
    }

    if (entries.length) {
      this.log("sending entries to parent process for form " + form.id);
      this.sendAsyncMessage("FormHistory:FormSubmitEntries", entries);
    }
  }
}
