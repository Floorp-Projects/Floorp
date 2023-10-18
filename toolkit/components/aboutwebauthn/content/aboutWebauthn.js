/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let AboutWebauthnService = null;

var AboutWebauthnManagerJS = {
  _topic: "about-webauthn-prompt",
  _initialized: false,
  _l10n: null,
  _current_tab: "",
  _previous_tab: "",

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
      fake_click_event_for_id("info-tab-button");
      this.show_ui_based_on_authenticator_info(data);
    } else if (data.type == "select-device") {
      set_info_text("about-webauthn-text-select-device");
    } else if (data.type == "pin-required") {
      open_pin_required_tab();
    } else if (data.type == "pin-invalid") {
      let retries = data.retries ? data.retries : 0;
      show_results_banner(
        "error",
        "about-webauthn-results-pin-invalid-error",
        JSON.stringify({ retriesLeft: retries })
      );
      open_pin_required_tab();
    } else if (data.type == "credential-management-update") {
      credential_management_in_progress(false);
      if (data.result.CredentialList) {
        show_results_banner("success", "about-webauthn-results-success");
        this.show_credential_list(data.result.CredentialList.credential_list);
      } else {
        // DeleteSuccess or UpdateSuccess
        show_results_banner("success", "about-webauthn-results-success");
        list_credentials();
      }
    } else if (data.type == "listen-success") {
      reset_page();
      // Show results
      show_results_banner("success", "about-webauthn-results-success");
      this._reset_in_progress = "";
      AboutWebauthnService.listen();
    } else if (data.type == "listen-error") {
      reset_page();

      if (!data.error) {
        show_results_banner("error", "about-webauthn-results-general-error");
      } else if (data.error.type == "pin-auth-blocked") {
        show_results_banner(
          "error",
          "about-webauthn-results-pin-auth-blocked-error"
        );
      } else if (data.error.type == "device-blocked") {
        show_results_banner(
          "error",
          "about-webauthn-results-pin-blocked-error"
        );
      } else if (data.error.type == "pin-is-too-short") {
        show_results_banner(
          "error",
          "about-webauthn-results-pin-too-short-error"
        );
      } else if (data.error.type == "pin-is-too-long") {
        show_results_banner(
          "error",
          "about-webauthn-results-pin-too-long-error"
        );
      } else if (data.error.type == "pin-invalid") {
        let retries = data.error.retries
          ? JSON.stringify({ retriesLeft: data.error.retries })
          : null;
        show_results_banner(
          "error",
          "about-webauthn-results-pin-invalid-error",
          retries
        );
      } else if (data.error.type == "cancel") {
        show_results_banner(
          "error",
          "about-webauthn-results-cancelled-by-user-error"
        );
      } else {
        show_results_banner("error", "about-webauthn-results-general-error");
      }
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
      // Check if token supports PINs
      if (data.auth_info.options.clientPin != null) {
        document.getElementById("pin-tab-button").style.display = "flex";
        if (data.auth_info.options.clientPin === true) {
          // It has a Pin set
          document.getElementById("change-pin-button").style.display = "block";
          document.getElementById("set-pin-button").style.display = "none";
          document.getElementById("current-pin-div").hidden = false;
        } else {
          // It does not have a Pin set yet
          document.getElementById("change-pin-button").style.display = "none";
          document.getElementById("set-pin-button").style.display = "block";
          document.getElementById("current-pin-div").hidden = true;
        }
      } else {
        document.getElementById("pin-tab-button").style.display = "none";
      }

      if (
        data.auth_info.options.credMgmt ||
        data.auth_info.options.credentialMgmtPreview
      ) {
        document.getElementById("credentials-tab-button").style.display =
          "flex";
      } else {
        document.getElementById("credentials-tab-button").style.display =
          "none";
      }
    } else {
      // Currently auth-rs doesn't send this, because it filters out ctap2-devices.
      // U2F / CTAP1 tokens can't be managed
      set_info_text("about-webauthn-text-non-ctap2-device");
    }
  },

  show_credential_list(credential_list) {
    // We may have temporarily hidden the tab when asking the user for a PIN
    // so we have to show it again.
    fake_click_event_for_id("credentials-tab-button");
    document.getElementById("credential-list-subsection").hidden = false;
    let table = document.getElementById("credential-list");
    var empty_table = document.createElement("table");
    empty_table.id = "credential-list";
    table.parentNode.replaceChild(empty_table, table);
    if (!credential_list.length) {
      document.getElementById("credential-list-empty-label").hidden = false;
      return;
    }
    document.getElementById("credential-list-empty-label").hidden = true;
    table = document.getElementById("credential-list");
    credential_list.forEach(rp => {
      // Add some text to the new cells:
      let key_text = rp.rp.id;
      rp.credentials.forEach(cred => {
        let value_text = cred.user.name;
        // Create an empty <tr> element and add it to the 1st position of the table:
        var row = table.insertRow(0);
        var key_node = document.createTextNode(key_text);
        var value_node = document.createTextNode(value_text);
        row.insertCell(0).appendChild(key_node);
        row.insertCell(1).appendChild(value_node);
        var delete_button = document.createElement("button");
        delete_button.classList.add("credentials-button");
        // TODO: Garbage-can symbol instead? See https://bugzilla.mozilla.org/show_bug.cgi?id=1859727
        delete_button.setAttribute(
          "data-l10n-id",
          "about-webauthn-delete-button"
        );
        delete_button.addEventListener("click", function () {
          credential_management_in_progress(true);
          let cmd = {
            CredentialManagement: { DeleteCredential: cred.credential_id },
          };
          AboutWebauthnService.runCommand(JSON.stringify(cmd));
        });
        row.insertCell(2).appendChild(delete_button);
      });
    });
  },
};

