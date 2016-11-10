// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var dialog;
var printService       = null;
var gOriginalNumCopies = 1;

var paramBlock;
var gPrefs             = null;
var gPrintSettings     = null;
var gWebBrowserPrint   = null;
var gPrintSetInterface = Components.interfaces.nsIPrintSettings;
var doDebug            = false;

// ---------------------------------------------------
function initDialog()
{
  dialog = {};

  dialog.propertiesButton = document.getElementById("properties");
  dialog.descText         = document.getElementById("descText");

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

  dialog.fileCheck       = document.getElementById("fileCheck");
  dialog.printerLabel    = document.getElementById("printerLabel");
  dialog.printerList     = document.getElementById("printerList");

  dialog.printButton     = document.documentElement.getButton("accept");

  // <data> elements
  dialog.printName       = document.getElementById("printButton");
  dialog.fpDialog        = document.getElementById("fpDialog");

  dialog.enabled         = false;
}

// ---------------------------------------------------
function checkInteger(element)
{
  var value = element.value;
  if (value && value.length > 0) {
    value = value.replace(/[^0-9]/g, "");
    if (!value) value = "";
    element.value = value;
  }
  if (!value || value < 1 || value > 999)
    dialog.printButton.setAttribute("disabled", "true");
  else
    dialog.printButton.removeAttribute("disabled");
}

// ---------------------------------------------------
function stripTrailingWhitespace(element)
{
  var value = element.value;
  value = value.replace(/\s+$/, "");
  element.value = value;
}

// ---------------------------------------------------
function getPrinterDescription(printerName)
{
  var s = "";

  try {
    /* This may not work with non-ASCII test (see bug 235763 comment #16) */
    s = gPrefs.getCharPref("print.printer_" + printerName + ".printer_description")
  } catch (e) {
  }

  return s;
}

// ---------------------------------------------------
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
            this.listElement.setAttribute("value", "");
            this.listElement.setAttribute("label",
              document.getElementById("printingBundle")
                      .getString("noprinter"));

            this.listElement.setAttribute("disabled", "true");
            dialog.printerLabel.setAttribute("disabled", "true");
            dialog.propertiesButton.setAttribute("disabled", "true");
            dialog.fileCheck.setAttribute("disabled", "true");
            dialog.printButton.setAttribute("disabled", "true");
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

// ---------------------------------------------------
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
  } catch (e) { printerEnumerator = null; }

  selectElement.appendPrinterNames(printerEnumerator);
  selectElement.listElement.value = printService.defaultPrinterName;

  // make sure we load the prefs for the initially selected printer
  setPrinterDefaultsForSelectedPrinter();
}


// ---------------------------------------------------
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
    dump("setPrinterDefaultsForSelectedPrinter: printerName='" + gPrintSettings.printerName + "', paperName='" + gPrintSettings.paperName + "'\n");
  }
}

// ---------------------------------------------------
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
  } catch (e) {
    dump("problems getting printingPromptService\n");
  }
}

// ---------------------------------------------------
function doPrintRange(inx)
{
  if (inx == 1) {
    dialog.frompageInput.removeAttribute("disabled");
    dialog.frompageLabel.removeAttribute("disabled");
    dialog.topageInput.removeAttribute("disabled");
    dialog.topageLabel.removeAttribute("disabled");
  } else {
    dialog.frompageInput.setAttribute("disabled", "true");
    dialog.frompageLabel.setAttribute("disabled", "true");
    dialog.topageInput.setAttribute("disabled", "true");
    dialog.topageLabel.setAttribute("disabled", "true");
  }
}

// ---------------------------------------------------
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
  } catch (e) {}

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
    dump("print_tofile            " + print_tofile + "\n");
    dump("print_frame             " + print_frametype + "\n");
    dump("print_howToEnableUI     " + print_howToEnableUI + "\n");
    dump("selection_radio_enabled " + print_selection_radio_enabled + "\n");
  }

  dialog.printrangeGroup.selectedItem = dialog.allpagesRadio;
  if (print_selection_radio_enabled) {
    dialog.selectionRadio.removeAttribute("disabled");
  } else {
    dialog.selectionRadio.setAttribute("disabled", "true");
  }
  doPrintRange(dialog.rangeRadio.selected);
  dialog.frompageInput.value  = 1;
  dialog.topageInput.value    = 1;
  dialog.numCopiesInput.value = print_copies;

  if (doDebug) {
    dump("print_howToEnableUI: " + print_howToEnableUI + "\n");
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
    dialog.aslaidoutRadio.removeAttribute("disabled");       // enable

    dialog.selectedframeRadio.setAttribute("disabled", "true"); // disable
    dialog.eachframesepRadio.removeAttribute("disabled");       // enable
    dialog.printframeGroupLabel.removeAttribute("disabled");    // enable

    // initialize
    dialog.printframeGroup.selectedItem = dialog.eachframesepRadio;

  } else {
    dialog.aslaidoutRadio.setAttribute("disabled", "true");
    dialog.selectedframeRadio.setAttribute("disabled", "true");
    dialog.eachframesepRadio.setAttribute("disabled", "true");
    dialog.printframeGroupLabel.setAttribute("disabled", "true");
  }

  dialog.printButton.label = dialog.printName.getAttribute("label");
}

// ---------------------------------------------------
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

// ---------------------------------------------------
function onAccept()
{
  if (gPrintSettings != null) {
    var print_howToEnableUI = gPrintSetInterface.kFrameEnableNone;

    // save these out so they can be picked up by the device spec
    gPrintSettings.printerName = dialog.printerList.value;
    print_howToEnableUI        = gPrintSettings.howToEnableFrameUI;
    gPrintSettings.printToFile = dialog.fileCheck.checked;

    if (gPrintSettings.printToFile && !chooseFile())
      return false;

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
      dump("frametype      " + frametype + "\n");
      dump("numCopies      " + gPrintSettings.numCopies + "\n");
      dump("printRange     " + gPrintSettings.printRange + "\n");
      dump("printerName    " + gPrintSettings.printerName + "\n");
      dump("startPageRange " + gPrintSettings.startPageRange + "\n");
      dump("endPageRange   " + gPrintSettings.endPageRange + "\n");
      dump("printToFile    " + gPrintSettings.printToFile + "\n");
    }
  }

  var saveToPrefs = false;

  saveToPrefs = gPrefs.getBoolPref("print.save_print_settings");

  if (saveToPrefs && printService != null) {
    var flags = gPrintSetInterface.kInitSavePaperSize      |
                gPrintSetInterface.kInitSaveEdges          |
                gPrintSetInterface.kInitSaveInColor        |
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

// ---------------------------------------------------
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

// ---------------------------------------------------
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
  } catch (ex) {
    dump(ex);
  }

  return false;
}

