// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var dialog;
var gPrintBundle;
var gPrintSettings = null;
var gPrintSettingsInterface  = Components.interfaces.nsIPrintSettings;
var gPaperArray;
var gPrefs;

var gPrintSetInterface = Components.interfaces.nsIPrintSettings;
var doDebug            = true;

//---------------------------------------------------
function checkDouble(element, maxVal)
{
  var value = element.value;
  if (value && value.length > 0) {
    value = value.replace(/[^\.|^0-9]/g,"");
    if (!value) {
      element.value = "";
    } else if (value > maxVal) {
      element.value = maxVal;
    } else {
      element.value = value;
    }
  }
}

//---------------------------------------------------
function isListOfPrinterFeaturesAvailable()
{
  var has_printerfeatures = false;

  try {
    has_printerfeatures = gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".has_special_printerfeatures");
  } catch (ex) {
  }

  return has_printerfeatures;
}

//---------------------------------------------------
function getDoubleStr(val, dec)
{
  var str = val.toString();
  var inx = str.indexOf(".");
  return str.substring(0, inx+dec+1);
}

//---------------------------------------------------
function initDialog()
{
  gPrintBundle = document.getElementById("printBundle");

  dialog = new Object;

  dialog.paperList       = document.getElementById("paperList");
  dialog.paperGroup      = document.getElementById("paperGroup");

  dialog.jobTitleLabel   = document.getElementById("jobTitleLabel");
  dialog.jobTitleGroup   = document.getElementById("jobTitleGroup");
  dialog.jobTitleInput   = document.getElementById("jobTitleInput");

  dialog.colorGroup      = document.getElementById("colorGroup");
  dialog.colorRadioGroup = document.getElementById("colorRadioGroup");
  dialog.colorRadio      = document.getElementById("colorRadio");
  dialog.grayRadio       = document.getElementById("grayRadio");

  dialog.topInput        = document.getElementById("topInput");
  dialog.bottomInput     = document.getElementById("bottomInput");
  dialog.leftInput       = document.getElementById("leftInput");
  dialog.rightInput      = document.getElementById("rightInput");
}

//---------------------------------------------------
function round10(val)
{
  return Math.round(val * 10) / 10;
}


//---------------------------------------------------
function paperListElement(aPaperListElement)
  {
    this.paperListElement = aPaperListElement;
  }

paperListElement.prototype =
  {
    clearPaperList:
      function ()
        {
          // remove the menupopup node child of the menulist.
          this.paperListElement.removeChild(this.paperListElement.firstChild);
        },

    appendPaperNames:
      function (aDataObject)
        {
          var popupNode = document.createElement("menupopup");
          for (var i=0;i<aDataObject.length;i++)  {
            var paperObj = aDataObject[i];
            var itemNode = document.createElement("menuitem");
            var label;
            try {
              label = gPrintBundle.getString(paperObj.name);
            }
            catch (e) {
              /* No name in string bundle ? Then build one manually (this
               * usually happens when gPaperArray was build by createPaperArrayFromPrinterFeatures() ...) */
              if (paperObj.inches) {
                label = paperObj.name + " (" + round10(paperObj.width) + "x" + round10(paperObj.height) + " inch)";
              }
              else {
                label = paperObj.name + " (" + paperObj.width + "x" + paperObj.height + " mm)";
              }
            }
            itemNode.setAttribute("label", label);
            itemNode.setAttribute("value", i);
            popupNode.appendChild(itemNode);
          }
          this.paperListElement.appendChild(popupNode);
        }
  };

//---------------------------------------------------
function createPaperArrayFromDefaults()
{
  var paperNames   = ["letterSize", "legalSize", "exectiveSize", "a5Size", "a4Size", "a3Size", "a2Size", "a1Size", "a0Size"];
  //var paperNames   = ["&letterRadio.label;", "&legalRadio.label;", "&exectiveRadio.label;", "&a4Radio.label;", "&a3Radio.label;"];
  var paperWidths  = [ 8.5,  8.5,  7.25, 148.0, 210.0, 287.0, 420.0, 594.0,  841.0];
  var paperHeights = [11.0, 14.0, 10.50, 210.0, 297.0, 420.0, 594.0, 841.0, 1189.0];
  var paperInches  = [true, true, true,  false, false, false, false, false, false];
  // this is deprecated
  var paperEnums  = [0, 1, 2, 3, 4, 5, 6, 7, 8];

  gPaperArray = new Array();

  for (var i=0;i<paperNames.length;i++) {
    var obj    = new Object();
    obj.name   = paperNames[i];
    obj.width  = paperWidths[i];
    obj.height = paperHeights[i];
    obj.inches = paperInches[i];

    /* Calculate the width/height in millimeters */
    if (paperInches[i]) {
      obj.width_mm  = paperWidths[i]  * 25.4;
      obj.height_mm = paperHeights[i] * 25.4;
    }
    else {
      obj.width_mm  = paperWidths[i];
      obj.height_mm = paperHeights[i];
    }
    gPaperArray[i] = obj;
  }
}

