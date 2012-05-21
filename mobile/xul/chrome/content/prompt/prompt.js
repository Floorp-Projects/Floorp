/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PromptHelper = {
  closeDialog: function(confirm, id) {
    let dialog = document.getElementById(id);
    if (typeof confirm == "boolean" && dialog.arguments && "defaultButton" in dialog.arguments)
      // confirmEx always returns 1 when dismissed with "escape" (bug 345067).
      dialog.arguments.result = confirm ? dialog.arguments.defaultButton : 1;
    else
      dialog.arguments.result = confirm;
    dialog.close();
  },

  // Alert dialog
  onCloseAlert: function(dialog) {
    if (dialog.arguments)
      dialog.arguments.value = document.getElementById("prompt-alert-checkbox").checked;
  },

  // Confirm dialog
  closeConfirm: function(confirm) {
    this.closeDialog(confirm, "prompt-confirm-dialog");
  },

  onCloseConfirm: function(dialog) {
    if (dialog.arguments && ("checkbox" in dialog.arguments))
      dialog.arguments.checkbox.value = document.getElementById("prompt-confirm-checkbox").checked;
  },

  // Prompt dialog
  closePrompt: function(confirm) {
    this.closeDialog(confirm, "prompt-prompt-dialog");
  },

  onClosePrompt: function(dialog) {
    if (dialog.arguments) {
      dialog.arguments.checkbox.value = document.getElementById("prompt-prompt-checkbox").checked;
      dialog.arguments.value.value = document.getElementById("prompt-prompt-textbox").value;
    }
  },

  // User / Password dialog
  onLoadPassword: function onLoadPassword(dialog) {
    let user = document.getElementById('prompt-password-user');
    if (!user.value)
      user.focus();
  },

  closePassword: function(confirm) {
    this.closeDialog(confirm, "prompt-password-dialog");
  },

  onClosePassword: function(dialog) {
    if (dialog.arguments) {
      dialog.arguments.checkbox.value = document.getElementById("prompt-password-checkbox").checked;
      dialog.arguments.user.value = document.getElementById("prompt-password-user").value;
      dialog.arguments.password.value = document.getElementById("prompt-password-password").value;
    }
  },

  // Select dialog
  closeSelect: function(confirm) {
    this.closeDialog(confirm, "prompt-select-dialog");
  },

  onCloseSelect: function(dialog) {
    if (dialog.arguments)
      dialog.arguments.selection.value = document.getElementById("prompt-select-list").selectedIndex;
  }
};