function set_info_text(l10nId) {
  document.getElementById("info-text-div").hidden = false;
  let field = document.getElementById("info-text-field");
  field.setAttribute("data-l10n-id", l10nId);
}

function show_results_banner(result, l10n, l10n_args) {
  document.getElementById("ctap-listen-div").hidden = false;
  let ctap_result = document.getElementById("ctap-listen-result");
  ctap_result.setAttribute("data-l10n-id", l10n);
  ctap_result.classList.add(result);
  if (l10n_args) {
    ctap_result.setAttribute("data-l10n-args", l10n_args);
  }
}

function hide_results_banner() {
  document
    .getElementById("ctap-listen-result")
    .classList.remove("success", "error");
  document.getElementById("ctap-listen-div").hidden = true;
}

function credential_management_in_progress(in_progress) {
  let buttons = Array.from(
    document.getElementsByClassName("credentials-button")
  );
  buttons.forEach(button => {
    button.disabled = in_progress;
  });
}

function fake_click_event_for_id(id) {
  // Not using document.getElementById(id).click();
  // here, because we have to add additional data, so we don't
  // hide the results-div here, if there is any. 'Normal' clicking
  // by the user will hide it.
  const evt = new CustomEvent("click", {
    detail: { skip_results_clearing: true },
  });
  document.getElementById(id).dispatchEvent(evt);
}

function reset_page() {
  // Hide all main sections
  document.getElementById("main-content").hidden = true;
  document.getElementById("categories").hidden = true;

  // Clear results and input fields
  hide_results_banner();
  var divs = Array.from(document.getElementsByTagName("input"));
  divs.forEach(div => {
    div.value = "";
  });

  sidebar_set_disabled(false);

  // ListCredentials
  credential_management_in_progress(false);
  document.getElementById("credential-list-subsection").hidden = true;

  // Only display the "please connect a device" - text
  set_info_text("about-webauthn-text-connect-device");

  AboutWebauthnManagerJS._previous_tab = "";
  AboutWebauthnManagerJS._current_tab = "";
}

function sidebar_set_disabled(disabled) {
  var cats = Array.from(document.getElementsByClassName("category"));
  cats.forEach(cat => {
    if (disabled) {
      cat.classList.add("disabled-category");
    } else {
      cat.classList.remove("disabled-category");
    }
  });
}

function check_pin_repeat_is_correct(button) {
  let pin = document.getElementById("new-pin");
  let pin_repeat = document.getElementById("new-pin-repeat");
  let has_current_pin = !document.getElementById("current-pin-div").hidden;
  let current_pin = document.getElementById("current-pin");
  let can_enable_button =
    pin.value != null && pin.value != "" && pin.value == pin_repeat.value;
  if (has_current_pin && !current_pin.value) {
    can_enable_button = false;
  }
  if (!can_enable_button) {
    pin.classList.add("different");
    pin_repeat.classList.add("different");
    document.getElementById("set-pin-button").disabled = true;
    document.getElementById("change-pin-button").disabled = true;
    return false;
  }
  pin.classList.remove("different");
  pin_repeat.classList.remove("different");
  document.getElementById("set-pin-button").disabled = false;
  document.getElementById("change-pin-button").disabled = false;
  return true;
}

