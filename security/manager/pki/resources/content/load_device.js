/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

/**
 * @file Implements the functionality of load_device.xul: a dialog that allows
 *       a PKCS #11 module to be loaded into Firefox.
 */

function onBrowseBtnPress() {
  let bundle = document.getElementById("pippki_bundle");
  let fp = Cc["@mozilla.org/filepicker;1"].createInstance(Ci.nsIFilePicker);
  fp.init(window, bundle.getString("loadPK11ModuleFilePickerTitle"),
          Ci.nsIFilePicker.modeOpen);
  fp.appendFilters(Ci.nsIFilePicker.filterAll);
  fp.open(rv => {
    if (rv == Ci.nsIFilePicker.returnOK) {
      document.getElementById("device_path").value = fp.file.path;
    }

    // This notification gets sent solely for test purposes. It should not be
    // used by production code.
    Services.obs.notifyObservers(window, "LoadPKCS11Module:FilePickHandled");
  });
}

/**
 * ondialogaccept() handler.
 *
 * @returns {Boolean} true to make the dialog close, false otherwise.
 */
function onDialogAccept() {
  let bundle = document.getElementById("pipnss_bundle");
  let nameBox = document.getElementById("device_name");
  let pathBox = document.getElementById("device_path");
  let pkcs11ModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                         .getService(Ci.nsIPKCS11ModuleDB);

  try {
    pkcs11ModuleDB.addModule(nameBox.value, pathBox.value, 0, 0);
  } catch (e) {
    alertPromptService(null, bundle.getString("AddModuleFailure"));
    return false;
  }

  return true;
}

function validateModuleName() {
  let bundle = document.getElementById("pippki_bundle");
  let name = document.getElementById("device_name").value;
  let helpText = document.getElementById("helpText");
  helpText.value = "";
  let dialogNode = document.querySelector("dialog");
  dialogNode.removeAttribute("buttondisabledaccept");
  if (name == "") {
    helpText.value = bundle.getString("loadModuleHelp_emptyModuleName");
    dialogNode.setAttribute("buttondisabledaccept", true);
  }
  if (name == "Root Certs") {
    helpText.value = bundle.getString("loadModuleHelp_rootCertsModuleName");
    dialogNode.setAttribute("buttondisabledaccept", true);
  }
}