//---------------------------------------------------
function createPaperArrayFromPrinterFeatures()
{
  var printername = gPrintSettings.printerName;
  if (doDebug) {
    dump("createPaperArrayFromPrinterFeatures for " + printername + ".\n");
  }

  gPaperArray = new Array();

  var numPapers = gPrefs.getIntPref("print.tmp.printerfeatures." + printername + ".paper.count");

  if (doDebug) {
    dump("processing " + numPapers + " entries...\n");
  }

  for (var i=0;i<numPapers;i++) {
    var obj    = new Object();
    obj.name      = gPrefs.getCharPref("print.tmp.printerfeatures." + printername + ".paper." + i + ".name");
    obj.width_mm  = gPrefs.getIntPref("print.tmp.printerfeatures."  + printername + ".paper." + i + ".width_mm");
    obj.height_mm = gPrefs.getIntPref("print.tmp.printerfeatures."  + printername + ".paper." + i + ".height_mm");
    obj.inches    = gPrefs.getBoolPref("print.tmp.printerfeatures." + printername + ".paper." + i + ".is_inch");

    /* Calculate the width/height in paper's native units (either inches or millimeters) */
    if (obj.inches) {
      obj.width  = obj.width_mm  / 25.4;
      obj.height = obj.height_mm / 25.4;
    }
    else {
      obj.width  = obj.width_mm;
      obj.height = obj.height_mm;
    }

    gPaperArray[i] = obj;

    if (doDebug) {
      dump("paper index=" + i + ", name=" + obj.name + ", width=" + obj.width + ", height=" + obj.height + ".\n");
    }
  }
}

//---------------------------------------------------
function createPaperArray()
{
  if (isListOfPrinterFeaturesAvailable()) {
    createPaperArrayFromPrinterFeatures();
  }
  else {
    createPaperArrayFromDefaults();
  }
}

//---------------------------------------------------
function createPaperSizeList(selectedInx)
{
  var selectElement = new paperListElement(dialog.paperList);
  selectElement.clearPaperList();

  selectElement.appendPaperNames(gPaperArray);

  if (selectedInx > -1) {
    selectElement.paperListElement.selectedIndex = selectedInx;
  }

  //dialog.paperList = selectElement;
}

