/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Masaki Katakai <katakai@japan.sun.com>
 *   Jessica Blanco <jblanco@us.ibm.com>
 *   Asko Tontti <atontti@cc.hut.fi>
 */

var dialog;
var printService       = null;

var default_file       = "mozilla.ps";
var gPrintOptInterface = Components.interfaces.nsIPrintOptions;
var doDebug            = false;

//---------------------------------------------------
function initDialog()
{
  dialog = new Object;

  dialog.propertiesButton = document.getElementById("properties");

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
function checkInteger(elementID)
{
  var integerField = document.getElementById( elementID );
  if ( !integerField )
    return;
  var value = integerField.value;
  if (value && value.length > 0) {
    value = value.replace(/[^0-9]/g,"");
    if (!value) value = "";
    integerField.value = value;
  }
  if (!value || value < 1 || value > 999)
    dialog.printButton.setAttribute("disabled","true" );
  else
    dialog.printButton.removeAttribute("disabled");
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
    dialog.fileInput.removeAttribute("disabled");
    dialog.chooseButton.removeAttribute("disabled");
  } else {
    dialog.fileLabel.setAttribute("disabled","true");
    dialog.fileInput.setAttribute("disabled","true");
    dialog.chooseButton.setAttribute("disabled","true");
  }
}

//---------------------------------------------------
function listElement( aListElement )
  {
    this.listElement = aListElement;
  }

listElement.prototype =
  {
    clearList:
      function ()
        {
          // remove the menupopup node child of the menulist.
          this.listElement.removeChild( this.listElement.firstChild );
        },

    appendPrinterNames: 
      function ( aDataObject ) 
        { 
          var popupNode = document.createElement( "menupopup" ); 
          var strDefaultPrinterName = "";
          var printerName;
          while (aDataObject.hasMoreElements()) {
            printerName = aDataObject.getNext();
            printerName = printerName.QueryInterface(Components.interfaces.nsISupportsWString);
            var printerNameStr = printerName.toString();
            if (strDefaultPrinterName == "")
               strDefaultPrinterName = printerNameStr;
            var itemNode = document.createElement( "menuitem" );
            itemNode.setAttribute( "value", printerNameStr );
            itemNode.setAttribute( "label", printerNameStr );
            popupNode.appendChild( itemNode );
          }
          if (strDefaultPrinterName != "") {
            this.listElement.removeAttribute( "disabled" );
          } else {
            var stringBundle = srGetStrBundle("chrome://communicator/locale/printing.properties");
            this.listElement.setAttribute( "value", strDefaultPrinterName );
            this.listElement.setAttribute( "label", stringBundle.GetStringFromName("noprinter") );
            this.listElement.setAttribute( "disabled", "true" );
            dialog.destGroup.setAttribute("disabled","true");
            dialog.printerRadio.setAttribute("disabled","true");
            dialog.printerLabel.setAttribute("disabled","true");
            dialog.propertiesButton.setAttribute("disabled","true");
            dialog.fileRadio.setAttribute("disabled","true");
            doPrintToFile(false);
            dialog.printButton.setAttribute("disabled","true");
          }
          this.listElement.appendChild( popupNode ); 
          return strDefaultPrinterName;
        } 
  };

//---------------------------------------------------
function getPrinters( )
{
  var printerEnumerator = printService.availablePrinters();
  var selectElement = new listElement(dialog.printerList);
  selectElement.clearList();
  var strDefaultPrinterName = selectElement.appendPrinterNames(printerEnumerator);

  selectElement.listElement.value = strDefaultPrinterName;  

}   

