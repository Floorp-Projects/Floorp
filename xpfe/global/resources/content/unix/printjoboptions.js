/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Asko Tontti <atontti@cc.hut.fi>
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
var gPrintSettings = null;
var gStringBundle  = null;
var gPrintSettingsInterface  = Components.interfaces.nsIPrintSettings;
var gPaperArray;
var gPlexArray;
var gResolutionArray;
var gColorSpaceArray;
var gPrefs;

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
function isListOfPrinterFeaturesAvailable()
{
  var has_printerfeatures = false;
  
  try {
    has_printerfeatures = gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".has_special_printerfeatures");
  } catch(ex) {
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
  dialog = new Object;

  dialog.paperList       = document.getElementById("paperList");
  dialog.paperGroup      = document.getElementById("paperGroup");

  dialog.plexList        = document.getElementById("plexList");
  dialog.plexGroup       = document.getElementById("plexGroup");

  dialog.resolutionList  = document.getElementById("resolutionList");
  dialog.resolutionGroup = document.getElementById("resolutionGroup");

  dialog.jobTitleLabel   = document.getElementById("jobTitleLabel");
  dialog.jobTitleGroup   = document.getElementById("jobTitleGroup");
  dialog.jobTitleInput   = document.getElementById("jobTitleInput");
    
  dialog.cmdLabel        = document.getElementById("cmdLabel");
  dialog.cmdGroup        = document.getElementById("cmdGroup");
  dialog.cmdInput        = document.getElementById("cmdInput");

  dialog.colorspaceList  = document.getElementById("colorspaceList");
  dialog.colorspaceGroup = document.getElementById("colorspaceGroup");

  dialog.colorGroup      = document.getElementById("colorGroup");
  dialog.colorRadioGroup = document.getElementById("colorRadioGroup");
  dialog.colorRadio      = document.getElementById("colorRadio");
  dialog.grayRadio       = document.getElementById("grayRadio");

  dialog.fontsGroup      = document.getElementById("fontsGroup");
  dialog.downloadFonts   = document.getElementById("downloadFonts");

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
              label = gStringBundle.GetStringFromName(paperObj.name)
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
    obj.paperSize = paperEnums[i]; // deprecated
    
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
    obj.paperSize = 666; // deprecated
    
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
  gStringBundle = srGetStrBundle("chrome://global/locale/printPageSetup.properties");

  var selectElement = new paperListElement(dialog.paperList);
  selectElement.clearPaperList();

  selectElement.appendPaperNames(gPaperArray);

  if (selectedInx > -1) {
    selectElement.paperListElement.selectedIndex = selectedInx;
  }

  //dialog.paperList = selectElement;
}

//---------------------------------------------------
function plexListElement(aPlexListElement)
  {
    this.plexListElement = aPlexListElement;
  }

plexListElement.prototype =
  {
    clearPlexList:
      function ()
        {
          // remove the menupopup node child of the menulist.
          this.plexListElement.removeChild(this.plexListElement.firstChild);
        },

    appendPlexNames: 
      function (aDataObject) 
        { 
          var popupNode = document.createElement("menupopup"); 
          for (var i=0;i<aDataObject.length;i++)  {
            var plexObj = aDataObject[i];
            var itemNode = document.createElement("menuitem");
            var label;
            try {
              label = gStringBundle.GetStringFromName(plexObj.name)
            } 
            catch (e) {
              /* No name in string bundle ? Then build one manually (this
               * usually happens when gPlexArray was build by createPlexArrayFromPrinterFeatures() ...) */              
              label = plexObj.name;
            }
            itemNode.setAttribute("label", label);
            itemNode.setAttribute("value", i);
            popupNode.appendChild(itemNode);            
          }
          this.plexListElement.appendChild(popupNode); 
        } 
  };


