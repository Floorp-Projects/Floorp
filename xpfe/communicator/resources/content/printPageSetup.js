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

var gDialog;
var gPrintSettings = null;
var gStringBundle  = null;
var gPaperArray;

var gPrintSettingsInterface = Components.interfaces.nsIPrintSettings;
var gDoDebug = false;

//---------------------------------------------------
function initDialog()
{
  gDialog = new Object;

  gDialog.paperList       = document.getElementById("paperList");
  gDialog.paperGroup      = document.getElementById("paperGroup");
  
  gDialog.orientation     = document.getElementById("orientation");
  gDialog.printBGColors   = document.getElementById("printBGColors");
  gDialog.printBGImages   = document.getElementById("printBGImages");

  gDialog.topInput        = document.getElementById("topInput");
  gDialog.bottomInput     = document.getElementById("bottomInput");
  gDialog.leftInput       = document.getElementById("leftInput");
  gDialog.rightInput      = document.getElementById("rightInput");

  gDialog.hLeftInput      = document.getElementById("hLeftInput");
  gDialog.hCenterInput    = document.getElementById("hCenterInput");
  gDialog.hRightInput     = document.getElementById("hRightInput");

  gDialog.fLeftInput      = document.getElementById("fLeftInput");
  gDialog.fCenterInput    = document.getElementById("fCenterInput");
  gDialog.fRightInput     = document.getElementById("fRightInput");

  gDialog.scalingInput    = document.getElementById("scalingInput");

  gDialog.enabled         = false;

}

