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
var printService       = null;

var default_command    = "lpr";
var gPrintOptInterface = Components.interfaces.nsIPrintOptions;
var doDebug            = false;

//---------------------------------------------------
function initDialog()
{
  dialog = new Object;

  dialog.cmdLabel        = document.getElementById("cmdLabel");

  dialog.reverseGroup    = document.getElementById("reverseGroup");
  dialog.firstRadio      = document.getElementById("firstRadio");
  dialog.lastRadio       = document.getElementById("lastRadio");

  dialog.colorGroup      = document.getElementById("colorGroup");
  dialog.colorRadio      = document.getElementById("colorRadio");
  dialog.grayRadio       = document.getElementById("grayRadio");

  dialog.paperGroup      = document.getElementById("paperGroup");
  dialog.a4Radio         = document.getElementById("a4Radio");
  dialog.a3Radio         = document.getElementById("a3Radio");
  dialog.letterRadio     = document.getElementById("letterRadio");
  dialog.legalRadio      = document.getElementById("legalRadio");
  dialog.exectiveRadio   = document.getElementById("exectiveRadio");
  
  dialog.orientationGroup = document.getElementById("orientationGroup");
  dialog.portraitRadio    = document.getElementById("portraitRadio");
  dialog.landscapeRadio   = document.getElementById("landscapeRadio");

  dialog.topInput        = document.getElementById("topInput");
  dialog.bottomInput     = document.getElementById("bottomInput");
  dialog.leftInput       = document.getElementById("leftInput");
  dialog.rightInput      = document.getElementById("rightInput");

  dialog.cmdInput        = document.getElementById("cmdInput");

  dialog.enabled         = false;
}

//---------------------------------------------------
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

//---------------------------------------------------
function stripTrailingWhitespace(element)
{
  var stringIn = element.value;
  stringIn = stringIn.replace(/\s+$/,"");
  element.value = stringIn;
}

//---------------------------------------------------
function getDoubleStr( val, dec )
{
  var str = val.toString();
  var inx = str.indexOf(".");
  return str.substring(0, inx+dec+1);
}

//---------------------------------------------------
function loadDialog()
{
  var print_reversed      = false;
  var print_color         = true;
  var print_paper_size    = 0;
  var print_orientation   = 0;
  var print_margin_top    = 0.5;
  var print_margin_left   = 0.5;
  var print_margin_bottom = 0.5;
  var print_margin_right  = 0.5;
  var print_command       = default_command;

  try {
    printService = Components.classes["@mozilla.org/gfx/printoptions;1"];
    if (printService) {
      printService = printService.getService();
      if (printService) {
        printService = printService.QueryInterface(Components.interfaces.nsIPrintOptions);
      }
    }
  } catch(e) {}

  if (printService) {
    print_reversed   = printService.printReversed;
    print_color      = printService.printInColor;
    print_paper_size = printService.paperSize;
    print_orientation = printService.orientation;

    print_margin_top    = printService.marginTop;
    print_margin_left   = printService.marginLeft;
    print_margin_right  = printService.marginRight;
    print_margin_bottom = printService.marginBottom;

    print_command   = printService.printCommand;
  }

  if (doDebug) {
    dump("printReversed "+print_reversed+"\n");
    dump("printInColor  "+print_color+"\n");
    dump("paperSize     "+print_paper_size+"\n");
    dump("orientation   "+print_orientation+"\n");
    dump("printCommand  "+print_command+"\n");

    dump("print_margin_top    "+print_margin_top+"\n");
    dump("print_margin_left   "+print_margin_left+"\n");
    dump("print_margin_right  "+print_margin_right+"\n");
    dump("print_margin_bottom "+print_margin_bottom+"\n");
  }

  if (print_command == "") {
    print_command = default_command;
  }

  if ( print_color) {
    dialog.colorGroup.selectedItem = dialog.colorRadio;
  } else {
    dialog.colorGroup.selectedItem = dialog.grayRadio;
  }

  if ( print_reversed) {
    dialog.reverseGroup.selectedItem = dialog.lastRadio;
  } else {
    dialog.reverseGroup.selectedItem = dialog.firstRadio;
  }

  if ( print_paper_size == gPrintOptInterface.kLetterPaperSize ) {
    dialog.paperGroup.selectedItem = dialog.letterRadio;
  } else if ( print_paper_size == gPrintOptInterface.kLegalPaperSize ) {
    dialog.paperGroup.selectedItem = dialog.legalRadio;
  } else if ( print_paper_size == gPrintOptInterface.kExecutivePaperSize ) {
    dialog.paperGroup.selectedItem = dialog.exectiveRadio;
  } else if ( print_paper_size == gPrintOptInterface.kA4PaperSize ) {
    dialog.paperGroup.selectedItem = dialog.a4Radio;
  } else if ( print_paper_size == gPrintOptInterface.kA3PaperSize ) {
    dialog.paperGroup.selectedItem = dialog.a3Radio;
  }

  if ( print_orientation == gPrintOptInterface.kPortraitOrientation ) {
    dialog.orientationGroup.selectedItem = dialog.portraitRadio;
  } else  if ( print_orientation == gPrintOptInterface.kLandscapeOrientation ) {
    dialog.orientationGroup.selectedItem = dialog.landscapeRadio;
  }

  dialog.topInput.value    = getDoubleStr(print_margin_top, 1);
  dialog.bottomInput.value = getDoubleStr(print_margin_bottom, 1);
  dialog.leftInput.value   = getDoubleStr(print_margin_left, 1);
  dialog.rightInput.value  = getDoubleStr(print_margin_right, 1);

  dialog.cmdInput.value    = print_command;
}

