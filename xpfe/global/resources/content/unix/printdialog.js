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
 * Contributor(s): Masaki Katakai <katakai@japan.sun.com>
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
var default_file       = "mozilla.ps";
var gPrintOptInterface = Components.interfaces.nsIPrintOptions;
var doDebug            = false;

//---------------------------------------------------
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
  dialog.a3Radio         = document.getElementById("a3Radio");
  dialog.letterRadio     = document.getElementById("letterRadio");
  dialog.legalRadio      = document.getElementById("legalRadio");
  dialog.exectiveRadio   = document.getElementById("exectiveRadio");
  
  dialog.portraitRadio   = document.getElementById("portraitRadio");
  dialog.landscapeRadio   = document.getElementById("landscapeRadio");

  dialog.allpagesRadio   = document.getElementById("allpagesRadio");
  dialog.rangeRadio      = document.getElementById("rangeRadio");
  dialog.selectionRadio  = document.getElementById("selectionRadio");
  dialog.frompageInput   = document.getElementById("frompageInput");
  dialog.frompageLabel   = document.getElementById("frompageLabel");
  dialog.topageInput     = document.getElementById("topageInput");
  dialog.topageLabel     = document.getElementById("topageLabel");

  dialog.aslaidoutRadio       = document.getElementById("aslaidoutRadio");
  dialog.selectedframeRadio   = document.getElementById("selectedframeRadio");
  dialog.eachframesepRadio    = document.getElementById("eachframesepRadio");
  dialog.printrangeGroupLabel = document.getElementById("printrangeGroupLabel");

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
function doPrintToFile( value )
{
  if (value ) {
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

//---------------------------------------------------
function doPrintRange( inx )
{
  if ( inx == 1 ) {
    dialog.frompageInput.removeAttribute("disabled");
    dialog.frompageLabel.removeAttribute("disabled");
    dialog.topageInput.removeAttribute("disabled");
    dialog.topageLabel.removeAttribute("disabled");
  } else {
    dialog.frompageInput.setAttribute("disabled","true" );
    dialog.frompageLabel.setAttribute("disabled","true" );
    dialog.topageInput.setAttribute("disabled","true" );
    dialog.topageLabel.setAttribute("disabled","true" );
  }
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
  var print_tofile        = false;
  var print_reversed      = false;
  var print_color         = true;
  var print_paper_size    = 0;
  var print_orientation   = 0;
  var print_margin_top    = 0.5;
  var print_margin_left   = 0.5;
  var print_margin_bottom = 0.5;
  var print_margin_right  = 0.5;
  var print_command       = default_command;
  var print_file          = default_file;
  var print_selection_radio_enabled = false;
  var print_frametype     = gPrintOptInterface.kSelectedFrame;
  var print_howToEnableUI = gPrintOptInterface.kFrameEnableNone;

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
    print_file      = printService.toFileName;
    print_tofile    = printService.printToFile;
    print_frametype = printService.printFrameType;
    print_howToEnableUI = printService.howToEnableFrameUI;
    print_selection_radio_enabled = printService.GetPrintOptions(gPrintOptInterface.kPrintOptionsEnableSelectionRB);
  }

  if (doDebug) {
    dump("printReversed "+print_reversed+"\n");
    dump("printInColor  "+print_color+"\n");
    dump("paperSize     "+print_paper_size+"\n");
    dump("orientation   "+print_orientation+"\n");
    dump("printCommand  "+print_command+"\n");
    dump("toFileName    "+print_file+"\n");
    dump("printToFile   "+print_tofile+"\n");
    dump("printToFile   "+print_tofile+"\n");
    dump("print_frame   "+print_frametype+"\n");
    dump("print_howToEnableUI "+print_howToEnableUI+"\n");

    dump("selection_radio_enabled "+print_selection_radio_enabled+"\n");

    dump("print_margin_top    "+print_margin_top+"\n");
    dump("print_margin_left   "+print_margin_left+"\n");
    dump("print_margin_right  "+print_margin_right+"\n");
    dump("print_margin_bottom "+print_margin_bottom+"\n");
  }

  if (print_file == "") {
    print_file = default_file;
  }

  if (print_command == "") {
    print_command = default_command;
  }

  if ( print_tofile) {
    dialog.fileRadio.checked = true;
    doPrintToFile( true );
  } else {
    dialog.printerRadio.checked = true;
    doPrintToFile( false );
  }

  if ( print_color) {
    dialog.colorRadio.checked = true;
  } else {
    dialog.grayRadio.checked = true;
  }

  if ( print_reversed) {
    dialog.lastRadio.checked = true;
  } else {
    dialog.firstRadio.checked = true;
  }

  if ( print_paper_size == gPrintOptInterface.kLetterPaperSize ) {
    dialog.letterRadio.checked = true;
  } else if ( print_paper_size == gPrintOptInterface.kLegalPaperSize ) {
    dialog.legalRadio.checked = true;
  } else if ( print_paper_size == gPrintOptInterface.kExecutivePaperSize ) {
    dialog.exectiveRadio.checked = true;
  } else if ( print_paper_size == gPrintOptInterface.kA4PaperSize ) {
    dialog.a4Radio.checked = true;
  } else if ( print_paper_size == gPrintOptInterface.kA3PaperSize ) {
    dialog.a3Radio.checked = true;  
  }

  if ( print_orientation == gPrintOptInterface.kPortraitOrientation ) {
    dialog.portraitRadio.checked = true;
  } else  if ( print_orientation == gPrintOptInterface.kLandscapeOrientation ) {
    dialog.landscapeRadio.checked = true;
  }

  dialog.allpagesRadio.checked = true;
  if ( print_selection_radio_enabled) {
    dialog.selectionRadio.removeAttribute("disabled");
  } else {
    dialog.selectionRadio.setAttribute("disabled","true" );
  }
  doPrintRange(dialog.rangeRadio.checked);
  dialog.frompageInput.value = 1;
  dialog.topageInput.value   = 1;

  dialog.topInput.value    = getDoubleStr(print_margin_top, 1);
  dialog.bottomInput.value = getDoubleStr(print_margin_bottom, 1);
  dialog.leftInput.value   = getDoubleStr(print_margin_left, 1);
  dialog.rightInput.value  = getDoubleStr(print_margin_right, 1);

  dialog.cmdInput.value    = print_command;
  dialog.fileInput.value   = print_file;

  dialog.print.label = document.getElementById("printButton").getAttribute("label");

  if (doDebug) {
    dump("print_howToEnableUI: "+print_howToEnableUI+"\n");
  }

  // print frame
  if (print_howToEnableUI == gPrintOptInterface.kFrameEnableAll) {
    dialog.aslaidoutRadio.removeAttribute("disabled");

    dialog.selectedframeRadio.removeAttribute("disabled");
    dialog.eachframesepRadio.removeAttribute("disabled");
    dialog.printrangeGroupLabel.removeAttribute("disabled");

    // initialize radio group
    dialog.selectedframeRadio.checked = true;

  } else if (print_howToEnableUI == gPrintOptInterface.kFrameEnableAsIsAndEach) {
    dialog.aslaidoutRadio.removeAttribute("disabled");       //enable

    dialog.selectedframeRadio.setAttribute("disabled","true" ); // disable
    dialog.eachframesepRadio.removeAttribute("disabled");       // enable
    dialog.printrangeGroupLabel.removeAttribute("disabled");    // enable

    // initialize
    dialog.eachframesepRadio.checked = true;

  } else {
    dialog.aslaidoutRadio.setAttribute("disabled","true" );
    dialog.selectedframeRadio.setAttribute("disabled","true" );
    dialog.eachframesepRadio.setAttribute("disabled","true" );
    dialog.printrangeGroupLabel.setAttribute("disabled","true" );
  }

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
    var print_howToEnableUI = gPrintOptInterface.kFrameEnableNone;

    printService.printToFile   = dialog.fileRadio.checked;
    printService.printReversed = dialog.lastRadio.checked;
    printService.printInColor  = dialog.colorRadio.checked;
    print_howToEnableUI        = printService.howToEnableFrameUI;

    if (dialog.letterRadio.checked) {
      print_paper_size = gPrintOptInterface.kLetterPaperSize;
    } else if (dialog.legalRadio.checked) {
      print_paper_size = gPrintOptInterface.kLegalPaperSize;
    } else if (dialog.exectiveRadio.checked) {
      print_paper_size = gPrintOptInterface.kExecutivePaperSize;
    } else if (dialog.a4Radio.checked) {
      print_paper_size = gPrintOptInterface.kA4PaperSize;
    } else if (dialog.a3Radio.checked) {
      print_paper_size = gPrintOptInterface.kA3PaperSize;
    }
    printService.paperSize = print_paper_size;
    
    var print_orientation;
    if (dialog.portraitRadio.checked) {
      print_orientation = gPrintOptInterface.kPortraitOrientation;
    } else if (dialog.landscapeRadio.checked) {
      print_orientation = gPrintOptInterface.kLandscapeOrientation;
    }
    printService.orientation = print_orientation;

    // save these out so they can be picked up by the device spec
    printService.marginTop    = dialog.topInput.value;
    printService.marginLeft   = dialog.leftInput.value;
    printService.marginBottom = dialog.bottomInput.value;
    printService.marginRight  = dialog.rightInput.value;

    printService.printCommand = dialog.cmdInput.value;
    printService.toFileName   = dialog.fileInput.value;

    if (dialog.allpagesRadio.checked) {
      printService.printRange = gPrintOptInterface.kRangeAllPages;
    } else if (dialog.rangeRadio.checked) {
      printService.printRange = gPrintOptInterface.kRangeSpecifiedPageRange;
    } else if (dialog.selectionRadio.checked) {
      printService.printRange = gPrintOptInterface.kRangeSelection;
    }
    printService.startPageRange = dialog.frompageInput.value;
    printService.endPageRange   = dialog.topageInput.value;

    var frametype = gPrintOptInterface.kNoFrames;
    if (print_howToEnableUI != gPrintOptInterface.kFrameEnableNone) {
      if (dialog.aslaidoutRadio.checked) {
        frametype = gPrintOptInterface.kFramesAsIs;
      } else if (dialog.selectedframeRadio.checked) {
        frametype = gPrintOptInterface.kSelectedFrame;
      } else if (dialog.eachframesepRadio.checked) {
        frametype = gPrintOptInterface.kEachFrameSep;
      } else {
        frametype = gPrintOptInterface.kSelectedFrame;
      }
    }
    printService.printFrameType = frametype;
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

//---------------------------------------------------
const nsIFilePicker = Components.interfaces.nsIFilePicker;
function onChooseFile()
{
  if (dialog.fileRadio.checked == false) {
    return;
  }
  try {
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, document.getElementById("fpDialog").getAttribute("label"), nsIFilePicker.modeSave);
    fp.appendFilters(nsIFilePicker.filterAll);
    fp.show();
    if (fp.file && fp.file.path.length > 0) {
      dialog.fileInput.value = fp.file.path;
    }
  } catch(ex) {
    dump(ex);
  }
}
