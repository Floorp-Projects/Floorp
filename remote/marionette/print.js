/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["print"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  clearInterval: "resource://gre/modules/Timer.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  setInterval: "resource://gre/modules/Timer.jsm",

  assert: "chrome://marionette/content/assert.js",
  Log: "chrome://marionette/content/log.js",
  pprint: "chrome://marionette/content/format.js",
});

XPCOMUtils.defineLazyGetter(this, "logger", () => Log.get());

this.print = {
  maxScaleValue: 2.0,
  minScaleValue: 0.1,
  letterPaperSizeCm: {
    width: 21.59,
    height: 27.94,
  },
};

print.addDefaultSettings = function(settings) {
  const {
    landscape = false,
    margin = {
      top: 1,
      bottom: 1,
      left: 1,
      right: 1,
    },
    page = print.letterPaperSizeCm,
    shrinkToFit = true,
    printBackground = false,
    scale = 1.0,
  } = settings;
  return { landscape, margin, page, shrinkToFit, printBackground, scale };
};

function getPrintSettings(settings, filePath) {
  const psService = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
    Ci.nsIPrintSettingsService
  );

  let cmToInches = cm => cm / 2.54;
  const printSettings = psService.newPrintSettings;
  printSettings.isInitializedFromPrinter = true;
  printSettings.isInitializedFromPrefs = true;
  printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;
  printSettings.printerName = "marionette";
  printSettings.printSilent = true;
  printSettings.printToFile = true;
  printSettings.showPrintProgress = false;
  printSettings.toFileName = filePath;

  // Setting the paperSizeUnit to kPaperSizeMillimeters doesn't work on mac
  printSettings.paperSizeUnit = Ci.nsIPrintSettings.kPaperSizeInches;
  printSettings.paperWidth = cmToInches(settings.page.width);
  printSettings.paperHeight = cmToInches(settings.page.height);

  printSettings.marginBottom = cmToInches(settings.margin.bottom);
  printSettings.marginLeft = cmToInches(settings.margin.left);
  printSettings.marginRight = cmToInches(settings.margin.right);
  printSettings.marginTop = cmToInches(settings.margin.top);

  printSettings.printBGColors = settings.printBackground;
  printSettings.printBGImages = settings.printBackground;
  printSettings.scaling = settings.scale;
  printSettings.shrinkToFit = settings.shrinkToFit;

  printSettings.headerStrCenter = "";
  printSettings.headerStrLeft = "";
  printSettings.headerStrRight = "";
  printSettings.footerStrCenter = "";
  printSettings.footerStrLeft = "";
  printSettings.footerStrRight = "";

  // Override any os-specific unwriteable margins
  printSettings.unwriteableMarginTop = 0;
  printSettings.unwriteableMarginLeft = 0;
  printSettings.unwriteableMarginBottom = 0;
  printSettings.unwriteableMarginRight = 0;

  if (settings.landscape) {
    printSettings.orientation = Ci.nsIPrintSettings.kLandscapeOrientation;
  }
  return printSettings;
}

print.printToFile = async function(browser, settings) {
  // Create a unique filename for the temporary PDF file
  const basePath = OS.Path.join(OS.Constants.Path.tmpDir, "marionette.pdf");
  const { file, path: filePath } = await OS.File.openUnique(basePath);
  await file.close();

  let printSettings = getPrintSettings(settings, filePath);

  await browser.browsingContext.print(printSettings);

  // Bug 1603739 - With e10s enabled the promise returned by print() resolves
  // too early, which means the file hasn't been completely written.
  await new Promise(resolve => {
    const DELAY_CHECK_FILE_COMPLETELY_WRITTEN = 100;

    let lastSize = 0;
    const timerId = setInterval(async () => {
      const fileInfo = await OS.File.stat(filePath);
      if (lastSize > 0 && fileInfo.size == lastSize) {
        clearInterval(timerId);
        resolve();
      }
      lastSize = fileInfo.size;
    }, DELAY_CHECK_FILE_COMPLETELY_WRITTEN);
  });

  logger.debug(`PDF output written to ${filePath}`);
  return filePath;
};