//---------------------------------------------------
function checkDouble(element)
{
  var value = element.value;
  if (value && value.length > 0) {
    value = value.replace(/[^\.|^0-9]/g,"");
    if (!value) value = "";
    element.value = value;
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

  var selectElement = new listElement(gDialog.paperList);
  selectElement.clearList();

  selectElement.appendPaperNames(gPaperArray);

  if (selectedInx > -1) {
    selectElement.listElement.selectedIndex = selectedInx;
  }

  //gDialog.paperList = selectElement;
}   

//---------------------------------------------------
function loadDialog()
{
  var print_paper_type    = 0;
  var print_paper_unit    = 0;
  var print_paper_width   = 0.0;
  var print_paper_height  = 0.0;
  var print_orientation   = 0;
  var print_margin_top    = 0.5;
  var print_margin_left   = 0.5;
  var print_margin_bottom = 0.5;
  var print_margin_right  = 0.5;

  if (gPrintSettings) {
    print_paper_type   = gPrintSettings.paperSizeType;
    print_paper_unit   = gPrintSettings.paperSizeUnit;
    print_paper_width  = gPrintSettings.paperWidth;
    print_paper_height = gPrintSettings.paperHeight;
    print_orientation  = gPrintSettings.orientation;

    print_margin_top    = gPrintSettings.marginTop;
    print_margin_left   = gPrintSettings.marginLeft;
    print_margin_right  = gPrintSettings.marginRight;
    print_margin_bottom = gPrintSettings.marginBottom;
  }

  if (gDoDebug) {
    dump("paperSizeType "+print_paper_unit+"\n");
    dump("paperWidth    "+print_paper_width+"\n");
    dump("paperHeight   "+print_paper_height+"\n");
    dump("orientation   "+print_orientation+"\n");

    dump("print_margin_top    "+print_margin_top+"\n");
    dump("print_margin_left   "+print_margin_left+"\n");
    dump("print_margin_right  "+print_margin_right+"\n");
    dump("print_margin_bottom "+print_margin_bottom+"\n");
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

  gDialog.printBGColors.checked = gPrintSettings.printBGColors;
  gDialog.printBGImages.checked = gPrintSettings.printBGImages;


  if (print_orientation == gPrintSettingsInterface.kPortraitOrientation) {
    gDialog.orientation.selectedIndex = 0;

  } else if (print_orientation == gPrintSettingsInterface.kLandscapeOrientation) {
    gDialog.orientation.selectedIndex = 1;
  }

  gDialog.topInput.value    = getDoubleStr(print_margin_top, 1);
  gDialog.bottomInput.value = getDoubleStr(print_margin_bottom, 1);
  gDialog.leftInput.value   = getDoubleStr(print_margin_left, 1);
  gDialog.rightInput.value  = getDoubleStr(print_margin_right, 1);

  gDialog.hLeftInput.value   = gPrintSettings.headerStrLeft;
  gDialog.hCenterInput.value = gPrintSettings.headerStrCenter;
  gDialog.hRightInput.value  = gPrintSettings.headerStrRight;

  gDialog.fLeftInput.value   = gPrintSettings.footerStrLeft;
  gDialog.fCenterInput.value = gPrintSettings.footerStrCenter;
  gDialog.fRightInput.value  = gPrintSettings.footerStrRight;

  gDialog.scalingInput.value  = getDoubleStr(gPrintSettings.scaling * 100.0, 3);

}

var param;

//---------------------------------------------------
function onLoad()
{
  // Init gDialog.
  initDialog();

  if (window.arguments[0] != null) {
    gPrintSettings = window.arguments[0].QueryInterface(Components.interfaces.nsIPrintSettings);
  } else if (gDoDebug) {
    alert("window.arguments[0] == null!");
  }

  if (gPrintSettings) {
    loadDialog();
  } else if (gDoDebug) {
    alert("Could initialize gDialog, PrintSettings is null!");
  }
}

//---------------------------------------------------
function onAccept()
{
  var print_paper_type   = gPrintSettingsInterface.kPaperSizeDefined;
  var print_paper_unit   = gPrintSettingsInterface.kPaperSizeInches;
  var print_paper_width  = 8.5;
  var print_paper_height = 11.0;

  if (gPrintSettings) {
    var selectedInx = gDialog.paperList.selectedIndex;
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

    gPrintSettings.orientation = gDialog.orientation.selectedIndex;

    // save these out so they can be picked up by the device spec
    gPrintSettings.marginTop    = gDialog.topInput.value;
    gPrintSettings.marginLeft   = gDialog.leftInput.value;
    gPrintSettings.marginBottom = gDialog.bottomInput.value;
    gPrintSettings.marginRight  = gDialog.rightInput.value;


    gPrintSettings.headerStrLeft   = gDialog.hLeftInput.value;
    gPrintSettings.headerStrCenter = gDialog.hCenterInput.value;
    gPrintSettings.headerStrRight  = gDialog.hRightInput.value;

    gPrintSettings.footerStrLeft   = gDialog.fLeftInput.value;
    gPrintSettings.footerStrCenter = gDialog.fCenterInput.value;
    gPrintSettings.footerStrRight  = gDialog.fRightInput.value;

    gPrintSettings.printBGColors = gDialog.printBGColors.checked;
    gPrintSettings.printBGImages = gDialog.printBGImages.checked;

    var scaling = document.getElementById("scalingInput").value;
    if (scaling < 50.0 || scaling > 100.0) {
      scaling = 100.0;
    }
    scaling /= 100.0;
    gPrintSettings.scaling = scaling;

    if (gDoDebug) {
      dump("******* Page Setup Accepting ******\n");
      dump("paperSize     "+gPrintSettings.paperSize+" (deprecated)\n");
      dump("paperSizeType "+print_paper_type+" (should be 1)\n");
      dump("paperSizeUnit "+print_paper_unit+"\n");
      dump("paperWidth    "+print_paper_width+"\n");
      dump("paperHeight   "+print_paper_height+"\n");

      dump("print_margin_top    "+gDialog.topInput.value+"\n");
      dump("print_margin_left   "+gDialog.leftInput.value+"\n");
      dump("print_margin_right  "+gDialog.bottomInput.value+"\n");
      dump("print_margin_bottom "+gDialog.rightInput.value+"\n");
    }
  }

  return true;
}

