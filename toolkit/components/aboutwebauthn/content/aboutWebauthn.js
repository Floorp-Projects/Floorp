/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let AboutWebauthnService = null;

var AboutWebauthnManagerJS = {
  _topic: "about-webauthn-prompt",
  _initialized: false,
  _l10n: null,

  init() {
    if (this._initialized) {
      return;
    }
    this._l10n = new Localization(["toolkit/about/aboutWebauthn.ftl"], true);
    Services.obs.addObserver(this, this._topic);
    this._initialized = true;
    reset_page();
  },

  uninit() {
    Services.obs.removeObserver(this, this._topic);
  },

  observe(aSubject, aTopic, aData) {
    let data = JSON.parse(aData);

    // We have token
    if (data.type == "selected-device") {
      this._curr_data = data.auth_info;
      document.getElementById("token-info-section").style.display = "block";
      this.show_ui_based_on_authenticator_info(data);
    } else if (data.type == "select-device") {
      set_info_text("about-webauthn-text-select-device");
    } else if (data.type == "listen-success" || data.type == "listen-error") {
      reset_page();
      AboutWebauthnService.listen();
    }
  },

  show_authenticator_options(options, element, l10n_base) {
    let table = document.getElementById(element);
    var empty_table = document.createElement("table");
    empty_table.id = element;
    table.parentNode.replaceChild(empty_table, table);
    table = document.getElementById(element);
    for (let key in options) {
      if (key == "options") {
        continue;
      }
      // Create an empty <tr> element and add it to the 1st position of the table:
      var row = table.insertRow(0);

      // Insert new cells (<td> elements) at the 1st and 2nd position of the "new" <tr> element:
      var cell1 = row.insertCell(0);
      var cell2 = row.insertCell(1);

      // Add some text to the new cells:
      let key_text = this._l10n.formatValueSync(
        l10n_base + "-" + key.toLowerCase().replace(/_/g, "-")
      );
      var key_node = document.createTextNode(key_text);
      cell1.appendChild(key_node);
      var raw_value = JSON.stringify(options[key]);
      var value = raw_value;
      if (["true", "false", "null"].includes(raw_value)) {
        value = this._l10n.formatValueSync(l10n_base + "-" + raw_value);
      }
      var value_node = document.createTextNode(value);
      cell2.appendChild(value_node);
    }
  },

  show_ui_based_on_authenticator_info(data) {
    // Hide the "Please plug in a token"-message
    document.getElementById("info-text-div").hidden = true;
    // Show options, based on what the token supports
    if (data.auth_info) {
      document.getElementById("main-content").hidden = false;
      document.getElementById("categories").hidden = false;
      this.show_authenticator_options(
        data.auth_info.options,
        "authenticator-options",
        "about-webauthn-auth-option"
      );
      this.show_authenticator_options(
        data.auth_info,
        "authenticator-info",
        "about-webauthn-auth-info"
      );
    } else {
      // Currently auth-rs doesn't send this, because it filters out ctap2-devices.
      // U2F / CTAP1 tokens can't be managed
      set_info_text("about-webauthn-text-non-ctap2-device");
    }
  },
};

function set_info_text(l10nId) {
  document.getElementById("info-text-div").hidden = false;
  let field = document.getElementById("info-text-field");
  field.setAttribute("data-l10n-id", l10nId);
}

function reset_page() {
  // Hide all main sections
  document.getElementById("main-content").hidden = true;
  document.getElementById("categories").hidden = true;

  // Only display the "please connect a device" - text
  set_info_text("about-webauthn-text-connect-device");
}

async function onLoad() {
  AboutWebauthnManagerJS.init();
  try {
    AboutWebauthnService.listen();
  } catch (ex) {
    set_info_text("about-webauthn-text-not-available");
    AboutWebauthnManagerJS.uninit();
  }
}

try {
  AboutWebauthnService = Cc["@mozilla.org/webauthn/service;1"].getService(
    Ci.nsIWebAuthnService
  );
  document.addEventListener("DOMContentLoaded", onLoad);
  window.addEventListener("beforeunload", event => {
    AboutWebauthnManagerJS.uninit();
    if (AboutWebauthnService) {
      AboutWebauthnService.cancel(0);
    }
  });
} catch (ex) {
  // Do nothing if we fail to create a singleton instance,
  // showing the default no-module message.
  console.error(ex);
}
