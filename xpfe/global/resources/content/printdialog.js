/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Masaki Katakai <katakai@japan.sun.com>
 *   Jessica Blanco <jblanco@us.ibm.com>
 *   Asko Tontti <atontti@cc.hut.fi>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Peter Weilbacher <mozilla@weilbacher.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

var dialog;
var printService       = null;
var gOriginalNumCopies = 1;

var paramBlock;
var gPrefs             = null;
var gPrintSettings     = null;
var gWebBrowserPrint   = null;
var gPrintSetInterface = Components.interfaces.nsIPrintSettings;
var doDebug            = false;

//---------------------------------------------------
function initDialog()
{
  dialog = new Object;

  dialog.propertiesButton = document.getElementById("properties");
  dialog.descText         = document.getElementById("descText");

  dialog.printRangeGroup = document.getElementById("printRangeGroup");
  dialog.allPagesRadio   = document.getElementById("allPagesRadio");
  dialog.rangeRadio      = document.getElementById("rangeRadio");
  dialog.selectionRadio  = document.getElementById("selectionRadio");
  dialog.fromPageInput   = document.getElementById("fromPageInput");
  dialog.fromPageLabel   = document.getElementById("fromPageLabel");
  dialog.toPageInput     = document.getElementById("toPageInput");
  dialog.toPageLabel     = document.getElementById("toPageLabel");

  dialog.numCopiesInput  = document.getElementById("numCopiesInput");  

  dialog.printFrameGroup      = document.getElementById("printFrameGroup");
  dialog.asLaidOutRadio       = document.getElementById("asLaidOutRadio");
  dialog.selectedFrameRadio   = document.getElementById("selectedFrameRadio");
  dialog.eachFrameSepRadio    = document.getElementById("eachFrameSepRadio");
  dialog.printFrameGroupLabel = document.getElementById("printFrameGroupLabel");

  dialog.fileCheck       = document.getElementById("fileCheck");
  dialog.printerLabel    = document.getElementById("printerLabel");
  dialog.printerList     = document.getElementById("printerList");

  dialog.printButton     = document.documentElement.getButton("accept");

  // <data> element
  dialog.fpDialog        = document.getElementById("fpDialog");

  dialog.enabled         = false;
}

//---------------------------------------------------
function checkInteger(element)
{
  var value = element.value;
  if (value && value.length > 0) {
    value = value.replace(/[^0-9]/g,"");
    if (!value) value = "";
    element.value = value;
  }
  if (!value || value < 1 || value > 999)
    dialog.printButton.setAttribute("disabled","true");
  else
    dialog.printButton.removeAttribute("disabled");
}

//---------------------------------------------------
function stripTrailingWhitespace(element)
{
  var value = element.value;
  value = value.replace(/\s+$/,"");
  element.value = value;
}

//---------------------------------------------------
function getPrinterDescription(printerName)
{
  var s = "";

  try {
    /* This may not work with non-ASCII test (see bug 235763 comment #16) */
    s = gPrefs.getCharPref("print.printer_" + printerName + ".printer_description")
  } catch(e) {
  }
    
  return s;
}

//---------------------------------------------------
function listElement(aListElement)
  {
    this.listElement = aListElement;
  }

listElement.prototype =
  {
    clearList:
      function ()
        {
          // remove the menupopup node child of the menulist.
          var popup = this.listElement.firstChild;
          if (popup) {
            this.listElement.removeChild(popup);
          }
        },

    appendPrinterNames: 
      function (aDataObject) 
        { 
          if ((null == aDataObject) || !aDataObject.hasMore()) {
            // disable dialog
            var stringBundle = srGetStrBundle("chrome://global/locale/printing.properties");
            this.listElement.setAttribute("value", "");
            this.listElement.setAttribute("label", stringBundle.GetStringFromName("noprinter"));

            this.listElement.setAttribute("disabled", "true");
            dialog.printerLabel.setAttribute("disabled","true");
            dialog.propertiesButton.setAttribute("disabled","true");
            dialog.fileCheck.setAttribute("disabled","true");
            dialog.printButton.setAttribute("disabled","true");
          }
          else {
            // build popup menu from printer names
            var list = document.getElementById("printerList"); 
            do {
              printerNameStr = aDataObject.getNext();
              list.appendItem(printerNameStr, printerNameStr, getPrinterDescription(printerNameStr));
            } while (aDataObject.hasMore());
            this.listElement.removeAttribute("disabled");
          }
        } 
  };

//---------------------------------------------------
function getPrinters()
{
  var selectElement = new listElement(dialog.printerList);
  selectElement.clearList();

  var printerEnumerator;
  try {
    printerEnumerator =
        Components.classes["@mozilla.org/gfx/printerenumerator;1"]
                  .getService(Components.interfaces.nsIPrinterEnumerator)
                  .printerNameList;
  } catch(e) { printerEnumerator = null; }

  selectElement.appendPrinterNames(printerEnumerator);
  selectElement.listElement.value = printService.defaultPrinterName;

  // make sure we load the prefs for the initially selected printer
  setPrinterDefaultsForSelectedPrinter();
}


