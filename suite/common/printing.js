/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Blake Ross <blakeross@telocity.com>
 *   Peter Annema <disttsc@bart.nl>
 *   Samir Gehani <sgehani@netscape.com>
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

var gPrintSettings = null;
var gPrintSettingsAreGlobal = false;
var gSavePrintSettings = false;

function setPrinterDefaultsForSelectedPrinter(aPrintService)
{
  if (gPrintSettings.printerName == "") {
    gPrintSettings.printerName = aPrintService.defaultPrinterName;
  }

  // First get any defaults from the printer 
  aPrintService.initPrintSettingsFromPrinter(gPrintSettings.printerName, gPrintSettings);

  // now augment them with any values from last time
  aPrintService.initPrintSettingsFromPrefs(gPrintSettings, true, gPrintSettings.kInitSaveAll);
}

function GetPrintSettings()
{
  try {
    if (gPrintSettings == null) {
      var pref = Components.classes["@mozilla.org/preferences-service;1"]
                           .getService(Components.interfaces.nsIPrefBranch);
      if (pref) {
        gPrintSettingsAreGlobal = pref.getBoolPref("print.use_global_printsettings", false);
        gSavePrintSettings = pref.getBoolPref("print.save_print_settings", false);
      }

      var printService = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                                        .getService(Components.interfaces.nsIPrintSettingsService);
      if (gPrintSettingsAreGlobal) {
        gPrintSettings = printService.globalPrintSettings;
        setPrinterDefaultsForSelectedPrinter(printService);
      } else {
        gPrintSettings = printService.newPrintSettings;
      }
    }
  } catch (e) {
    dump("GetPrintSettings() "+e+"\n");
  }

  return gPrintSettings;
}

function goPageSetup(domwin, printSettings)
{
  try {
    if (printSettings == null) {
      dump("***************** PrintSettings arg is null!");
      return false;
    }

    // This code calls the printoptions service to bring up the printoptions
    // dialog.  This will be an xp dialog if the platform did not override
    // the ShowPrintSetupDialog method.
    var printingPromptService = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                                             .getService(Components.interfaces.nsIPrintingPromptService);
    printingPromptService.showPageSetup(domwin, printSettings, null);
  } catch(e) {
    // Note: Pressing Cancel gets here too.

    return false; 
  }
  return true;
}

function NSPrintSetup()
{
  var didOK = false;
  try {
    gPrintSettings = GetPrintSettings();

    var webBrowserPrint = null;
    if (content) {
      webBrowserPrint = content
        .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
        .getInterface(Components.interfaces.nsIWebBrowserPrint);
    }

    didOK = goPageSetup(window, gPrintSettings);
    if (didOK) {
      if (webBrowserPrint) {
        if (gPrintSettingsAreGlobal && gSavePrintSettings) {
          var psService = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                                            .getService(Components.interfaces.nsIPrintSettingsService);
          psService.savePrintSettingsToPrefs(gPrintSettings, false, gPrintSettings.kInitSaveNativeData);
        }
      }
    }
  } catch (e) {
    dump("NSPrintSetup() " + e + "\n");
  }
  return didOK;
}

function NSPrint()
{
  try {
    var webBrowserPrint = content
          .QueryInterface(Components.interfaces.nsIInterfaceRequestor)
          .getInterface(Components.interfaces.nsIWebBrowserPrint);
    if (webBrowserPrint) {
      gPrintSettings = GetPrintSettings();
      webBrowserPrint.print(gPrintSettings, null);
    }
  } catch (e) {
    // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
    // causing an exception to be thrown which we catch here.
    // Unfortunately this will also consume helpful failures, so add a
    // dump(e); // if you need to debug
  }
}
