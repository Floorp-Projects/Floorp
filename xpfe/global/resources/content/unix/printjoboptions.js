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
var gPrintSettings = null;
var gStringBundle  = null;
var gPrintSettingsInterface  = Components.interfaces.nsIPrintSettings;
var gPaperArray;

var default_command    = "lpr";
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
    } else {
      if (value > maxVal) {
        element.value = maxVal;
      } else {
        element.value = value;
      }
    }
  }
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
  dialog = new Object;

  dialog.paperList       = document.getElementById("paperList");
  dialog.paperGroup      = document.getElementById("paperGroup");
  
  dialog.cmdLabel        = document.getElementById("cmdLabel");
  dialog.cmdInput        = document.getElementById("cmdInput");

  dialog.colorGroup      = document.getElementById("colorGroup");
  dialog.colorRadio      = document.getElementById("colorRadio");
  dialog.grayRadio       = document.getElementById("grayRadio");

  dialog.topInput        = document.getElementById("topInput");
  dialog.bottomInput     = document.getElementById("bottomInput");
  dialog.leftInput       = document.getElementById("leftInput");
  dialog.rightInput      = document.getElementById("rightInput");
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
          this.listElement.removeChild(this.listElement.firstChild);
        },

    appendPaperNames: 
      function (aDataObject) 
        { 
          var popupNode = document.createElement("menupopup"); 
          for (var i=0;i<aDataObject.length;i++)  {
            var paperObj = aDataObject[i];
            var itemNode = document.createElement("menuitem");
            try {
              var label = gStringBundle.GetStringFromName(paperObj.name)
              itemNode.setAttribute("label", label);
              itemNode.setAttribute("value", i);
              popupNode.appendChild(itemNode);
            } catch (e) {}
          }
          this.listElement.appendChild(popupNode); 
        } 
  };

//---------------------------------------------------
function createPaperArray()
{
  var paperNames   = ["letterSize", "legalSize", "exectiveSize", "a4Size", "a3Size"];
  //var paperNames   = ["&letterRadio.label;", "&legalRadio.label;", "&exectiveRadio.label;", "&a4Radio.label;", "&a3Radio.label;"];
  var paperWidths  = [8.5, 8.5, 7.25, 210.0, 287.0];
  var paperHeights = [11.0, 14.0, 10.5, 297.0, 420.0];
  var paperInches  = [true, true, true, false, false];
  // this is deprecated
  var paperEnums  = [0, 1, 2, 3, 4];

  gPaperArray = new Array();

  for (var i=0;i<paperNames.length;i++) {
    var obj    = new Object();
    obj.name   = paperNames[i];
    obj.width  = paperWidths[i];
    obj.height = paperHeights[i];
    obj.inches = paperInches[i];
    obj.paperSize = paperEnums[i]; // deprecated
    gPaperArray[i] = obj;
  }
}

//---------------------------------------------------
function createPaperSizeList(selectedInx)
{
  gStringBundle = srGetStrBundle("chrome://communicator/locale/printPageSetup.properties");

  var selectElement = new listElement(dialog.paperList);
  selectElement.clearList();

  selectElement.appendPaperNames(gPaperArray);

  if (selectedInx > -1) {
    selectElement.listElement.selectedIndex = selectedInx;
  }

  //dialog.paperList = selectElement;
}   

//---------------------------------------------------
function loadDialog()
{
  var print_paper_type    = 0;
  var print_paper_unit    = 0;
  var print_paper_width   = 0.0;
  var print_paper_height  = 0.0;
  var print_color         = true;
  var print_command       = default_command;

  if (gPrintSettings) {
    print_paper_type   = gPrintSettings.paperSizeType;
    print_paper_unit   = gPrintSettings.paperSizeUnit;
    print_paper_width  = gPrintSettings.paperWidth;
    print_paper_height = gPrintSettings.paperHeight;
    print_color     = gPrintSettings.printInColor;
    print_command   = gPrintSettings.printCommand;
  }

  if (doDebug) {
    dump("loadDialog******************************\n");
    dump("paperSizeType "+print_paper_unit+"\n");
    dump("paperWidth    "+print_paper_width+"\n");
    dump("paperHeight   "+print_paper_height+"\n");
    dump("printInColor  "+print_color+"\n");
    dump("printCommand  "+print_command+"\n");
  }

  createPaperArray();

  var selectedInx = 0;
  for (var i=0;i<gPaperArray.length;i++) {
    if (print_paper_width == gPaperArray[i].width && print_paper_height == gPaperArray[i].height) {
      selectedInx = i;
      break;
    }
  }
  createPaperSizeList(selectedInx);

  if (print_command == "") {
    print_command = default_command;
  }

  if (print_color) {
    dialog.colorGroup.selectedItem = dialog.colorRadio;
  } else {
    dialog.colorGroup.selectedItem = dialog.grayRadio;
  }

  dialog.cmdInput.value = print_command;

  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    dialog.topInput.value    = pref.getIntPref("print.print_edge_top") / 100.0;
    dialog.bottomInput.value = pref.getIntPref("print.print_edge_left") / 100.0;
    dialog.leftInput.value   = pref.getIntPref("print.print_edge_right") / 100.0;
    dialog.rightInput.value  = pref.getIntPref("print.print_edge_bottom") / 100.0;
  } catch (e) {
    dialog.topInput.value    = "0.04";
    dialog.bottomInput.value = "0.04";
    dialog.leftInput.value   = "0.04";
    dialog.rightInput.value  = "0.04";
  }
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
  var print_paper_type   = gPrintSettingsInterface.kPaperSizeDefined;
  var print_paper_unit   = gPrintSettingsInterface.kPaperSizeInches;
  var print_paper_width  = 8.5;
  var print_paper_height = 11.0;

  if (gPrintSettings != null) {
    var selectedInx = dialog.paperList.selectedIndex;
    if (gPaperArray[selectedInx].inches) {
      print_paper_unit = gPrintSettingsInterface.kPaperSizeInches;
    } else {
      print_paper_unit = gPrintSettingsInterface.kPaperSizeMillimeters;
    }
    print_paper_width  = gPaperArray[selectedInx].width;
    print_paper_height = gPaperArray[selectedInx].height;
    gPrintSettings.paperSize = gPaperArray[selectedInx].paperSize; // deprecated

    gPrintSettings.paperSizeType = print_paper_type;
    gPrintSettings.paperSizeUnit = print_paper_unit;
    gPrintSettings.paperWidth    = print_paper_width;
    gPrintSettings.paperHeight   = print_paper_height;

    // save these out so they can be picked up by the device spec
    gPrintSettings.printInColor = dialog.colorRadio.selected;
    gPrintSettings.printCommand = dialog.cmdInput.value;

    // 
    try {
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
      var i = dialog.topInput.value * 100;
      pref.setIntPref("print.print_edge_top", i);

      i = dialog.bottomInput.value * 100;
      pref.setIntPref("print.print_edge_left", i);

      i = dialog.leftInput.value * 100;
      pref.setIntPref("print.print_edge_right", i);

      i = dialog.rightInput.value * 100;
      pref.setIntPref("print.print_edge_bottom", i);
    } catch (e) {
    }

    if (doDebug) {
      dump("onAccept******************************\n");
      dump("paperSize     "+gPrintSettings.paperSize+" (deprecated)\n");
      dump("paperSizeType "+print_paper_type+" (should be 1)\n");
      dump("paperSizeUnit "+print_paper_unit+"\n");
      dump("paperWidth    "+print_paper_width+"\n");
      dump("paperHeight   "+print_paper_height+"\n");

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