//---------------------------------------------------
function displayPropertiesDialog( )
{
  var displayed = new Object;
  displayed.value = false;
  printService.displayJobProperties(dialog.printerList.value, displayed);

  if (doDebug) {
    if (displayed)
      dump("\nproperties dlg came up. displayed = "+displayed.value+"\n");
    else
      dump("\nproperties dlg didn't come up. displayed = "+displayed.value+"\n");
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
  inx = str.indexOf(".");
  return str.substring(0, inx+dec+1);
}

//---------------------------------------------------
function loadDialog()
{
  var print_tofile        = false;
  var print_copies        = 1;
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
    print_copies    = printService.numCopies;
    print_file      = printService.toFileName;
    print_tofile    = printService.printToFile;
    print_frametype = printService.printFrameType;
    print_howToEnableUI = printService.howToEnableFrameUI;
    print_selection_radio_enabled = printService.GetPrintOptions(gPrintOptInterface.kPrintOptionsEnableSelectionRB);
  }

  if (doDebug) {
    dump("toFileName    "+print_file+"\n");
    dump("printToFile   "+print_tofile+"\n");
    dump("printToFile   "+print_tofile+"\n");
    dump("print_frame   "+print_frametype+"\n");
    dump("print_howToEnableUI "+print_howToEnableUI+"\n");

    dump("selection_radio_enabled "+print_selection_radio_enabled+"\n");
  }

  if (print_file == "") {
    print_file = default_file;
  }

  if (print_tofile) {
    dialog.destGroup.selectedItem = dialog.fileRadio;
    doPrintToFile( true );
  } else {
    dialog.destGroup.selectedItem = dialog.printerRadio;
    doPrintToFile( false );
  }

  dialog.printrangeGroup.selectedItem = dialog.allpagesRadio;
  if ( print_selection_radio_enabled) {
    dialog.selectionRadio.removeAttribute("disabled");
  } else {
    dialog.selectionRadio.setAttribute("disabled","true" );
  }
  doPrintRange(dialog.rangeRadio.selected);
  dialog.frompageInput.value  = 1;
  dialog.topageInput.value    = 1;
  dialog.numCopiesInput.value = 1;

  dialog.fileInput.value   = print_file;

  getPrinters();

  if (doDebug) {
    dump("print_howToEnableUI: "+print_howToEnableUI+"\n");
  }

  // print frame
  if (print_howToEnableUI == gPrintOptInterface.kFrameEnableAll) {
    dialog.aslaidoutRadio.removeAttribute("disabled");

    dialog.selectedframeRadio.removeAttribute("disabled");
    dialog.eachframesepRadio.removeAttribute("disabled");
    dialog.printframeGroupLabel.removeAttribute("disabled");

    // initialize radio group
    dialog.printframeGroup.selectedItem = dialog.selectedframeRadio;

  } else if (print_howToEnableUI == gPrintOptInterface.kFrameEnableAsIsAndEach) {
    dialog.aslaidoutRadio.removeAttribute("disabled");       //enable

    dialog.selectedframeRadio.setAttribute("disabled","true" ); // disable
    dialog.eachframesepRadio.removeAttribute("disabled");       // enable
    dialog.printframeGroupLabel.removeAttribute("disabled");    // enable

    // initialize
    dialog.printframeGroup.selectedItem = dialog.eachframesepRadio;

  } else {
    dialog.aslaidoutRadio.setAttribute("disabled","true" );
    dialog.selectedframeRadio.setAttribute("disabled","true" );
    dialog.eachframesepRadio.setAttribute("disabled","true" );
    dialog.printframeGroupLabel.setAttribute("disabled","true" );
  }

  dialog.printButton.label = dialog.printName.getAttribute("label");
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

  if (printService) {
    var print_howToEnableUI = gPrintOptInterface.kFrameEnableNone;

    printService.printToFile   = dialog.fileRadio.selected;
    print_howToEnableUI        = printService.howToEnableFrameUI;

    // save these out so they can be picked up by the device spec
    printService.toFileName   = dialog.fileInput.value;
    printService.printer      = dialog.printerList.value;

    if (dialog.allpagesRadio.selected) {
      printService.printRange = gPrintOptInterface.kRangeAllPages;
    } else if (dialog.rangeRadio.selected) {
      printService.printRange = gPrintOptInterface.kRangeSpecifiedPageRange;
    } else if (dialog.selectionRadio.selected) {
      printService.printRange = gPrintOptInterface.kRangeSelection;
    }
    printService.startPageRange = dialog.frompageInput.value;
    printService.endPageRange   = dialog.topageInput.value;
    printService.numCopies      = dialog.numCopiesInput.value;

    var frametype = gPrintOptInterface.kNoFrames;
    if (print_howToEnableUI != gPrintOptInterface.kFrameEnableNone) {
      if (dialog.aslaidoutRadio.selected) {
        frametype = gPrintOptInterface.kFramesAsIs;
      } else if (dialog.selectedframeRadio.selected) {
        frametype = gPrintOptInterface.kSelectedFrame;
      } else if (dialog.eachframesepRadio.selected) {
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
  if (dialog.fileRadio.selected == false) {
    return;
  }
  try {
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, dialog.fpDialog.getAttribute("label"), nsIFilePicker.modeSave);
    fp.appendFilters(nsIFilePicker.filterAll);
    fp.show();
    if (fp.file && fp.file.path.length > 0) {
      dialog.fileInput.value = fp.file.path;
    }
  } catch(ex) {
    dump(ex);
  }
}