//---------------------------------------------------
// update gPrintSettings with the defaults for the selected printer
function setPrinterDefaultsForSelectedPrinter()
{
  gPrintSettings.printerName = dialog.printerList.value;
  
  dialog.descText.value = getPrinterDescription(gPrintSettings.printerName);
  
  // First get any defaults from the printer 
  printService.initPrintSettingsFromPrinter(gPrintSettings.printerName, gPrintSettings);
  
  // now augment them with any values from last time
  printService.initPrintSettingsFromPrefs(gPrintSettings, true, gPrintSetInterface.kInitSaveAll);
  
  if (doDebug) {
    dump("setPrinterDefaultsForSelectedPrinter: printerName='"+gPrintSettings.printerName+"', paperName='"+gPrintSettings.paperName+"'\n");
  }
}

//---------------------------------------------------
function displayPropertiesDialog()
{
  gPrintSettings.numCopies = dialog.numCopiesInput.value;
  try {
    var printingPromptService = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                                                 .getService(Components.interfaces.nsIPrintingPromptService);
    if (printingPromptService) {
      printingPromptService.showPrinterProperties(null, dialog.printerList.value, gPrintSettings);
      dialog.numCopiesInput.value = gPrintSettings.numCopies;
    }
  } catch(e) {
    dump("problems getting printingPromptService\n");
  }
}

//---------------------------------------------------
function doPrintRange(inx)
{
  if (inx == 1) {
    dialog.fromPageInput.removeAttribute("disabled");
    dialog.fromPageLabel.removeAttribute("disabled");
    dialog.toPageInput.removeAttribute("disabled");
    dialog.toPageLabel.removeAttribute("disabled");
  } else {
    dialog.fromPageInput.setAttribute("disabled","true");
    dialog.fromPageLabel.setAttribute("disabled","true");
    dialog.toPageInput.setAttribute("disabled","true");
    dialog.toPageLabel.setAttribute("disabled","true");
  }
}

//---------------------------------------------------
function loadDialog()
{
  var print_copies        = 1;
  var print_selection_radio_enabled = false;
  var print_frametype     = gPrintSetInterface.kSelectedFrame;
  var print_howToEnableUI = gPrintSetInterface.kFrameEnableNone;
  var print_tofile        = "";

  try {
    gPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

    printService = Components.classes["@mozilla.org/gfx/printsettings-service;1"];
    if (printService) {
      printService = printService.getService();
      if (printService) {
        printService = printService.QueryInterface(Components.interfaces.nsIPrintSettingsService);
      }
    }
  } catch(e) {}

  // Note: getPrinters sets up the PrintToFile control
  getPrinters();

  if (gPrintSettings) {
    print_tofile        = gPrintSettings.printToFile;
    gOriginalNumCopies  = gPrintSettings.numCopies;

    print_copies        = gPrintSettings.numCopies;
    print_frametype     = gPrintSettings.printFrameType;
    print_howToEnableUI = gPrintSettings.howToEnableFrameUI;
    print_selection_radio_enabled = gPrintSettings.GetPrintOptions(gPrintSetInterface.kEnableSelectionRB);
  }

  if (doDebug) {
    dump("loadDialog*********************************************\n");
    dump("print_tofile            "+print_tofile+"\n");
    dump("print_frame             "+print_frametype+"\n");
    dump("print_howToEnableUI     "+print_howToEnableUI+"\n");
    dump("selection_radio_enabled "+print_selection_radio_enabled+"\n");
  }

  dialog.printRangeGroup.selectedItem = dialog.allPagesRadio;
  if (print_selection_radio_enabled) {
    dialog.selectionRadio.removeAttribute("disabled");
  } else {
    dialog.selectionRadio.setAttribute("disabled","true");
  }
  doPrintRange(dialog.rangeRadio.selected);
  dialog.fromPageInput.value  = 1;
  dialog.toPageInput.value    = 1;
  dialog.numCopiesInput.value = print_copies;

  if (doDebug) {
    dump("print_howToEnableUI: "+print_howToEnableUI+"\n");
  }

  // print frame
  if (print_howToEnableUI == gPrintSetInterface.kFrameEnableAll) {
    dialog.asLaidOutRadio.removeAttribute("disabled");

    dialog.selectedFrameRadio.removeAttribute("disabled");
    dialog.eachFrameSepRadio.removeAttribute("disabled");
    dialog.printFrameGroupLabel.removeAttribute("disabled");

    // initialize radio group
    dialog.printFrameGroup.selectedItem = dialog.selectedFrameRadio;

  } else if (print_howToEnableUI == gPrintSetInterface.kFrameEnableAsIsAndEach) {
    dialog.asLaidOutRadio.removeAttribute("disabled");       //enable

    dialog.selectedFrameRadio.setAttribute("disabled","true"); // disable
    dialog.eachFrameSepRadio.removeAttribute("disabled");       // enable
    dialog.printFrameGroupLabel.removeAttribute("disabled");    // enable

    // initialize
    dialog.printFrameGroup.selectedItem = dialog.eachFrameSepRadio;

  } else {
    dialog.asLaidOutRadio.setAttribute("disabled","true");
    dialog.selectedFrameRadio.setAttribute("disabled","true");
    dialog.eachFrameSepRadio.setAttribute("disabled","true");
    dialog.printFrameGroupLabel.setAttribute("disabled","true");
  }
}

