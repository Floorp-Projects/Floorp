# -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is 
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Blake Ross <blakeross@telocity.com>
#   Peter Annema <disttsc@bart.nl>
#   Samir Gehani <sgehani@netscape.com>
#   Pierre Chanial <p_ch@verizon.net>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK ***** */

var PrintUtils = {

  showPageSetup: function ()
  {
    var didOK = false;
    try {
      var printSettings = this.getPrintSettings();

      var webBrowserPrint = null;
      if (_content) {
        var ifreq = _content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
        webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);
      }

      var PRINTPROMPTSVC = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                                     .getService(Components.interfaces.nsIPrintingPromptService);
      didOK = PRINTPROMPTSVC.showPageSetup(window, printSettings, null);

      if (didOK) {
        if (webBrowserPrint) {
          var PREF = Components.classes["@mozilla.org/preferences-service;1"]
                               .getService(Components.interfaces.nsIPrefBranch);
          var isGlobal = PREF.getBoolPref("print.use_global_printsettings", false);
          var doSave   = PREF.getBoolPref("print.save_print_settings", false);

          if (isGlobal && doSave) {
            var PSSVC = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                                  .getService(Components.interfaces.nsIPrintSettingsService);
            PSSVC.savePrintSettingsToPrefs(printSettings, false, printSettings.kInitSaveNativeData);
          }
        }
      }
    } catch (e) {
      dump("showPageSetup "+e+"\n");
    }
    return didOK;
  },

  print: function ()
  {
    try {
      var ifreq = _content.QueryInterface(Components.interfaces.nsIInterfaceRequestor);
      var webBrowserPrint = ifreq.getInterface(Components.interfaces.nsIWebBrowserPrint);     
      if (webBrowserPrint) {
        var printSettings = this.getPrintSettings();
        webBrowserPrint.print(printSettings, null);
      }
    } catch (e) {
      // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
      // causing an exception to be thrown which we catch here.
      // Unfortunately this will also consume helpful failures, so add a
      // dump("print: "+e+"\n"); // if you need to debug
    }
  },


  ////////////////////////////////////////
  // "private" methods. Don't use them. //
  ////////////////////////////////////////

  setPrinterDefaultsForSelectedPrinter: function (aPRINTSVC, aPrintSettings)
  {
    if (!aPrintSettings.printerName)
      aPrintSettings.printerName = aPRINTSVC.defaultPrinterName;

    // First get any defaults from the printer 
    aPRINTSVC.initPrintSettingsFromPrinter(aPrintSettings.printerName, aPrintSettings);
    // now augment them with any values from last time
    aPRINTSVC.initPrintSettingsFromPrefs(aPrintSettings, true,  aPrintSettings.kInitSaveAll);
  },

  getPrintSettings: function ()
  {
    var printSettings;
    var PREF = Components.classes["@mozilla.org/preferences-service;1"]
                         .getService(Components.interfaces.nsIPrefBranch);
    try {
      var isGlobal = PREF.getBoolPref("print.use_global_printsettings", false);

      var PRINTSVC = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                               .getService(Components.interfaces.nsIPrintSettingsService);
      if (isGlobal) {
        printSettings = PRINTSVC.globalPrintSettings;
        this.setPrinterDefaultsForSelectedPrinter(PRINTSVC, printSettings);
      } else
        printSettings = PRINTSVC.newPrintSettings;
    } catch (e) {
      dump("getPrintSettings: "+e+"\n");
    }
    return printSettings;
  }
}
