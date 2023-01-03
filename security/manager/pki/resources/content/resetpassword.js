/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

document.addEventListener("dialogaccept", resetPassword);

function resetPassword() {
  var pk11db = Cc["@mozilla.org/security/pk11tokendb;1"].getService(
    Ci.nsIPK11TokenDB
  );
  var token = pk11db.getInternalKeyToken();
  token.reset();

  try {
    Services.logins.removeAllUserFacingLogins();
  } catch (e) {}

  let l10n = new Localization(["security/pippki/pippki.ftl"], true);
  if (l10n) {
    Services.prompt.alert(
      window,
      l10n.formatValueSync("pippki-reset-password-confirmation-title"),
      l10n.formatValueSync("pippki-reset-password-confirmation-message")
    );
  }
}