//---------------------------------------------------
function onLoad()
{
  // Init dialog.
  initDialog();

  // param[0]: nsIPrintSettings object
  // param[1]: container for return value (1 = print, 0 = cancel)

  gPrintSettings   = window.arguments[0].QueryInterface(gPrintSetInterface);
  gWebBrowserPrint = window.arguments[1].QueryInterface(Components.interfaces.nsIWebBrowserPrint);
  paramBlock       = window.arguments[2].QueryInterface(Components.interfaces.nsIDialogParamBlock);

  // default return value is "cancel"
  paramBlock.SetInt(0, 0);

  loadDialog();
}

//---------------------------------------------------
function onAccept()
{
  if (gPrintSettings != null) {
    var print_howToEnableUI = gPrintSetInterface.kFrameEnableNone;

    // save these out so they can be picked up by the device spec
    gPrintSettings.printerName = dialog.printerList.value;
    print_howToEnableUI        = gPrintSettings.howToEnableFrameUI;
    gPrintSettings.printToFile = dialog.fileCheck.checked;

    if (gPrintSettings.printToFile)
      if (!chooseFile())
        return false;

    if (dialog.allPagesRadio.selected) {
      gPrintSettings.printRange = gPrintSetInterface.kRangeAllPages;
    } else if (dialog.rangeRadio.selected) {
      gPrintSettings.printRange = gPrintSetInterface.kRangeSpecifiedPageRange;
    } else if (dialog.selectionRadio.selected) {
      gPrintSettings.printRange = gPrintSetInterface.kRangeSelection;
    }
    gPrintSettings.startPageRange = dialog.fromPageInput.value;
    gPrintSettings.endPageRange   = dialog.toPageInput.value;
    gPrintSettings.numCopies      = dialog.numCopiesInput.value;

    var frametype = gPrintSetInterface.kNoFrames;
    if (print_howToEnableUI != gPrintSetInterface.kFrameEnableNone) {
      if (dialog.asLaidOutRadio.selected) {
        frametype = gPrintSetInterface.kFramesAsIs;
      } else if (dialog.selectedFrameRadio.selected) {
        frametype = gPrintSetInterface.kSelectedFrame;
      } else if (dialog.eachFrameSepRadio.selected) {
        frametype = gPrintSetInterface.kEachFrameSep;
      } else {
        frametype = gPrintSetInterface.kSelectedFrame;
      }
    }
    gPrintSettings.printFrameType = frametype;
    if (doDebug) {
      dump("onAccept*********************************************\n");
      dump("frametype      "+frametype+"\n");
      dump("numCopies      "+gPrintSettings.numCopies+"\n");
      dump("printRange     "+gPrintSettings.printRange+"\n");
      dump("printerName    "+gPrintSettings.printerName+"\n");
      dump("startPageRange "+gPrintSettings.startPageRange+"\n");
      dump("endPageRange   "+gPrintSettings.endPageRange+"\n");
      dump("printToFile    "+gPrintSettings.printToFile+"\n");
    }
  }

  var saveToPrefs = false;

  saveToPrefs = gPrefs.getBoolPref("print.save_print_settings");

  if (saveToPrefs && printService != null) {
    var flags = gPrintSetInterface.kInitSavePaperSize      | 
                gPrintSetInterface.kInitSaveColorSpace     |
                gPrintSetInterface.kInitSaveEdges          |
                gPrintSetInterface.kInitSaveInColor        |
                gPrintSetInterface.kInitSaveResolutionName |
                gPrintSetInterface.kInitSaveDownloadFonts  |
                gPrintSetInterface.kInitSavePrintCommand   |
                gPrintSetInterface.kInitSaveShrinkToFit    |
                gPrintSetInterface.kInitSaveScaling;
    printService.savePrintSettingsToPrefs(gPrintSettings, true, flags);
  }

  // set return value to "print"
  if (paramBlock) {
    paramBlock.SetInt(0, 1);
  } else {
    dump("*** FATAL ERROR: No paramBlock\n");
  }

  return true;
}

//---------------------------------------------------
function onCancel()
{
  // set return value to "cancel"
  if (paramBlock) {
    paramBlock.SetInt(0, 0);
  } else {
    dump("*** FATAL ERROR: No paramBlock\n");
  }

  return true;
}

//---------------------------------------------------
const nsIFilePicker = Components.interfaces.nsIFilePicker;
function chooseFile()
{
  try {
    var fp = Components.classes["@mozilla.org/filepicker;1"]
             .createInstance(nsIFilePicker);
    fp.init(window, dialog.fpDialog.getAttribute("label"), nsIFilePicker.modeSave);
    fp.appendFilters(nsIFilePicker.filterAll);
    if (fp.show() != Components.interfaces.nsIFilePicker.returnCancel &&
        fp.file && fp.file.path) {
      gPrintSettings.toFileName = fp.file.path;
      return true;
    }
  } catch(ex) {
    dump(ex);
  }

  return false;
}