var param;

//---------------------------------------------------
function onLoad()
{
  // Init dialog.
  initDialog();

  param = window.arguments[0].QueryInterface(Components.interfaces.nsIDialogParamBlock);
  if( !param ) {
    return;
  }

  param.SetInt(0, 1 );

  loadDialog();
}

//---------------------------------------------------
function onAccept()
{
  var print_paper_size = 0;

  if (printService) {
    printService.printReversed = dialog.lastRadio.selected;
    printService.printInColor  = dialog.colorRadio.selected;

    if (dialog.letterRadio.selected) {
      print_paper_size = gPrintOptInterface.kLetterPaperSize;
    } else if (dialog.legalRadio.selected) {
      print_paper_size = gPrintOptInterface.kLegalPaperSize;
    } else if (dialog.exectiveRadio.selected) {
      print_paper_size = gPrintOptInterface.kExecutivePaperSize;
    } else if (dialog.a4Radio.selected) {
      print_paper_size = gPrintOptInterface.kA4PaperSize;
    } else if (dialog.a3Radio.selected) {
      print_paper_size = gPrintOptInterface.kA3PaperSize;
    }
    printService.paperSize = print_paper_size;
    
    var print_orientation;
    if (dialog.portraitRadio.selected) {
      print_orientation = gPrintOptInterface.kPortraitOrientation;
    } else if (dialog.landscapeRadio.selected) {
      print_orientation = gPrintOptInterface.kLandscapeOrientation;
    }
    printService.orientation = print_orientation;

    // save these out so they can be picked up by the device spec
    printService.marginTop    = dialog.topInput.value;
    printService.marginLeft   = dialog.leftInput.value;
    printService.marginBottom = dialog.bottomInput.value;
    printService.marginRight  = dialog.rightInput.value;

    printService.printCommand = dialog.cmdInput.value;

  } else {
    dump("************ printService: "+printService+"\n");
  }

  if (param) {
    param.SetInt(0, 0 );
  }

  return true;
}

//---------------------------------------------------
function onCancel()
{
  if (param) {
    param.SetInt(0, 1 );
  }
  return true;
}

