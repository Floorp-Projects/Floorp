// -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var gOCSPDialog = {
  _updateUI: function (called_by) {
    var securityOCSPEnabled = document.getElementById("security.OCSP.enabled");
    var enableOCSP = document.getElementById("enableOCSP");
    var requireOCSP = document.getElementById("requireOCSP");

    if (called_by) {
      securityOCSPEnabled.value = enableOCSP.checked ? 1 : 0
    } else {
      enableOCSP.checked = parseInt(securityOCSPEnabled.value) != 0;
    }

    requireOCSP.disabled = !enableOCSP.checked;
    return undefined;
  }
};
