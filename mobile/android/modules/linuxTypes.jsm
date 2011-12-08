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

let EXPORTED_SYMBOLS = ["GLib", "EBook"];

const Cu = Components.utils;

Cu.import("resource://gre/modules/ctypes.jsm");

let GLib = {
  lib: null,

  init: function glib_init() {
    if (this.lib)
      return;

    this.lib = true; // TODO hook up the real glib
    
    this.GError = new ctypes.StructType("GError", [
      {"domain": ctypes.int32_t},
      {"code": ctypes.int32_t},
      {"message": ctypes.char.ptr}
    ]);

    this.GList = new ctypes.StructType("GList", [
      {"data": ctypes.voidptr_t},
      {"next": ctypes.voidptr_t},
      {"prev": ctypes.voidptr_t}
    ]);
  }
};

let EBook = {
  lib: null,
  
  E_CONTACT_FULL_NAME: 4,
  E_CONTACT_EMAIL_1: 8,
  E_CONTACT_EMAIL_2: 9,
  E_CONTACT_EMAIL_3: 10,
  E_CONTACT_EMAIL_4: 11,
  E_CONTACT_PHONE_BUSINESS: 17,
  E_CONTACT_PHONE_BUSINESS_2: 18,
  E_CONTACT_PHONE_HOME: 23,
  E_CONTACT_PHONE_HOME_2: 24,
  E_CONTACT_PHONE_MOBILE: 27,

  E_CONTACT_EMAIL_FIRST: 8,
  E_CONTACT_EMAIL_LAST: 11,
  E_CONTACT_PHONE_FIRST: 16,
  E_CONTACT_PHONE_LAST: 34,

  init: function ebook_init() {
    if (this.lib)
      return;

    GLib.init();

    try {
      // Shipping on N900
      this.lib = ctypes.open("libebook-1.2.so.5");
    } catch (e) {
      try {
        // Shipping on Ubuntu
        this.lib = ctypes.open("libebook-1.2.so.9");
      } catch (e) {
        Cu.reportError("EBook: couldn't load libebook:\n" + e)
        this.lib = null;
        return;
      }
    }

    this.EBook = new ctypes.StructType("EBook");
    this.EQuery = new ctypes.StructType("EQuery");

    this.openSystem = this.lib.declare("e_book_new_system_addressbook", ctypes.default_abi, EBook.EBook.ptr, GLib.GError.ptr.ptr);
    this.openBook = this.lib.declare("e_book_open", ctypes.default_abi, ctypes.bool, EBook.EBook.ptr, ctypes.bool, GLib.GError.ptr.ptr);

    this.queryAnyFieldContains = this.lib.declare("e_book_query_any_field_contains", ctypes.default_abi, EBook.EQuery.ptr, ctypes.char.ptr);

    this.getContacts = this.lib.declare("e_book_get_contacts", ctypes.default_abi, ctypes.bool, EBook.EBook.ptr, EBook.EQuery.ptr, GLib.GList.ptr.ptr, GLib.GError.ptr.ptr);
    this.getContactField = this.lib.declare("e_contact_get_const", ctypes.default_abi, ctypes.char.ptr, ctypes.voidptr_t, ctypes.uint32_t);
  }
};
