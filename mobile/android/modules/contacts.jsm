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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Finkle <mfinkle@mozilla.com>
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

let EXPORTED_SYMBOLS = ["Contacts"];

const Cu = Components.utils;

let Contacts = {
  _providers: [],
  _contacts: [],

  _load: function _load() {
    this._contacts = [];

    this._providers.forEach(function(provider) {
      this._contacts = this._contacts.concat(provider.getContacts());
    }, this)
  },

  init: function init() {
    // Not much to do for now
    this._load();
  },

  refresh: function refresh() {
    // Pretty simple for now
    this._load();
  },

  addProvider: function(aProvider) {
    this._providers.push(aProvider);
    this.refresh();
  },

  find: function find(aMatch) {
    let results = [];

    if (!this._contacts.length)
      return results;

    for (let field in aMatch) {
      // Simple string-only partial matching
      let match = aMatch[field];
      this._contacts.forEach(function(aContact) {
        if (field in aContact && aContact[field].indexOf(match) != -1)
          results.push(aContact);
      });
    }
    return results;
  }
};

#ifndef ANDROID
#ifndef XP_MACOSX
#ifdef XP_UNIX
Cu.import("resource://gre/modules/ctypes.jsm");
Cu.import("resource:///modules/linuxTypes.jsm");

function EBookProvider() {
  EBook.init();
}

EBookProvider.prototype = {
  getContacts: function() {
    if (!EBook.lib) {
      Cu.reportError("EBook not loaded")
      return [];
    }

    let gError = new GLib.GError.ptr;
    let book = EBook.openSystem(gError.address());
    if (!book) {
      Cu.reportError("EBook.openSystem: " + gError.contents.message.readString())
      return [];
    }

    if (!EBook.openBook(book, false, gError.address())) {
      Cu.reportError("EBook.openBook: " + gError.contents.message.readString())
      return [];
    }

    let query = EBook.queryAnyFieldContains("");
    if (query) {
      let gList = new GLib.GList.ptr();
      if (!EBook.getContacts(book, query, gList.address(),  gError.address())) {
        Cu.reportError("EBook.getContacts: " + gError.contents.message.readString())
        return [];
      }

      let contacts = [];
      while (gList && !gList.isNull()) {
        let fullName = EBook.getContactField(gList.contents.data, EBook.E_CONTACT_FULL_NAME);
        if (!fullName.isNull()) {
          let contact = {};
          contact.fullName = fullName.readString();
          contact.emails = [];
          contact.phoneNumbers = [];

          for (let emailIndex=EBook.E_CONTACT_EMAIL_FIRST; emailIndex<=EBook.E_CONTACT_EMAIL_LAST; emailIndex++) {
            let email = EBook.getContactField(gList.contents.data, emailIndex);
            if (!email.isNull())
              contact.emails.push(email.readString());
          }

          for (let phoneIndex=EBook.E_CONTACT_PHONE_FIRST; phoneIndex<=EBook.E_CONTACT_PHONE_LAST; phoneIndex++) {
            let phone = EBook.getContactField(gList.contents.data, phoneIndex);
            if (!phone.isNull())
              contact.phoneNumbers.push(phone.readString());
          }

          contacts.push(contact);
        }
        gList = ctypes.cast(gList.contents.next, GLib.GList.ptr);
      }
      return contacts;
    }
    return [];
  }
};

Contacts.addProvider(new EBookProvider);
# XP_UNIX
#endif
#endif
#endif
