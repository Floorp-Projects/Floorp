/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function LOG() {
  return; // comment out for verbose debugging
  let msg = Array.join(arguments, " ");
  dump(msg + "\n");
  Cu.reportError(msg);
}

// Lazily get the base Form AutoComplete Search
XPCOMUtils.defineLazyGetter(this, "FAC", function() {
  return Components.classesByID["{c11c21b2-71c9-4f87-a0f8-5e13f50495fd}"]
                   .getService(Ci.nsIFormAutoComplete);
});

XPCOMUtils.defineLazyGetter(this, "Contacts", function() {
  Cu.import("resource:///modules/contacts.jsm");
  return Contacts;
});

function FormAutoComplete() {
  LOG("new FAC");
}

FormAutoComplete.prototype = {
  classDescription: "Form AutoComplete Plus",
  classID: Components.ID("{cccd414c-3ec2-4cc5-9dc4-36c87cc3c4fe}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIFormAutoComplete]),

  // Specify the html5 types that we want and some values to guess
  contactTypes: {
    email: /^(?:.*(?:e-?mail|recipients?).*|(send_)?to(_b?cc)?)$/i,
    tel: /^(?:tel(?:ephone)?|.*phone.*)$/i
  },

  checkQueryType: function checkQueryType(aName, aField) {
    // If we have an input field with the desired html5 type, take it!
    if (aField && "type" in aField) {
      let type = aField.type;
      if (type && type in this.contactTypes)
        return type;
    }

    // Grab properties to check for contact inputs
    let props = [aName];
    if (aField) {
      let specialProps = [aField["className"], aField["id"]];
      props = props.concat(specialProps.filter(function(aValue) {
        return aValue;
      }));
    }

    // Check the gathered properties for contact-like values
    for (let [type, regex] in Iterator(this.contactTypes)) {
      if (props.some(function(prop) prop.search(regex) != -1))
        return type;
    }
    return null;
  },

  findContact: function findContact(aQuery, aType, aResult, aDupCheck) {
    // Match the name and show the email for now..
    Contacts.find({ fullName: aQuery }).forEach(function(contact) {
      // Might not have an email for some reason... ?
      try {
        LOG("findContact", "Contact " + contact.fullName);

        let suggestions;
        switch (aType) {
          case "email":
            suggestions = contact.emails;
            break;
          case "tel":
            suggestions = contact.phoneNumbers;
            break;
          default:
            LOG("unknown type!", aType);
            return;
        }

        for each (let suggestion in suggestions) {
          if (aDupCheck[suggestion])
            continue;
          aDupCheck[suggestion] = true;

          let data = contact.fullName + " <" + suggestion + ">";
          aResult.appendMatch(suggestion, data, null, "contact");
        }
      }
      catch(ex) {
        LOG("findContact error", ex);
      }
    });
  },

  autoCompleteSearch: function autoCompleteSearch(aName, aQuery, aField, aPrev) {
    if (!Services.prefs.getBoolPref("browser.formfill.enable"))
      return null;

    LOG("autocomplete search", Array.slice(arguments));
    let result = Cc["@mozilla.org/autocomplete/simple-result;1"].createInstance(Ci.nsIAutoCompleteSimpleResult);
    result.setSearchString(aQuery);

    // Don't allow duplicates get merged into the final results
    let dupCheck = {};

    // Use the base form autocomplete for non-contact searches
    let normal = FAC.autoCompleteSearch(aName, aQuery, aField, aPrev);
    if (normal.matchCount > 0) {
      for (let i = 0; i < normal.matchCount; i++) {
        dupCheck[normal.getValueAt(i)] = true;
        result.appendMatch(normal.getValueAt(i), normal.getCommentAt(i), normal.getImageAt(i), normal.getStyleAt(i));
      }
    }

    // Do searches for certain input fields
    let type = this.checkQueryType(aName, aField);
    if (type != null)
      this.findContact(aQuery, type, result, dupCheck);

    let resultCode = result.matchCount ? "RESULT_SUCCESS" : "RESULT_NOMATCH";
    result.setSearchResult(Ci.nsIAutoCompleteResult[resultCode]);
    return result;
  }
};

let components = [FormAutoComplete];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