function send_pin() {
  close_pin_required_tab();
  let pin = document.getElementById("pin-required").value;
  AboutWebauthnService.pinCallback(0, pin);
}

function set_pin() {
  let pin = document.getElementById("new-pin").value;
  let cmd = { SetPIN: pin };
  AboutWebauthnService.runCommand(JSON.stringify(cmd));
}

function change_pin() {
  let curr_pin = document.getElementById("current-pin").value;
  let new_pin = document.getElementById("new-pin").value;
  let cmd = { ChangePIN: [curr_pin, new_pin] };
  AboutWebauthnService.runCommand(JSON.stringify(cmd));
}

function list_credentials() {
  credential_management_in_progress(true);
  let cmd = { CredentialManagement: "GetCredentials" };
  AboutWebauthnService.runCommand(JSON.stringify(cmd));
}

function cancel_transaction() {
  credential_management_in_progress(false);
  AboutWebauthnService.cancel(0);
}

async function onLoad() {
  AboutWebauthnManagerJS.init();
  try {
    AboutWebauthnService.listen();
  } catch (ex) {
    set_info_text("about-webauthn-text-not-available");
    AboutWebauthnManagerJS.uninit();
    return;
  }
  document.getElementById("set-pin-button").addEventListener("click", set_pin);
  document
    .getElementById("change-pin-button")
    .addEventListener("click", change_pin);
  document
    .getElementById("list-credentials-button")
    .addEventListener("click", list_credentials);
  document
    .getElementById("new-pin")
    .addEventListener("input", check_pin_repeat_is_correct);
  document
    .getElementById("new-pin-repeat")
    .addEventListener("input", check_pin_repeat_is_correct);
  document
    .getElementById("current-pin")
    .addEventListener("input", check_pin_repeat_is_correct);
  document
    .getElementById("info-tab-button")
    .addEventListener("click", open_info_tab);
  document
    .getElementById("pin-tab-button")
    .addEventListener("click", open_pin_tab);
  document
    .getElementById("credentials-tab-button")
    .addEventListener("click", open_credentials_tab);
  document
    .getElementById("send-pin-button")
    .addEventListener("click", send_pin);
  document
    .getElementById("cancel-send-pin-button")
    .addEventListener("click", cancel_transaction);
}

function open_tab(evt, tabName) {
  var tabcontent, tablinks;
  // Hide all others
  tabcontent = Array.from(document.getElementsByClassName("tabcontent"));
  tabcontent.forEach(tab => {
    tab.style.display = "none";
  });
  // Display the one we selected
  document.getElementById(tabName).style.display = "block";

  // If this is a temporary overlay, like pin-required, we don't
  // touch the sidebar and which button is selected.
  if (!evt.detail.temporary_overlay) {
    tablinks = Array.from(document.getElementsByClassName("category"));
    tablinks.forEach(tablink => {
      tablink.removeAttribute("selected");
      tablink.disabled = false;
    });
    evt.currentTarget.setAttribute("selected", "true");
  }

  if (!evt.detail.skip_results_clearing) {
    hide_results_banner();
  }
  sidebar_set_disabled(false);
  AboutWebauthnManagerJS._previous_tab = AboutWebauthnManagerJS._current_tab;
  AboutWebauthnManagerJS._current_tab = tabName;
}

function open_info_tab(evt) {
  open_tab(evt, "token-info-section");
}
function open_pin_tab(evt) {
  open_tab(evt, "set-change-pin-section");
}
function open_credentials_tab(evt) {
  open_tab(evt, "credential-management-section");
}
function open_pin_required_tab() {
  // Remove any old value we might have had
  document.getElementById("pin-required").value = "";
  const evt = new CustomEvent("click", {
    detail: {
      temporary_overlay: true,
      skip_results_clearing: true, // We might be called multiple times, if PIN was invalid
    },
  });
  open_tab(evt, "pin-required-section");
  document.getElementById("pin-required").focus();
  // This is a temporary overlay, so we don't want the
  // user to click away from it, unless via the Cancel-button.
  sidebar_set_disabled(true);
}
function close_pin_required_tab() {
  const evt = new CustomEvent("click", {
    detail: { temporary_overlay: true },
  });
  open_tab(evt, AboutWebauthnManagerJS._previous_tab);
  sidebar_set_disabled(false);
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
