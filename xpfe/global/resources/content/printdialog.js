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
var printOptions       = null;
var gOriginalNumCopies = 1;

var paramBlock;
var gPrefs             = null;
var gPrintSettings     = null;
var gWebBrowserPrint   = null;
var default_file       = "mozilla.ps";
var gPrintSetInterface = Components.interfaces.nsIPrintSettings;
var doDebug            = false;
var gFileFromPicker    = "";

//---------------------------------------------------
function initDialog()
{
  dialog = new Object;

  dialog.propertiesButton = document.getElementById("properties");
  dialog.descText         = document.getElementById("descText");

  dialog.destGroup       = document.getElementById("destGroup");
  dialog.fileRadio       = document.getElementById("fileRadio");
  dialog.printerRadio    = document.getElementById("printerRadio");

  dialog.printrangeGroup = document.getElementById("printrangeGroup");
  dialog.allpagesRadio   = document.getElementById("allpagesRadio");
  dialog.rangeRadio      = document.getElementById("rangeRadio");
  dialog.selectionRadio  = document.getElementById("selectionRadio");
  dialog.frompageInput   = document.getElementById("frompageInput");
  dialog.frompageLabel   = document.getElementById("frompageLabel");
  dialog.topageInput     = document.getElementById("topageInput");
  dialog.topageLabel     = document.getElementById("topageLabel");

  dialog.numCopiesInput  = document.getElementById("numCopiesInput");  

  dialog.printframeGroup      = document.getElementById("printframeGroup");
  dialog.aslaidoutRadio       = document.getElementById("aslaidoutRadio");
  dialog.selectedframeRadio   = document.getElementById("selectedframeRadio");
  dialog.eachframesepRadio    = document.getElementById("eachframesepRadio");
  dialog.printframeGroupLabel = document.getElementById("printframeGroupLabel");

  dialog.fileInput       = document.getElementById("fileInput");
  dialog.fileLabel       = document.getElementById("fileLabel");
  dialog.printerLabel    = document.getElementById("printerLabel");
  dialog.chooseButton    = document.getElementById("chooseFile");
  dialog.printerList     = document.getElementById("printerList");

  dialog.printButton     = document.documentElement.getButton("accept");

  // <data> elements
  dialog.printName       = document.getElementById("printButton");
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
function doEnablePrintToFile(value)
{
  if (value) {
    dialog.fileLabel.removeAttribute("disabled");
    dialog.fileInput.removeAttribute("disabled");
    dialog.chooseButton.removeAttribute("disabled");
  } else {
    dialog.fileLabel.setAttribute("disabled","true");
    dialog.fileInput.setAttribute("disabled","true");
    dialog.chooseButton.setAttribute("disabled","true");
  }
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
          var list = document.getElementById("printerList"); 
          var strDefaultPrinterName = "";
          var printerName;

          // build popup menu from printer names
          while (aDataObject.hasMoreElements()) {
            printerName = aDataObject.getNext();
            printerName = printerName.QueryInterface(Components.interfaces.nsISupportsString);
            var printerNameStr = printerName.toString();
            if (strDefaultPrinterName == "")
               strDefaultPrinterName = printerNameStr;

            list.appendItem(printerNameStr, printerNameStr, getPrinterDescription(printerNameStr));
          }
          if (strDefaultPrinterName != "") {
            this.listElement.removeAttribute("disabled");
          } else {
            var stringBundle = srGetStrBundle("chrome://global/locale/printing.properties");
            this.listElement.setAttribute("value", strDefaultPrinterName);
            this.listElement.setAttribute("label", stringBundle.GetStringFromName("noprinter"));

            // disable dialog
            this.listElement.setAttribute("disabled", "true");
            dialog.destGroup.setAttribute("disabled","true");
            dialog.printerRadio.setAttribute("disabled","true");
            dialog.printerLabel.setAttribute("disabled","true");
            dialog.propertiesButton.setAttribute("disabled","true");
            dialog.fileRadio.setAttribute("disabled","true");
            dialog.printButton.setAttribute("disabled","true");
            doEnablePrintToFile(false);
          }

          return strDefaultPrinterName;
        } 
  };

