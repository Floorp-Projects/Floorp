/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): Masaki Katakai <katakai@japan.sun.com>
 */

var dialog;
var prefs = null;

var default_command = "lpr";
var default_file = "mozilla.ps";

function initDialog()
{
  dialog = new Object;

  dialog.findKey         = document.getElementById("dialog.findKey");
  dialog.caseSensitive   = document.getElementById("dialog.caseSensitive");
  dialog.wrap            = document.getElementById("dialog.wrap");
  dialog.searchBackwards = document.getElementById("dialog.searchBackwards");
  dialog.find            = document.getElementById("ok");
  dialog.cancel          = document.getElementById("cancel");

  dialog.fileLabel       = document.getElementById("fileLabel");
  dialog.cmdLabel        = document.getElementById("cmdLabel");

  dialog.fileRadio       = document.getElementById("fileRadio");
  dialog.printerRadio    = document.getElementById("printerRadio");

  dialog.firstRadio      = document.getElementById("firstRadio");
  dialog.lastRadio       = document.getElementById("lastRadio");

  dialog.colorRadio      = document.getElementById("colorRadio");
  dialog.grayRadio       = document.getElementById("grayRadio");

  dialog.a4Radio         = document.getElementById("a4Radio");
  dialog.letterRadio     = document.getElementById("letterRadio");
  dialog.legalRadio      = document.getElementById("legalRadio");
  dialog.exectiveRadio   = document.getElementById("exectiveRadio");

  dialog.topInput        = document.getElementById("topInput");
  dialog.bottomInput     = document.getElementById("bottomInput");
  dialog.leftInput       = document.getElementById("leftInput");
  dialog.rightInput      = document.getElementById("rightInput");

  dialog.cmdInput        = document.getElementById("cmdInput");
  dialog.fileInput       = document.getElementById("fileInput");
  dialog.chooseButton    = document.getElementById("chooseFile");

  dialog.print           = document.getElementById("ok");

  dialog.enabled         = false;
}

function checkValid(elementID)
{
  var editField = document.getElementById( elementID );
  if ( !editField )
    return;
  var stringIn = editField.value;
  if (stringIn && stringIn.length > 0)
  {
    stringIn = stringIn.replace(/[^\.|^0-9]/g,"");
    if (!stringIn) stringIn = "";
    editField.value = stringIn;
  }
}

function doPrintToFile( value )
{
  if (value == true ) {
    dialog.fileLabel.removeAttribute("disabled");
    dialog.cmdLabel.setAttribute("disabled","true" );
    dialog.fileInput.removeAttribute("disabled");
    dialog.chooseButton.removeAttribute("disabled");
    dialog.cmdInput.setAttribute("disabled","true" );
  } else {
    dialog.cmdLabel.removeAttribute("disabled");
    dialog.fileLabel.setAttribute("disabled","true" );
    dialog.fileInput.setAttribute("disabled","true" );
    dialog.chooseButton.setAttribute("disabled","true" );
    dialog.cmdInput.removeAttribute("disabled");
  }
}

