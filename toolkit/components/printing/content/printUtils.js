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

var  XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

var PrintUtils = {

  showPageSetup: function ()
  {
    try {
      var printSettings = this.getPrintSettings();
      var PRINTPROMPTSVC = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                                     .getService(Components.interfaces.nsIPrintingPromptService);
      PRINTPROMPTSVC.showPageSetup(window, printSettings, null);
      this.savePrintSettings(printSettings);

    } catch (e) {
      dump("showPageSetup "+e+"\n");
      return false;
    }
    return true;
  },

  print: function ()
  {
    var webBrowserPrint = this.getWebBrowserPrint();
    var printSettings = this.getPrintSettings();
    try {
      webBrowserPrint.print(printSettings, null);
    } catch (e) {
      // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
      // causing an exception to be thrown which we catch here.
      // Unfortunately this will also consume helpful failures, so add a
      // dump("print: "+e+"\n"); // if you need to debug
    }
  },

  printPreview: function (aEnterPPCallback, aExitPPCallback)
  {
    // if we're in PP mode, don't copy the callbacks.
    if (!document.getElementById("print-preview-toolbar")) {
      this._onEnterPP = aEnterPPCallback;
      this._onExitPP  = aExitPPCallback;
    }
    this._webProgressPP = {};
    var ppParams        = {};
    var notifyOnOpen    = {};
    var webBrowserPrint = this.getWebBrowserPrint();
    var printSettings   = this.getPrintSettings();
    // Here we get the PrintingPromptService so we can display the PP Progress from script
    // For the browser implemented via XUL with the PP toolbar we cannot let it be
    // automatically opened from the print engine because the XUL scrollbars in the PP window
    // will layout before the content window and a crash will occur.
    // Doing it all from script, means it lays out before hand and we can let printing do it's own thing
    var PPROMPTSVC = Components.classes["@mozilla.org/embedcomp/printingprompt-service;1"]
                               .getService(Components.interfaces.nsIPrintingPromptService);
    // just in case we are already printing, 
    // an error code could be returned if the Prgress Dialog is already displayed
    try {
      PPROMPTSVC.showProgress(this, webBrowserPrint, printSettings, this._obsPP, false,
                              this._webProgressPP, ppParams, notifyOnOpen);
      if (ppParams.value) {
        var webNav = getBrowser().webNavigation;
        ppParams.value.docTitle = webNav.document.title;
        ppParams.value.docURL   = webNav.currentURI.spec;
      }

      // this tells us whether we should continue on with PP or 
      // wait for the callback via the observer
      if (!notifyOnOpen.value.valueOf() || this._webProgressPP.value == null)
        this.enterPrintPreview();
    } catch (e) {
      this.enterPrintPreview();
    }
  },

  getWebBrowserPrint: function ()
  {
    return _content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                   .getInterface(Components.interfaces.nsIWebBrowserPrint);
  },

  savePrintSettings: function (aPrintSettings)
  {
    var PSSVC = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                          .getService(Components.interfaces.nsIPrintSettingsService);
    PSSVC.savePrintSettingsToPrefs(aPrintSettings, false, aPrintSettings.kInitSaveNativeData);
  },

  ////////////////////////////////////////
  // "private" methods. Don't use them. //
  ////////////////////////////////////////

  setPrinterDefaultsForSelectedPrinter: function (aPSSVC, aPrintSettings)
  {
    if (!aPrintSettings.printerName)
      aPrintSettings.printerName = aPSSVC.defaultPrinterName;

    // First get any defaults from the printer 
    aPSSVC.initPrintSettingsFromPrinter(aPrintSettings.printerName, aPrintSettings);
    // now augment them with any values from last time
    aPSSVC.initPrintSettingsFromPrefs(aPrintSettings, true,  aPrintSettings.kInitSaveAll);
  },

  getPrintSettings: function ()
  {
    var printSettings;
    try {
      var PSSVC = Components.classes["@mozilla.org/gfx/printsettings-service;1"]
                            .getService(Components.interfaces.nsIPrintSettingsService);
      printSettings = PSSVC.globalPrintSettings;
      this.setPrinterDefaultsForSelectedPrinter(PSSVC, printSettings);
    } catch (e) {
      dump("getPrintSettings: "+e+"\n");
    }
    return printSettings;
  },

  _chromeState: {},
  _closeHandlerPP: null,
  _webProgressPP: null,
  _onEnterPP: null,
  _onExitPP: null,

  // This observer is called once the progress dialog has been "opened"
  _obsPP: 
  {
    observe: function(aSubject, aTopic, aData)
    {
      // delay the print preview to show the content of the progress dialog
      setTimeout("PrintUtils.enterPrintPreview();", 0);
    },

    QueryInterface : function(iid)
    {
      if (iid.equals(Components.interfaces.nsIObserver) || iid.equals(Components.interfaces.nsISupportsWeakReference))
        return this;   
      throw Components.results.NS_NOINTERFACE;
    }
  },

  enterPrintPreview: function ()
  {
    var webBrowserPrint = this.getWebBrowserPrint();
    var printSettings   = this.getPrintSettings();
    try {
      webBrowserPrint.printPreview(printSettings, null, this._webProgressPP.value);
    } catch (e) {
      // Pressing cancel is expressed as an NS_ERROR_ABORT return value,
      // causing an exception to be thrown which we catch here.
      // Unfortunately this will also consume helpful failures, so add a
      // dump(e); // if you need to debug
    }

    var printPreviewTB = document.getElementById("print-preview-toolbar");
    if (printPreviewTB) {
      printPreviewTB.updateToolbar();
      return;
    }

    // show the toolbar after we go into print preview mode so
    // that we can initialize the toolbar with total num pages
    printPreviewTB = document.createElementNS(XUL_NS, "toolbar");
    printPreviewTB.setAttribute("printpreview", true);
    printPreviewTB.setAttribute("id", "print-preview-toolbar");

#ifdef MOZ_PHOENIX
    getBrowser().parentNode.insertBefore(printPreviewTB, getBrowser());

    // Tab browser...
    if ("getStripVisibility" in getBrowser()) {
      this._chromeState.hadTabStrip = getBrowser().getStripVisibility();
      getBrowser().setStripVisibilityTo(false);
    }
#endif

    // copy the window close handler
    if (document.documentElement.hasAttribute("onclose"))
      this._closeHandlerPP = document.documentElement.getAttribute("onclose");
    else
      this._closeHandlerPP = null;
    document.documentElement.setAttribute("onclose", "PrintUtils.exitPrintPreview(); return false;");

    // disable chrome shortcuts...
    window.addEventListener("keypress", this.onKeyPressPP, true);
 
    _content.focus();
    
    // on Enter PP Call back
    if (this._onEnterPP) {
      this._onEnterPP();
      this._onEnterPP = null;
    }
  },

  exitPrintPreview: function ()
  {
    window.removeEventListener("keypress", this.onKeyPressPP, true);

#ifdef MOZ_THUNDERBIRD
    BrowserExitPrintPreview(); // make the traditional call..don't do any of the inline toolbar browser stuff
    return;
#endif

    // restore the old close handler
    document.documentElement.setAttribute("onclose", this._closeHandlerPP);
    this._closeHandlerPP = null;

    if ("getStripVisibility" in getBrowser())
      getBrowser().setStripVisibilityTo(this._chromeState.hadTabStrip);

    var webBrowserPrint = this.getWebBrowserPrint();
    webBrowserPrint.exitPrintPreview(); 

    // remove the print preview toolbar
    var printPreviewTB = document.getElementById("print-preview-toolbar");
    getBrowser().parentNode.removeChild(printPreviewTB);

    _content.focus();

    // on Exit PP Call back
    if (this._onExitPP) {
      this._onExitPP();
      this._onExitPP = null;
    }
  },

  onKeyPressPP: function (aEvent)
  {
    var closeKey;
    try {
      closeKey = document.getElementById("key_close")
                         .getAttribute("key");
      closeKey = aEvent["DOM_VK_"+closeKey];
    } catch (e) {}
    var isModif = aEvent.ctrlKey || aEvent.metaKey;
    // ESC and Ctrl-W exits the PP
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE || isModif &&
        (aEvent.charCode == closeKey || aEvent.charCode == closeKey + 32))
      PrintUtils.exitPrintPreview();
    // cancel shortkeys
    if (isModif) {
      aEvent.preventDefault();
      aEvent.stopPropagation();
    }
  }
}