//---------------------------------------------------
function loadDialog()
{
  var print_paper_unit       = 0;
  var print_paper_width      = 0.0;
  var print_paper_height     = 0.0;
  var print_paper_name       = "";
  var print_color            = true;
  var print_jobtitle         = "";

  gPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

  if (gPrintSettings) {
    print_paper_unit       = gPrintSettings.paperSizeUnit;
    print_paper_width      = gPrintSettings.paperWidth;
    print_paper_height     = gPrintSettings.paperHeight;
    print_paper_name       = gPrintSettings.paperName;
    print_color            = gPrintSettings.printInColor;
    print_jobtitle         = gPrintSettings.title;
  }

  if (doDebug) {
    dump("loadDialog******************************\n");
    dump("paperSizeType   "+print_paper_unit+"\n");
    dump("paperWidth      "+print_paper_width+"\n");
    dump("paperHeight     "+print_paper_height+"\n");
    dump("paperName       "+print_paper_name+"\n");
    dump("print_color     "+print_color+"\n");
    dump("print_jobtitle   "+print_jobtitle+"\n");
  }

  createPaperArray();

  var paperSelectedInx = 0;
  for (var i=0;i<gPaperArray.length;i++) {
    if (print_paper_name == gPaperArray[i].name) {
      paperSelectedInx = i;
      break;
    }
  }

  if (doDebug) {
    if (i == gPaperArray.length)
      dump("loadDialog: No paper found.\n");
    else
      dump("loadDialog: found paper '"+gPaperArray[paperSelectedInx].name+"'.\n");
  }

  createPaperSizeList(paperSelectedInx);

  /* Enable/disable and/or hide/unhide widgets based in the information
   * whether the selected printer and/or print module supports the matching
   * feature or not */
  if (isListOfPrinterFeaturesAvailable()) {
    // job title
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_jobtitle"))
      dialog.jobTitleInput.removeAttribute("disabled");
    else
      dialog.jobTitleInput.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_jobtitle_change"))
      dialog.jobTitleGroup.removeAttribute("hidden");
    else
      dialog.jobTitleGroup.setAttribute("hidden","true");

    // paper size
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_paper_size"))
      dialog.paperList.removeAttribute("disabled");
    else
      dialog.paperList.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_paper_size_change"))
      dialog.paperGroup.removeAttribute("hidden");
    else
      dialog.paperGroup.setAttribute("hidden","true");

    // color/grayscale radio
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_printincolor"))
      dialog.colorRadioGroup.removeAttribute("disabled");
    else
      dialog.colorRadioGroup.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_printincolor_change"))
      dialog.colorGroup.removeAttribute("hidden");
    else
      dialog.colorGroup.setAttribute("hidden","true");
  }

  if (print_color) {
    dialog.colorRadioGroup.selectedItem = dialog.colorRadio;
  } else {
    dialog.colorRadioGroup.selectedItem = dialog.grayRadio;
  }

  dialog.jobTitleInput.value = print_jobtitle;

  dialog.topInput.value    = gPrintSettings.edgeTop.toFixed(2);
  dialog.bottomInput.value = gPrintSettings.edgeBottom.toFixed(2);
  dialog.leftInput.value   = gPrintSettings.edgeLeft.toFixed(2);
  dialog.rightInput.value  = gPrintSettings.edgeRight.toFixed(2);
}

//---------------------------------------------------
function onLoad()
{
  // Init dialog.
  initDialog();

  gPrintSettings = window.arguments[0].QueryInterface(gPrintSetInterface);
  paramBlock = window.arguments[1].QueryInterface(Components.interfaces.nsIDialogParamBlock);

  if (doDebug) {
    if (gPrintSettings == null) alert("PrintSettings is null!");
    if (paramBlock == null) alert("nsIDialogParam is null!");
  }

  // default return value is "cancel"
  paramBlock.SetInt(0, 0);

  loadDialog();
}

//---------------------------------------------------
function onAccept()
{
  var print_paper_unit        = gPrintSettingsInterface.kPaperSizeInches;
  var print_paper_width       = 0.0;
  var print_paper_height      = 0.0;
  var print_paper_name        = "";

  if (gPrintSettings != null) {
    var paperSelectedInx = dialog.paperList.selectedIndex;
    if (gPaperArray[paperSelectedInx].inches) {
      print_paper_unit = gPrintSettingsInterface.kPaperSizeInches;
    } else {
      print_paper_unit = gPrintSettingsInterface.kPaperSizeMillimeters;
    }
    print_paper_width      = gPaperArray[paperSelectedInx].width;
    print_paper_height     = gPaperArray[paperSelectedInx].height;
    print_paper_name       = gPaperArray[paperSelectedInx].name;

    gPrintSettings.paperSizeUnit   = print_paper_unit;
    gPrintSettings.paperWidth      = print_paper_width;
    gPrintSettings.paperHeight     = print_paper_height;
    gPrintSettings.paperName       = print_paper_name;

    // save these out so they can be picked up by the device spec
    gPrintSettings.printInColor     = dialog.colorRadio.selected;
    gPrintSettings.title            = dialog.jobTitleInput.value;

    gPrintSettings.edgeTop          = dialog.topInput.value;
    gPrintSettings.edgeBottom       = dialog.bottomInput.value;
    gPrintSettings.edgeLeft         = dialog.leftInput.value;
    gPrintSettings.edgeRight        = dialog.rightInput.value;

    if (doDebug) {
      dump("onAccept******************************\n");
      dump("paperSizeUnit    "+print_paper_unit+"\n");
      dump("paperWidth       "+print_paper_width+"\n");
      dump("paperHeight      "+print_paper_height+"\n");
      dump("paperName       '"+print_paper_name+"'\n");

      dump("printInColor     "+gPrintSettings.printInColor+"\n");
    }
  } else {
    dump("************ onAccept gPrintSettings: "+gPrintSettings+"\n");
  }

  if (paramBlock) {
    // set return value to "ok"
    paramBlock.SetInt(0, 1);
  } else {
    dump("*** FATAL ERROR: paramBlock missing\n");
  }

  return true;
}
