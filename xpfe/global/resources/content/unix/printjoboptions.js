/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *   Masaki Katakai <katakai@japan.sun.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Asko Tontti <atontti@cc.hut.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var dialog;
var gPrintSettings      = null;

var default_command    = "lpr";
var gPrintSetInterface = Components.interfaces.nsIPrintSettings;
var doDebug            = true;

//---------------------------------------------------
function initDialog()
{
  dialog = new Object;

  dialog.cmdLabel        = document.getElementById("cmdLabel");
  dialog.cmdInput        = document.getElementById("cmdInput");

  dialog.colorGroup      = document.getElementById("colorGroup");
  dialog.colorRadio      = document.getElementById("colorRadio");
  dialog.grayRadio       = document.getElementById("grayRadio");
}

//---------------------------------------------------
function loadDialog()
{
  var print_color         = true;
  var print_command       = default_command;

  if (gPrintSettings) {
    print_color     = gPrintSettings.printInColor;
    print_command   = gPrintSettings.printCommand;
  }

  if (doDebug) {
    dump("loadDialog******************************\n");
    dump("printInColor  "+print_color+"\n");
    dump("printCommand  "+print_command+"\n");
  }

  if (print_command == "") {
    print_command = default_command;
  }

  if (print_color) {
    dialog.colorGroup.selectedItem = dialog.colorRadio;
  } else {
    dialog.colorGroup.selectedItem = dialog.grayRadio;
  }

  dialog.cmdInput.value    = print_command;
}

var param;

//---------------------------------------------------
function onLoad()
{
  // Init dialog.
  initDialog();

  // window.arguments[0]: container for return value (1 = ok, 0 = cancel)
  var ps = window.arguments[0].QueryInterface(gPrintSetInterface);
  if (ps != null) {
    gPrintSettings = ps;
    paramBlock = window.arguments[1].QueryInterface(Components.interfaces.nsIDialogParamBlock);
  }

  if (doDebug) {
    if (ps == null) alert("PrintSettings is null!");
    if (paramBlock == null) alert("nsIDialogParam is null!");
  }

  // default return value is "cancel"
  paramBlock.SetInt(0, 0);

  loadDialog();
}

//---------------------------------------------------
function onAccept()
{
  if (gPrintSettings != null) {
    // save these out so they can be picked up by the device spec
    gPrintSettings.printInColor = dialog.colorRadio.selected;
    gPrintSettings.printCommand = dialog.cmdInput.value;
    if (doDebug) {
      dump("onAccept******************************\n");
      dump("printInColor  "+gPrintSettings.printInColor+"\n");
      dump("printCommand  "+gPrintSettings.printCommand+"\n");
    }
  } else {
    dump("************ onAccept gPrintSettings: "+gPrintSettings+"\n");
  }

  if (param) {
    // set return value to "ok"
    param.SetInt(0, 1);
  } else {
    dump("*** FATAL ERROR: printService missing\n");
  }


  return true;
}
