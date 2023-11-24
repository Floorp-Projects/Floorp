/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  assert: "chrome://remote/content/shared/webdriver/Assert.sys.mjs",
  Log: "chrome://remote/content/shared/Log.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "logger", () => lazy.Log.get());

export const print = {
  maxScaleValue: 2.0,
  minScaleValue: 0.1,
};

print.defaults = {
  // The size of the page in centimeters.
  page: {
    width: 21.59,
    height: 27.94,
  },
  margin: {
    top: 1.0,
    bottom: 1.0,
    left: 1.0,
    right: 1.0,
  },
  orientationValue: ["landscape", "portrait"],
};

print.addDefaultSettings = function (settings) {
  const {
    background = false,
    margin = {},
    orientation = "portrait",
    page = {},
    pageRanges = [],
    scale = 1.0,
    shrinkToFit = true,
  } = settings;

  lazy.assert.object(page, `Expected "page" to be a object, got ${page}`);
  lazy.assert.object(margin, `Expected "margin" to be a object, got ${margin}`);

  if (!("width" in page)) {
    page.width = print.defaults.page.width;
  }

  if (!("height" in page)) {
    page.height = print.defaults.page.height;
  }

  if (!("top" in margin)) {
    margin.top = print.defaults.margin.top;
  }

  if (!("bottom" in margin)) {
    margin.bottom = print.defaults.margin.bottom;
  }

  if (!("right" in margin)) {
    margin.right = print.defaults.margin.right;
  }

  if (!("left" in margin)) {
    margin.left = print.defaults.margin.left;
  }

  return {
    background,
    margin,
    orientation,
    page,
    pageRanges,
    scale,
    shrinkToFit,
  };
};

print.getPrintSettings = function (settings) {
  const psService = Cc["@mozilla.org/gfx/printsettings-service;1"].getService(
    Ci.nsIPrintSettingsService
  );

  let cmToInches = cm => cm / 2.54;
  const printSettings = psService.createNewPrintSettings();
  printSettings.isInitializedFromPrinter = true;
  printSettings.isInitializedFromPrefs = true;
  printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;
  printSettings.printerName = "marionette";
  printSettings.printSilent = true;

  // Setting the paperSizeUnit to kPaperSizeMillimeters doesn't work on mac
  printSettings.paperSizeUnit = Ci.nsIPrintSettings.kPaperSizeInches;
  printSettings.paperWidth = cmToInches(settings.page.width);
  printSettings.paperHeight = cmToInches(settings.page.height);
  printSettings.usePageRuleSizeAsPaperSize = true;

  printSettings.marginBottom = cmToInches(settings.margin.bottom);
  printSettings.marginLeft = cmToInches(settings.margin.left);
  printSettings.marginRight = cmToInches(settings.margin.right);
  printSettings.marginTop = cmToInches(settings.margin.top);

  printSettings.printBGColors = settings.background;
  printSettings.printBGImages = settings.background;
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

  if (settings.orientation === "landscape") {
    printSettings.orientation = Ci.nsIPrintSettings.kLandscapeOrientation;
  }

  if (settings.pageRanges?.length) {
    printSettings.pageRanges = parseRanges(settings.pageRanges);
  }

  return printSettings;
};

/**
 * Convert array of strings of the form ["1-3", "2-4", "7", "9-"] to an flat array of
 * limits, like [1, 4, 7, 7, 9, 2**31 - 1] (meaning 1-4, 7, 9-end)
 *
 * @param {Array.<string|number>} ranges
 *     Page ranges to print, e.g., ['1-5', '8', '11-13'].
 *     Defaults to the empty string, which means print all pages.
 *
 * @returns {Array.<number>}
 *     Even-length array containing page range limits
 */
function parseRanges(ranges) {
  const MAX_PAGES = 0x7fffffff;

  if (ranges.length === 0) {
    return [];
  }

  let allLimits = [];

  for (let range of ranges) {
    let limits;
    if (typeof range !== "string") {
      // We got a single integer so the limits are just that page
      lazy.assert.positiveInteger(range);
      limits = [range, range];
    } else {
      // We got a string presumably of the form <int> | <int>? "-" <int>?
      const msg = `Expected a range of the form <int> or <int>-<int>, got ${range}`;

      limits = range.split("-").map(x => x.trim());
      lazy.assert.that(o => [1, 2].includes(o.length), msg)(limits);

      // Single numbers map to a range with that page at the start and the end
      if (limits.length == 1) {
        limits.push(limits[0]);
      }

      // Need to check that both limits are strings conisting only of
      // decimal digits (or empty strings)
      const assertNumeric = lazy.assert.that(o => /^\d*$/.test(o), msg);
      limits.every(x => assertNumeric(x));

      // Convert from strings representing numbers to actual numbers
      // If we don't have an upper bound, choose something very large;
      // the print code will later truncate this to the number of pages
      limits = limits.map((limitStr, i) => {
        if (limitStr == "") {
          return i == 0 ? 1 : MAX_PAGES;
        }
        return parseInt(limitStr);
      });
    }
    lazy.assert.that(
      x => x[0] <= x[1],
      "Lower limit ${parts[0]} is higher than upper limit ${parts[1]}"
    )(limits);

    allLimits.push(limits);
  }
  // Order by lower limit
  allLimits.sort((a, b) => a[0] - b[0]);
  let parsedRanges = [allLimits.shift()];
  for (let limits of allLimits) {
    let prev = parsedRanges[parsedRanges.length - 1];
    let prevMax = prev[1];
    let [min, max] = limits;
    if (min <= prevMax) {
      // min is inside previous range, so extend the max if needed
      if (max > prevMax) {
        prev[1] = max;
      }
    } else {
      // Otherwise we have a new range
      parsedRanges.push(limits);
    }
  }

  let rv = parsedRanges.flat();
  lazy.logger.debug(`Got page ranges [${rv.join(", ")}]`);
  return rv;
}

print.printToBinaryString = async function (browsingContext, printSettings) {
  // Create a stream to write to.
  const stream = Cc["@mozilla.org/storagestream;1"].createInstance(
    Ci.nsIStorageStream
  );
  stream.init(4096, 0xffffffff);

  printSettings.outputDestination =
    Ci.nsIPrintSettings.kOutputDestinationStream;
  printSettings.outputStream = stream.getOutputStream(0);

  await browsingContext.print(printSettings);

  const inputStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );

  inputStream.setInputStream(stream.newInputStream(0));

  const available = inputStream.available();
  const bytes = inputStream.readBytes(available);

  stream.close();

  return bytes;
};
