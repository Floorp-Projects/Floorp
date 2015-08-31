// -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var PrintHelper = {
  init: function() {
    Services.obs.addObserver(this, "Print:PDF", false);
  },

  observe: function (aSubject, aTopic, aData) {
    let browser = BrowserApp.selectedBrowser;

    switch (aTopic) {
      case "Print:PDF":
        Messaging.handleRequest(aTopic, aData, (data) => {
          return this.generatePDF(browser);
        });
        break;
    }
  },

  generatePDF: function(aBrowser) {
    // Create the final destination file location
    let fileName = ContentAreaUtils.getDefaultFileName(aBrowser.contentTitle, aBrowser.currentURI, null, null);
    fileName = fileName.trim() + ".pdf";

    let file = Services.dirsvc.get("TmpD", Ci.nsIFile);
    file.append(fileName);
    file.createUnique(file.NORMAL_FILE_TYPE, parseInt("666", 8));

    let printSettings = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(Ci.nsIPrintSettingsService).newPrintSettings;
    printSettings.printSilent = true;
    printSettings.showPrintProgress = false;
    printSettings.printBGImages = false;
    printSettings.printBGColors = false;
    printSettings.printToFile = true;
    printSettings.toFileName = file.path;
    printSettings.printFrameType = Ci.nsIPrintSettings.kFramesAsIs;
    printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;

    let webBrowserPrint = aBrowser.contentWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIWebBrowserPrint);

    return new Promise((resolve, reject) => {
      webBrowserPrint.print(printSettings, {
        onStateChange: function(webProgress, request, stateFlags, status) {
          // We get two STATE_STOP calls, one for STATE_IS_DOCUMENT and one for STATE_IS_NETWORK
          if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP && stateFlags & Ci.nsIWebProgressListener.STATE_IS_NETWORK) {
            if (Components.isSuccessCode(status)) {
              // Send the details to Java
              resolve({ file: file.path, title: fileName });
            } else {
              reject();
            }
          }
        },
        onProgressChange: function () {},
        onLocationChange: function () {},
        onStatusChange: function () {},
        onSecurityChange: function () {},
      });
    });
  }
};
