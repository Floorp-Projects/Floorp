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
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fabrice Desré <fabrice.desre@gmail.com>, Original author
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