//---------------------------------------------------
function createPlexArrayFromDefaults()
{
  var plexNames = ["default"];

  gPlexArray = new Array();

  for (var i=0;i<plexNames.length;i++) {
    var obj    = new Object();
    obj.name   = plexNames[i];
    
    gPlexArray[i] = obj;
  }
}

//---------------------------------------------------
function createPlexArrayFromPrinterFeatures()
{
  var printername = gPrintSettings.printerName;
  if (doDebug) {
    dump("createPlexArrayFromPrinterFeatures for " + printername + ".\n");
  }

  gPlexArray = new Array();
  
  var numPlexs = gPrefs.getIntPref("print.tmp.printerfeatures." + printername + ".plex.count");
  
  if (doDebug) {
    dump("processing " + numPlexs + " entries...\n");
  }    

  for ( var i=0 ; i < numPlexs ; i++ ) {
    var obj = new Object();
    obj.name = gPrefs.getCharPref("print.tmp.printerfeatures." + printername + ".plex." + i + ".name");

    gPlexArray[i] = obj;

    if (doDebug) {
      dump("plex index=" + i + ", name='" + obj.name + "'.\n");
    }
  }  
}

//---------------------------------------------------
function createPlexArray()
{  
  if (isListOfPrinterFeaturesAvailable()) {
    createPlexArrayFromPrinterFeatures();    
  }
  else {
    createPlexArrayFromDefaults();
  }
}

//---------------------------------------------------
function createPlexNameList(selectedInx)
{
  gStringBundle = srGetStrBundle("chrome://global/locale/printPageSetup.properties");

  var selectElement = new plexListElement(dialog.plexList);
  selectElement.clearPlexList();

  selectElement.appendPlexNames(gPlexArray);

  if (selectedInx > -1) {
    selectElement.plexListElement.selectedIndex = selectedInx;
  }

  //dialog.plexList = selectElement;
}   

//---------------------------------------------------
function resolutionListElement(aResolutionListElement)
  {
    this.resolutionListElement = aResolutionListElement;
  }

resolutionListElement.prototype =
  {
    clearResolutionList:
      function ()
        {
          // remove the menupopup node child of the menulist.
          this.resolutionListElement.removeChild(this.resolutionListElement.firstChild);
        },

    appendResolutionNames: 
      function (aDataObject) 
        { 
          var popupNode = document.createElement("menupopup"); 
          for (var i=0;i<aDataObject.length;i++)  {
            var resolutionObj = aDataObject[i];
            var itemNode = document.createElement("menuitem");
            var label;
            try {
              label = gStringBundle.GetStringFromName(resolutionObj.name)
            } 
            catch (e) {
              /* No name in string bundle ? Then build one manually (this
               * usually happens when gResolutionArray was build by createResolutionArrayFromPrinterFeatures() ...) */              
              label = resolutionObj.name;
            }
            itemNode.setAttribute("label", label);
            itemNode.setAttribute("value", i);
            popupNode.appendChild(itemNode);            
          }
          this.resolutionListElement.appendChild(popupNode); 
        } 
  };


//---------------------------------------------------
function createResolutionArrayFromDefaults()
{
  var resolutionNames = ["default"];

  gResolutionArray = new Array();

  for (var i=0;i<resolutionNames.length;i++) {
    var obj    = new Object();
    obj.name   = resolutionNames[i];
    
    gResolutionArray[i] = obj;
  }
}

//---------------------------------------------------
function createResolutionArrayFromPrinterFeatures()
{
  var printername = gPrintSettings.printerName;
  if (doDebug) {
    dump("createResolutionArrayFromPrinterFeatures for " + printername + ".\n");
  }

  gResolutionArray = new Array();
  
  var numResolutions = gPrefs.getIntPref("print.tmp.printerfeatures." + printername + ".resolution.count");
  
  if (doDebug) {
    dump("processing " + numResolutions + " entries...\n");
  }    

  for ( var i=0 ; i < numResolutions ; i++ ) {
    var obj = new Object();
    obj.name = gPrefs.getCharPref("print.tmp.printerfeatures." + printername + ".resolution." + i + ".name");

    gResolutionArray[i] = obj;

    if (doDebug) {
      dump("resolution index=" + i + ", name='" + obj.name + "'.\n");
    }
  }  
}

