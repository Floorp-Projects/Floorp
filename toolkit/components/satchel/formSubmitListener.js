/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

(function() {

var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

var satchelFormListener = {
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver,
                                            Ci.nsIDOMEventListener,
                                            Ci.nsIObserver,
                                            Ci.nsISupportsWeakReference]),

    debug          : true,
    enabled        : true,
    saveHttpsForms : true,

    init : function() {
        Services.obs.addObserver(this, "earlyformsubmit", false);
        Services.prefs.addObserver("browser.formfill.", this, false);
        this.updatePrefs();
        addEventListener("unload", this, false);
    },

    updatePrefs : function() {
        this.debug          = Services.prefs.getBoolPref("browser.formfill.debug");
        this.enabled        = Services.prefs.getBoolPref("browser.formfill.enable");
        this.saveHttpsForms = Services.prefs.getBoolPref("browser.formfill.saveHttpsForms");
    },

    // Implements the Luhn checksum algorithm as described at
    // http://wikipedia.org/wiki/Luhn_algorithm
    isValidCCNumber : function(ccNumber) {
        // Remove dashes and whitespace
        ccNumber = ccNumber.replace(/[\-\s]/g, '');

        let len = ccNumber.length;
        if (len != 9 && len != 15 && len != 16)
            return false;

        if (!/^\d+$/.test(ccNumber))
            return false;

        let total = 0;
        for (let i = 0; i < len; i++) {
            let ch = parseInt(ccNumber[len - i - 1]);
            if (i % 2 == 1) {
                // Double it, add digits together if > 10
                ch *= 2;
                if (ch > 9)
                    ch -= 9;
            }
            total += ch;
        }
        return total % 10 == 0;
    },

    log : function(message) {
        if (!this.debug)
            return;
        dump("satchelFormListener: " + message + "\n");
        Services.console.logStringMessage("satchelFormListener: " + message);
    },

    /* ---- dom event handler ---- */

    handleEvent: function(e) {
        switch (e.type) {
            case "unload":
                Services.obs.removeObserver(this, "earlyformsubmit");
                Services.prefs.removeObserver("browser.formfill.", this);
                break;

            default:
                this.log("Oops! Unexpected event: " + e.type);
                break;
        }
    },

    /* ---- nsIObserver interface ---- */

    observe : function(subject, topic, data) {
        if (topic == "nsPref:changed")
            this.updatePrefs();
        else
            this.log("Oops! Unexpected notification: " + topic);
    },

    /* ---- nsIFormSubmitObserver interfaces ---- */

    notify : function(form, domWin, actionURI, cancelSubmit) {
        try {
            // Even though the global context is for a specific browser, we
            // can receive observer events from other tabs! Ensure this event
            // is about our content.
            if (domWin.top != content)
                return;
            if (!this.enabled)
                return;

            if (PrivateBrowsingUtils.isContentWindowPrivate(domWin))
                return;

            this.log("Form submit observer notified.");

            if (!this.saveHttpsForms) {
                if (actionURI.schemeIs("https"))
                    return;
                if (form.ownerDocument.documentURIObject.schemeIs("https"))
                    return;
            }

            if (form.hasAttribute("autocomplete") &&
                form.getAttribute("autocomplete").toLowerCase() == "off")
                return;

            let entries = [];
            for (let i = 0; i < form.elements.length; i++) {
                let input = form.elements[i];
                if (!(input instanceof Ci.nsIDOMHTMLInputElement))
                    continue;

                // Only use inputs that hold text values (not including type="password")
                if (!input.mozIsTextField(true))
                    continue;

                // Bug 394612: If Login Manager marked this input, don't save it.
                // The login manager will deal with remembering it.

                // Don't save values when autocomplete=off is present.
                if (input.hasAttribute("autocomplete") &&
                    input.getAttribute("autocomplete").toLowerCase() == "off")
                    continue;

                let value = input.value.trim();

                // Don't save empty or unchanged values.
                if (!value || value == input.defaultValue.trim())
                    continue;

                // Don't save credit card numbers.
                if (this.isValidCCNumber(value)) {
                    this.log("skipping saving a credit card number");
                    continue;
                }

                let name = input.name || input.id;
                if (!name)
                    continue;

                if (name == 'searchbar-history') {
                    this.log('addEntry for input name "' + name + '" is denied')
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

                entries.push({ name: name, value: value });
            }

            if (entries.length) {
                this.log("sending entries to parent process for form " + form.id);
                sendAsyncMessage("FormHistory:FormSubmitEntries", entries);
            }
        }
        catch (e) {
            this.log("notify failed: " + e);
        }
    }
};

satchelFormListener.init();

})();
