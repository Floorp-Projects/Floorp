/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* import-globals-from pippki.js */
"use strict";

const nsIDialogParamBlock = Components.interfaces.nsIDialogParamBlock;

var dialogParams;
var itemCount = 0;
var rememberBox;

function onLoad()
{
    var cn;
    var org;
    var issuer;

    dialogParams = window.arguments[0].QueryInterface(nsIDialogParamBlock);
    cn = dialogParams.GetString(0);
    org = dialogParams.GetString(1);
    issuer = dialogParams.GetString(2);

    // added with bug 431819. reuse string from caps in order to avoid string changes
    var capsBundle = document.getElementById("caps_bundle");
    var rememberString = capsBundle.getString("CheckMessage");
    var rememberSetting = true;

    var pref = Components.classes['@mozilla.org/preferences-service;1']
                         .getService(Components.interfaces.nsIPrefService);
    if (pref) {
      pref = pref.getBranch(null);
      try {
	rememberSetting =
	  pref.getBoolPref("security.remember_cert_checkbox_default_setting");
      } catch (e) {
	// pref is missing
      }
    }

    rememberBox = document.getElementById("rememberBox");
    rememberBox.label = rememberString;
    rememberBox.checked = rememberSetting;

    var bundle = document.getElementById("pippki_bundle");
    var message1 = bundle.getFormattedString("clientAuthMessage1", [org]);
    var message2 = bundle.getFormattedString("clientAuthMessage2", [issuer]);
    setText("hostname", cn);
    setText("organization", message1);
    setText("issuer", message2);

    var selectElement = document.getElementById("nicknames");
    itemCount = dialogParams.GetInt(0);
    for (let i = 0; i < itemCount; i++) {
        var menuItemNode = document.createElement("menuitem");
        let nick = dialogParams.GetString(i + 3);
        menuItemNode.setAttribute("value", i);
        menuItemNode.setAttribute("label", nick); // this is displayed
        selectElement.firstChild.appendChild(menuItemNode);
        if (i == 0) {
            selectElement.selectedItem = menuItemNode;
        }
    }

    setDetails();
}

function setDetails()
{
  let index = parseInt(document.getElementById("nicknames").value);
  let details = dialogParams.GetString(index + itemCount + 3);
  document.getElementById("details").value = details;
}

function onCertSelected()
{
  setDetails();
}

function doOK()
{
  // Signal that the user accepted.
  dialogParams.SetInt(0, 1);
  let index = parseInt(document.getElementById("nicknames").value);
  // Signal the index of the selected cert in the list of cert nicknames
  // provided.
  dialogParams.SetInt(1, index);
  // Signal whether the user wanted to remember the selection.
  dialogParams.SetInt(2, rememberBox.checked);
  return true;
}

function doCancel()
{
  // Signal that the user cancelled.
  dialogParams.SetInt(0, 0);
  // Signal some invalid index value since a cert hasn't actually been chosen.
  dialogParams.SetInt(1, -1); // invalid value
  // Signal whether the user wanted to remember the "selection".
  dialogParams.SetInt(2, rememberBox.checked);
  return true;
}