//---------------------------------------------------
function createResolutionArray()
{  
  if (isListOfPrinterFeaturesAvailable()) {
    createResolutionArrayFromPrinterFeatures();    
  }
  else {
    createResolutionArrayFromDefaults();
  }
}

//---------------------------------------------------
function createResolutionNameList(selectedInx)
{
  gStringBundle = srGetStrBundle("chrome://global/locale/printPageSetup.properties");

  var selectElement = new resolutionListElement(dialog.resolutionList);
  selectElement.clearResolutionList();

  selectElement.appendResolutionNames(gResolutionArray);

  if (selectedInx > -1) {
    selectElement.resolutionListElement.selectedIndex = selectedInx;
  }

  //dialog.resolutionList = selectElement;
}  

//---------------------------------------------------
function colorspaceListElement(aColorspaceListElement)
  {
    this.colorspaceListElement = aColorspaceListElement;
  }

colorspaceListElement.prototype =
  {
    clearColorspaceList:
      function ()
        {
          // remove the menupopup node child of the menulist.
          this.colorspaceListElement.removeChild(this.colorspaceListElement.firstChild);
        },

    appendColorspaceNames: 
      function (aDataObject) 
        { 
          var popupNode = document.createElement("menupopup"); 
          for (var i=0;i<aDataObject.length;i++)  {
            var colorspaceObj = aDataObject[i];
            var itemNode = document.createElement("menuitem");
            var label;
            try {
              label = gStringBundle.GetStringFromName(colorspaceObj.name)
            } 
            catch (e) {
              /* No name in string bundle ? Then build one manually (this
               * usually happens when gColorspaceArray was build by createColorspaceArrayFromPrinterFeatures() ...) */              
              label = colorspaceObj.name;
            }
            itemNode.setAttribute("label", label);
            itemNode.setAttribute("value", i);
            popupNode.appendChild(itemNode);            
          }
          this.colorspaceListElement.appendChild(popupNode); 
        } 
  };


//---------------------------------------------------
function createColorspaceArrayFromDefaults()
{
  var colorspaceNames = ["default"];

  gColorspaceArray = new Array();

  for (var i=0;i<colorspaceNames.length;i++) {
    var obj    = new Object();
    obj.name   = colorspaceNames[i];
    
    gColorspaceArray[i] = obj;
  }
}

//---------------------------------------------------
function createColorspaceArrayFromPrinterFeatures()
{
  var printername = gPrintSettings.printerName;
  if (doDebug) {
    dump("createColorspaceArrayFromPrinterFeatures for " + printername + ".\n");
  }

  gColorspaceArray = new Array();
  
  var numColorspaces = gPrefs.getIntPref("print.tmp.printerfeatures." + printername + ".colorspace.count");
  
  if (doDebug) {
    dump("processing " + numColorspaces + " entries...\n");
  }    

  for ( var i=0 ; i < numColorspaces ; i++ ) {
    var obj = new Object();
    obj.name = gPrefs.getCharPref("print.tmp.printerfeatures." + printername + ".colorspace." + i + ".name");

    gColorspaceArray[i] = obj;

    if (doDebug) {
      dump("colorspace index=" + i + ", name='" + obj.name + "'.\n");
    }
  }  
}

//---------------------------------------------------
function createColorspaceArray()
{  
  if (isListOfPrinterFeaturesAvailable()) {
    createColorspaceArrayFromPrinterFeatures();    
  }
  else {
    createColorspaceArrayFromDefaults();
  }
}