//---------------------------------------------------
function getPrinters()
{
  var printerEnumerator = printOptions.availablePrinters();

  var selectElement = new listElement(dialog.printerList);
  selectElement.clearList();
  var strDefaultPrinterName = selectElement.appendPrinterNames(printerEnumerator);

  selectElement.listElement.value = strDefaultPrinterName;

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
    dialog.frompageInput.removeAttribute("disabled");
    dialog.frompageLabel.removeAttribute("disabled");
    dialog.topageInput.removeAttribute("disabled");
    dialog.topageLabel.removeAttribute("disabled");
  } else {
    dialog.frompageInput.setAttribute("disabled","true");
    dialog.frompageLabel.setAttribute("disabled","true");
    dialog.topageInput.setAttribute("disabled","true");
    dialog.topageLabel.setAttribute("disabled","true");
  }
}

//---------------------------------------------------
function loadDialog()
{
  var print_copies        = 1;
  var print_file          = default_file;
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
        printOptions = printService.QueryInterface(Components.interfaces.nsIPrintOptions);
      }
    }
  } catch(e) {}

  // Note: getPrinters sets up the PrintToFile radio buttons and initalises gPrintSettings
  getPrinters();

  if (gPrintSettings) {
    print_tofile        = gPrintSettings.printToFile;
    gOriginalNumCopies  = gPrintSettings.numCopies;

    print_copies        = gPrintSettings.numCopies;
    print_file          = gPrintSettings.toFileName;
    print_frametype     = gPrintSettings.printFrameType;
    print_howToEnableUI = gPrintSettings.howToEnableFrameUI;
    print_selection_radio_enabled = gPrintSettings.GetPrintOptions(gPrintSetInterface.kEnableSelectionRB);
  }

  if (print_tofile) {
    dialog.destGroup.selectedItem = dialog.fileRadio;
    doEnablePrintToFile(true);
  } else {
    dialog.destGroup.selectedItem = dialog.printerRadio;
    doEnablePrintToFile(false);
  }

  if (doDebug) {
    dump("loadDialog*********************************************\n");
    dump("toFileName              ["+print_file+"]\n");
    dump("print_tofile            "+print_tofile+"\n");
    dump("print_frame             "+print_frametype+"\n");
    dump("print_howToEnableUI     "+print_howToEnableUI+"\n");
    dump("selection_radio_enabled "+print_selection_radio_enabled+"\n");
  }

  if (print_file == "") {
    print_file = default_file;
  }

  dialog.printrangeGroup.selectedItem = dialog.allpagesRadio;
  if (print_selection_radio_enabled) {
    dialog.selectionRadio.removeAttribute("disabled");
  } else {
    dialog.selectionRadio.setAttribute("disabled","true");
  }
  doPrintRange(dialog.rangeRadio.selected);
  dialog.frompageInput.value  = 1;
  dialog.topageInput.value    = 1;
  dialog.numCopiesInput.value = print_copies;

  dialog.fileInput.value   = print_file;

  if (gPrintSettings.toFileName != "") {
    dialog.fileInput.value = gPrintSettings.toFileName;
  }

  if (doDebug) {
    dump("print_howToEnableUI: "+print_howToEnableUI+"\n");
  }

  // print frame
  if (print_howToEnableUI == gPrintSetInterface.kFrameEnableAll) {
    dialog.aslaidoutRadio.removeAttribute("disabled");

    dialog.selectedframeRadio.removeAttribute("disabled");
    dialog.eachframesepRadio.removeAttribute("disabled");
    dialog.printframeGroupLabel.removeAttribute("disabled");

    // initialize radio group
    dialog.printframeGroup.selectedItem = dialog.selectedframeRadio;

  } else if (print_howToEnableUI == gPrintSetInterface.kFrameEnableAsIsAndEach) {
    dialog.aslaidoutRadio.removeAttribute("disabled");       //enable

    dialog.selectedframeRadio.setAttribute("disabled","true"); // disable
    dialog.eachframesepRadio.removeAttribute("disabled");       // enable
    dialog.printframeGroupLabel.removeAttribute("disabled");    // enable

    // initialize
    dialog.printframeGroup.selectedItem = dialog.eachframesepRadio;

  } else {
    dialog.aslaidoutRadio.setAttribute("disabled","true");
    dialog.selectedframeRadio.setAttribute("disabled","true");
    dialog.eachframesepRadio.setAttribute("disabled","true");
    dialog.printframeGroupLabel.setAttribute("disabled","true");
  }

  dialog.printButton.label = dialog.printName.getAttribute("label");
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
    var stringBundle = srGetStrBundle("chrome://global/locale/printing.properties");
    var promptService = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                        .getService(Components.interfaces.nsIPromptService);

    if (dialog.fileRadio.selected && dialog.fileInput.value == "") {
      var titleText = stringBundle.GetStringFromName("noPrintFilename.title");
      var alertText = stringBundle.GetStringFromName("noPrintFilename.alert");
      promptService.alert(this.window, titleText, alertText);
      return false;
    }

    // save these out so they can be picked up by the device spec
    gPrintSettings.printToFile = dialog.fileRadio.selected;
    print_howToEnableUI        = gPrintSettings.howToEnableFrameUI;

    // save these out so they can be picked up by the device spec
    gPrintSettings.toFileName   = dialog.fileInput.value;
    gPrintSettings.printerName  = dialog.printerList.value;

    if (gPrintSettings.printToFile) {
      if (gPrintSettings.toFileName == "")
        return false;
      var sfile = Components.classes["@mozilla.org/file/local;1"]
                  .createInstance(Components.interfaces.nsILocalFile);
      sfile.initWithPath(gPrintSettings.toFileName);
      if (sfile.exists() &&
        gPrintSettings.toFileName != gFileFromPicker) {
        var desc = stringBundle.formatStringFromName("fileConfirm.exists",
                                                     [gPrintSettings.toFileName],
                                                     1);
        if (!promptService.confirm(this.window, null, desc)) {
          return false;
        }
      }
    }

    if (dialog.allpagesRadio.selected) {
      gPrintSettings.printRange = gPrintSetInterface.kRangeAllPages;
    } else if (dialog.rangeRadio.selected) {
      gPrintSettings.printRange = gPrintSetInterface.kRangeSpecifiedPageRange;
    } else if (dialog.selectionRadio.selected) {
      gPrintSettings.printRange = gPrintSetInterface.kRangeSelection;
    }
    gPrintSettings.startPageRange = dialog.frompageInput.value;
    gPrintSettings.endPageRange   = dialog.topageInput.value;
    gPrintSettings.numCopies      = dialog.numCopiesInput.value;

    var frametype = gPrintSetInterface.kNoFrames;
    if (print_howToEnableUI != gPrintSetInterface.kFrameEnableNone) {
      if (dialog.aslaidoutRadio.selected) {
        frametype = gPrintSetInterface.kFramesAsIs;
      } else if (dialog.selectedframeRadio.selected) {
        frametype = gPrintSetInterface.kSelectedFrame;
      } else if (dialog.eachframesepRadio.selected) {
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
    var flags = gPrintSetInterface.kInitSavePaperSizeType | 
                gPrintSetInterface.kInitSavePaperSizeUnit |
                gPrintSetInterface.kInitSavePaperWidth    | 
                gPrintSetInterface.kInitSavePaperHeight   |
                gPrintSetInterface.kInitSavePaperName     | 
                gPrintSetInterface.kInitSaveColorSpace    |
                gPrintSetInterface.kInitSaveInColor       |
                gPrintSetInterface.kInitSaveResolutionName   |
                gPrintSetInterface.kInitSaveDownloadFonts |
                gPrintSetInterface.kInitSavePrintCommand  |
                gPrintSetInterface.kInitSaveShrinkToFit   |
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
function onChooseFile()
{
  if (dialog.fileRadio.selected == false)
    return;

  try {
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, dialog.fpDialog.getAttribute("label"), nsIFilePicker.modeSave);
    fp.appendFilters(nsIFilePicker.filterAll);
    fp.show();
    if (fp.file && fp.file.path.length > 0) {
      dialog.fileInput.value = fp.file.path;
      gFileFromPicker = dialog.fileInput.value;
    }
  } catch(ex) {
    dump(ex);
  }
}

