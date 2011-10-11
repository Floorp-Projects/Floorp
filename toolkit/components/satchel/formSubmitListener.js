/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Justin Dolske <dolske@mozilla.com>
 *  Paul O’Shannessy <paul@oshannessy.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

(function(){

var Cc = Components.classes;
var Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

var satchelFormListener = {
    QueryInterface : XPCOMUtils.generateQI([Ci.nsIFormSubmitObserver,
                                            Ci.nsIDOMEventListener,
                                            Ci.nsObserver,
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

    updatePrefs : function () {
        this.debug          = Services.prefs.getBoolPref("browser.formfill.debug");
        this.enabled        = Services.prefs.getBoolPref("browser.formfill.enable");
        this.saveHttpsForms = Services.prefs.getBoolPref("browser.formfill.saveHttpsForms");
    },

    // Implements the Luhn checksum algorithm as described at
    // http://wikipedia.org/wiki/Luhn_algorithm
    isValidCCNumber : function (ccNumber) {
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

    log : function (message) {
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

    observe : function (subject, topic, data) {
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