//---------------------------------------------------
function createColorspaceNameList(selectedInx)
{
  gStringBundle = srGetStrBundle("chrome://global/locale/printPageSetup.properties");

  var selectElement = new colorspaceListElement(dialog.colorspaceList);
  selectElement.clearColorspaceList();

  selectElement.appendColorspaceNames(gColorspaceArray);

  if (selectedInx > -1) {
    selectElement.colorspaceListElement.selectedIndex = selectedInx;
  }

  //dialog.colorspaceList = selectElement;
}  

//---------------------------------------------------
function loadDialog()
{
  var print_paper_type       = 0;
  var print_paper_unit       = 0;
  var print_paper_width      = 0.0;
  var print_paper_height     = 0.0;
  var print_paper_name       = "";
  var print_plex_name        = "";
  var print_resolution_name  = "";
  var print_colorspace       = "";
  var print_color            = true;
  var print_downloadfonts    = true;
  var print_command          = default_command;
  var print_jobtitle         = "";

  gPrefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);

  if (gPrintSettings) {
    print_paper_type       = gPrintSettings.paperSizeType;
    print_paper_unit       = gPrintSettings.paperSizeUnit;
    print_paper_width      = gPrintSettings.paperWidth;
    print_paper_height     = gPrintSettings.paperHeight;
    print_paper_name       = gPrintSettings.paperName;
    print_plex_name        = gPrintSettings.plexName;
    print_resolution_name  = gPrintSettings.resolutionName;
    print_colorspace       = gPrintSettings.colorspace;
    print_color            = gPrintSettings.printInColor;
    print_downloadfonts    = gPrintSettings.downloadFonts;
    print_command          = gPrintSettings.printCommand;
    print_jobtitle         = gPrintSettings.title;
  }

  if (doDebug) {
    dump("loadDialog******************************\n");
    dump("paperSizeType   "+print_paper_unit+"\n");
    dump("paperWidth      "+print_paper_width+"\n");
    dump("paperHeight     "+print_paper_height+"\n");
    dump("paperName       "+print_paper_name+"\n");
    dump("plexName        "+print_plex_name+"\n");
    dump("resolutionName  "+print_resolution_name+"\n");
    dump("colorspace      "+print_colorspace+"\n");
    dump("print_color     "+print_color+"\n");
    dump("print_downloadfonts "+print_downloadfonts+"\n");
    dump("print_command    "+print_command+"\n");
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

  createPlexArray();
  var plexSelectedInx = 0;
  for (var i=0;i<gPlexArray.length;i++) { 
    if (print_plex_name == gPlexArray[i].name) {
      plexSelectedInx = i;
      break;
    }
  }

  if (doDebug) {
    if (i == gPlexArray.length)
      dump("loadDialog: No plex found.\n");
    else
      dump("loadDialog: found plex '"+gPlexArray[plexSelectedInx].name+"'.\n");
  }

  createResolutionArray();
  var resolutionSelectedInx = 0;
  for (var i=0;i<gResolutionArray.length;i++) { 
    if (print_resolution_name == gResolutionArray[i].name) {
      resolutionSelectedInx = i;
      break;
    }
  }
  
  if (doDebug) {
    if (i == gResolutionArray.length)
      dump("loadDialog: No resolution found.\n");
    else
      dump("loadDialog: found resolution '"+gResolutionArray[resolutionSelectedInx].name+"'.\n");
  }

  createColorspaceArray();
  var colorspaceSelectedInx = 0;
  for (var i=0;i<gColorspaceArray.length;i++) { 
    if (print_colorspace == gColorspaceArray[i].name) {
      colorspaceSelectedInx = i;
      break;
    }
  }
  
  if (doDebug) {
    if (i == gColorspaceArray.length)
      dump("loadDialog: No colorspace found.\n");
    else
      dump("loadDialog: found colorspace '"+gColorspaceArray[colorspaceSelectedInx].name+"'.\n");
  }
  
  createPlexNameList(plexSelectedInx);
  createResolutionNameList(resolutionSelectedInx);
  createColorspaceNameList(colorspaceSelectedInx);
    
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

    // spooler command
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_spoolercommand"))
      dialog.cmdInput.removeAttribute("disabled");
    else
      dialog.cmdInput.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_spoolercommand_change"))
      dialog.cmdGroup.removeAttribute("hidden");
    else
      dialog.cmdGroup.setAttribute("hidden","true");

    // paper size
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_paper_size"))
      dialog.paperList.removeAttribute("disabled");
    else
      dialog.paperList.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_paper_size_change"))
      dialog.paperGroup.removeAttribute("hidden");
    else
      dialog.paperGroup.setAttribute("hidden","true");
            
    // plex mode
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_plex"))
      dialog.plexList.removeAttribute("disabled");
    else
      dialog.plexList.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_plex_change"))
      dialog.plexGroup.removeAttribute("hidden");
    else
      dialog.plexGroup.setAttribute("hidden","true");

    // resolution
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_resolution"))
      dialog.resolutionList.removeAttribute("disabled");
    else
      dialog.resolutionList.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_resolution_change"))
      dialog.resolutionGroup.removeAttribute("hidden");
    else
      dialog.resolutionGroup.setAttribute("hidden","true");

    // color/grayscale radio
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_printincolor"))
      dialog.colorRadioGroup.removeAttribute("disabled");
    else
      dialog.colorRadioGroup.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_printincolor_change"))
      dialog.colorGroup.removeAttribute("hidden");
    else
      dialog.colorGroup.setAttribute("hidden","true");

    // colorspace
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_colorspace"))
      dialog.colorspaceList.removeAttribute("disabled");
    else
      dialog.colorspaceList.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_colorspace_change"))
      dialog.colorspaceGroup.removeAttribute("hidden");
    else
      dialog.colorspaceGroup.setAttribute("hidden","true");

    // font download
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".can_change_downloadfonts"))
      dialog.downloadFonts.removeAttribute("disabled");
    else
      dialog.downloadFonts.setAttribute("disabled","true");
    if (gPrefs.getBoolPref("print.tmp.printerfeatures." + gPrintSettings.printerName + ".supports_downloadfonts_change"))
      dialog.fontsGroup.removeAttribute("hidden");
    else
      dialog.fontsGroup.setAttribute("hidden","true");
  }

  if (print_command == "") {
    print_command = default_command;
  }

  if (print_color) {
    dialog.colorRadioGroup.selectedItem = dialog.colorRadio;
  } else {
    dialog.colorRadioGroup.selectedItem = dialog.grayRadio;
  }

  dialog.downloadFonts.checked = print_downloadfonts;

  dialog.cmdInput.value      = print_command;
  dialog.jobTitleInput.value = print_jobtitle;

  /* First initalize with the hardcoded defaults... */
  dialog.topInput.value    = "0.04";
  dialog.bottomInput.value = "0.04";
  dialog.leftInput.value   = "0.04";
  dialog.rightInput.value  = "0.04";

  try {
    /* ... then try to get the generic settings ... */
    dialog.topInput.value    = gPrefs.getIntPref("print.print_edge_top") / 100.0;
    dialog.bottomInput.value = gPrefs.getIntPref("print.print_edge_bottom") / 100.0;
    dialog.leftInput.value   = gPrefs.getIntPref("print.print_edge_left") / 100.0;
    dialog.rightInput.value  = gPrefs.getIntPref("print.print_edge_right") / 100.0;

    /* ... and then the printer specific settings. */
    var printername = gPrintSettings.printerName;
    dialog.topInput.value    = gPrefs.getIntPref("print.printer_"+printername+".print_edge_top") / 100.0;
    dialog.bottomInput.value = gPrefs.getIntPref("print.printer_"+printername+".print_edge_bottom") / 100.0;
    dialog.leftInput.value   = gPrefs.getIntPref("print.printer_"+printername+".print_edge_left") / 100.0;
    dialog.rightInput.value  = gPrefs.getIntPref("print.printer_"+printername+".print_edge_right") / 100.0;
  } catch (e) {  }
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
  var print_paper_type        = gPrintSettingsInterface.kPaperSizeDefined;
  var print_paper_unit        = gPrintSettingsInterface.kPaperSizeInches;
  var print_paper_width       = 0.0;
  var print_paper_height      = 0.0;
  var print_paper_name        = "";
  var print_plex_name         = "";
  var print_resolution_name   = "";
  var print_colorspace        = "";

  if (gPrintSettings != null) {
    var paperSelectedInx = dialog.paperList.selectedIndex;
    var plexSelectedInx  = dialog.plexList.selectedIndex;
    var resolutionSelectedInx = dialog.resolutionList.selectedIndex;
    var colorspaceSelectedInx = dialog.colorspaceList.selectedIndex;
    if (gPaperArray[paperSelectedInx].inches) {
      print_paper_unit = gPrintSettingsInterface.kPaperSizeInches;
    } else {
      print_paper_unit = gPrintSettingsInterface.kPaperSizeMillimeters;
    }
    print_paper_width      = gPaperArray[paperSelectedInx].width;
    print_paper_height     = gPaperArray[paperSelectedInx].height;
    print_paper_name       = gPaperArray[paperSelectedInx].name;
    print_plex_name        = gPlexArray[plexSelectedInx].name;
    print_resolution_name  = gResolutionArray[resolutionSelectedInx].name;
    print_colorspace       = gColorspaceArray[colorspaceSelectedInx].name;

    gPrintSettings.paperSize       = gPaperArray[paperSelectedInx].paperSize; // deprecated
    gPrintSettings.paperSizeType   = print_paper_type;
    gPrintSettings.paperSizeUnit   = print_paper_unit;
    gPrintSettings.paperWidth      = print_paper_width;
    gPrintSettings.paperHeight     = print_paper_height;
    gPrintSettings.paperName       = print_paper_name;
    gPrintSettings.plexName        = print_plex_name;
    gPrintSettings.resolutionName  = print_resolution_name;
    gPrintSettings.colorspace      = print_colorspace;

    // save these out so they can be picked up by the device spec
    gPrintSettings.printInColor     = dialog.colorRadio.selected;
    gPrintSettings.downloadFonts    = dialog.downloadFonts.checked;
    gPrintSettings.printCommand     = dialog.cmdInput.value;
    gPrintSettings.title            = dialog.jobTitleInput.value;

    // 
    try {
      var printerName = gPrintSettings.printerName;
      var i = dialog.topInput.value * 100;
      gPrefs.setIntPref("print.printer_"+printerName+".print_edge_top", i);

      i = dialog.bottomInput.value * 100;
      gPrefs.setIntPref("print.printer_"+printerName+".print_edge_bottom", i);

      i = dialog.leftInput.value * 100;
      gPrefs.setIntPref("print.printer_"+printerName+".print_edge_left", i);

      i = dialog.rightInput.value * 100;
      gPrefs.setIntPref("print.printer_"+printerName+".print_edge_right", i);
    } catch (e) {
    }

    if (doDebug) {
      dump("onAccept******************************\n");
      dump("paperSize        "+gPrintSettings.paperSize+" (deprecated)\n");
      dump("paperSizeType    "+print_paper_type+" (should be 1)\n");
      dump("paperSizeUnit    "+print_paper_unit+"\n");
      dump("paperWidth       "+print_paper_width+"\n");
      dump("paperHeight      "+print_paper_height+"\n");
      dump("paperName       '"+print_paper_name+"'\n");
      dump("plexName        '"+print_plex_name+"'\n");
      dump("resolutionName  '"+print_resolution_name+"'\n");
      dump("colorspace      '"+print_colorspace+"'\n");

      dump("printInColor     "+gPrintSettings.printInColor+"\n");
      dump("downloadFonts    "+gPrintSettings.downloadFonts+"\n");
      dump("printCommand    '"+gPrintSettings.printCommand+"'\n");
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