function loadDialog()
{
  var print_tofile = false;
  var print_reversed = false;
  var print_color = true;
  var print_landscape = true;
  var print_paper_size = 0;
  var print_margin_top = 500;
  var print_margin_left = 500;
  var print_margin_bottom = 0;
  var print_margin_right = 0;
  var print_command = default_command;
  var print_file = default_file;

  try {
    prefs = Components.classes["@mozilla.org/preferences;1"];
    if (prefs) {
      prefs = prefs.getService();
      if (prefs)
        prefs = prefs.QueryInterface(Components.interfaces.nsIPref);
    }
  } catch(e) { }

  if (prefs) {
    try { print_tofile = prefs.GetBoolPref("print.print_tofile"); } catch(e) { }
    try { print_reversed = prefs.GetBoolPref("print.print_reversed"); } catch(e) { }
    try { print_color = prefs.GetBoolPref("print.print_color"); } catch(e) { }
    try { print_paper_size = prefs.GetIntPref("print.print_paper_size"); } catch(e) { }
    try { print_margin_top = prefs.GetIntPref("print.print_margin_top"); } catch(e) { }
    try { print_margin_left = prefs.GetIntPref("print.print_margin_left"); } catch(e) { }
    try { print_margin_bottom = prefs.GetIntPref("print.print_margin_bottom"); } catch(e) { }
    try { print_margin_right = prefs.GetIntPref("print.print_margin_right"); } catch(e) { }
    try { print_command = prefs.CopyCharPref("print.print_command"); } catch(e) { }
    try { print_file = prefs.CopyCharPref("print.print_file"); } catch(e) { }
  }

  if ( print_tofile == true) {
    dialog.fileRadio.checked = true;
    doPrintToFile( true );
  } else {
    dialog.printerRadio.checked = true;
    doPrintToFile( false );
  }

  if ( print_color == true) {
    dialog.colorRadio.checked = true;
  } else {
    dialog.grayRadio.checked = true;
  }

  if ( print_reversed == true) {
    dialog.lastRadio.checked = true;
  } else {
    dialog.firstRadio.checked = true;
  }

  if ( print_paper_size == 0 ) {
    dialog.letterRadio.checked = true;
  } else if ( print_paper_size == 1 ) {
    dialog.legalRadio.checked = true;
  } else if ( print_paper_size == 2 ) {
    dialog.exectiveRadio.checked = true;
  } else if ( print_paper_size == 3 ) {
    dialog.a4Radio.checked = true;
  }

  dialog.topInput.value = print_margin_top * 0.001;
  dialog.bottomInput.value = print_margin_bottom * 0.001;
  dialog.leftInput.value = print_margin_left * 0.001;
  dialog.rightInput.value = print_margin_right * 0.001;

  dialog.cmdInput.value = print_command;
  dialog.fileInput.value = print_file;

  dialog.print.setAttribute("value",
             document.getElementById("printButton").getAttribute("value"));
}

var param;

function onLoad()
{
  // Init dialog.
  initDialog();

  // setup the dialogOverlay.xul button handlers
  doSetOKCancel(onOK, onCancel);

  param = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
  if( !param ) {
    return;
  }

  param.SetInt(0, 1 );

  loadDialog();
}

function onUnload()
{
}

function onOK()
{
  var print_paper_size = 0;

  if (prefs) {

    if (dialog.fileRadio.checked == true) {
      prefs.SetBoolPref("print.print_tofile", true);
    } else {
      prefs.SetBoolPref("print.print_tofile", false);
    }

    if (dialog.lastRadio.checked == true) {
      prefs.SetBoolPref("print.print_reversed", true);
    } else {
      prefs.SetBoolPref("print.print_reversed", false);
    }

    if (dialog.colorRadio.checked == true) {
      prefs.SetBoolPref("print.print_color", true);
    } else {
      prefs.SetBoolPref("print.print_color", false);
    }

    if (dialog.letterRadio.checked == true) {
      print_paper_size = 0;
    } else if (dialog.legalRadio.checked == true) {
      print_paper_size = 1;
    } else if (dialog.exectiveRadio.checked == true) {
      print_paper_size = 2;
    } else if (dialog.a4Radio.checked == true) {
      print_paper_size = 3;
    }
    prefs.SetIntPref("print.print_paper_size", print_paper_size);

    prefs.SetIntPref("print.print_margin_top", dialog.topInput.value * 1000);
    prefs.SetIntPref("print.print_margin_left", dialog.leftInput.value * 1000);
    prefs.SetIntPref("print.print_margin_bottom", dialog.bottomInput.value *1000);
    prefs.SetIntPref("print.print_margin_right", dialog.rightInput.value * 1000);

    prefs.SetCharPref("print.print_command", dialog.cmdInput.value);
    prefs.SetCharPref("print.print_file", dialog.fileInput.value);
  }

  if (param) {
    param.SetInt(0, 0 );
  }

  return true;
}

function onCancel()
{
  if (param) {
    param.SetInt(0, 1 );
  }
  return true;
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;
function onChooseFile()
{
  if (dialog.fileRadio.checked == false) {
    return;
  }
  try {
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, document.getElementById("fpDialog").getAttribute("value"), nsIFilePicker.modeSave);
    fp.appendFilters(nsIFilePicker.filterAll);
    fp.show();
    if (fp.file && fp.file.path.length > 0) {
      dialog.fileInput.value = fp.file.path;
    }
  } catch(ex) {
    dump(ex);
  }
}
