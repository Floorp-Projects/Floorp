#filter substitution
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

const Telemetry = Services.telemetry;
const bundle = Services.strings.createBundle(
  "chrome://global/locale/aboutTelemetry.properties");
const brandBundle = Services.strings.createBundle(
  "chrome://branding/locale/brand.properties");
const TelemetryPing = Cc["@mozilla.org/base/telemetry-ping;1"].
  getService(Ci.nsITelemetryPing);

// Maximum height of a histogram bar (in em)
const MAX_BAR_HEIGHT = 18;
const PREF_TELEMETRY_SERVER_OWNER = "toolkit.telemetry.server_owner";
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabledPreRelease";
const PREF_TELEMETRY_DISPLAYED = "toolkit.telemetry.notifiedOptOut";
#else
const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";
const PREF_TELEMETRY_DISPLAYED = "toolkit.telemetry.prompted";
#endif
const PREF_TELEMETRY_REJECTED  = "toolkit.telemetry.rejected";
const TELEMETRY_DISPLAY_REV = @MOZ_TELEMETRY_DISPLAY_REV@;
const PREF_DEBUG_SLOW_SQL = "toolkit.telemetry.debugSlowSql";
const PREF_SYMBOL_SERVER_URI = "profiler.symbolicationUrl";
const DEFAULT_SYMBOL_SERVER_URI = "http://symbolapi.mozilla.org";

// Cached value of document's RTL mode
let documentRTLMode = "";

/**
 * Helper function for fetching a config pref
 *
 * @param aPrefName Name of config pref to fetch.
 * @param aDefault Default value to return if pref isn't set.
 * @return Value of pref
 */
function getPref(aPrefName, aDefault) {
  let result = aDefault;

  try {
    let prefType = Services.prefs.getPrefType(aPrefName);
    if (prefType == Ci.nsIPrefBranch.PREF_BOOL) {
      result = Services.prefs.getBoolPref(aPrefName);
    } else if (prefType == Ci.nsIPrefBranch.PREF_STRING) {
      result = Services.prefs.getCharPref(aPrefName);
    }
  } catch (e) {
    // Return default if Prefs service throws exception
  }

  return result;
}

/**
 * Helper function for determining whether the document direction is RTL.
 * Caches result of check on first invocation.
 */
function isRTL() {
  if (!documentRTLMode)
    documentRTLMode = window.getComputedStyle(document.body).direction;
  return (documentRTLMode == "rtl");
}

let observer = {

  enableTelemetry: bundle.GetStringFromName("enableTelemetry"),

  disableTelemetry: bundle.GetStringFromName("disableTelemetry"),

  /**
   * Observer is called whenever Telemetry is enabled or disabled
   */
  observe: function observe(aSubject, aTopic, aData) {
    if (aData == PREF_TELEMETRY_ENABLED) {
      this.updatePrefStatus();
      Services.prefs.setBoolPref(PREF_TELEMETRY_REJECTED,
                                 !getPref(PREF_TELEMETRY_ENABLED, false));
      Services.prefs.setIntPref(PREF_TELEMETRY_DISPLAYED,
                                TELEMETRY_DISPLAY_REV);
    }
  },

  /**
   * Updates the button & text at the top of the page to reflect Telemetry state.
   */
  updatePrefStatus: function updatePrefStatus() {
    // Notify user whether Telemetry is enabled
    let enabledElement = document.getElementById("description-enabled");
    let disabledElement = document.getElementById("description-disabled");
    let toggleElement = document.getElementById("toggle-telemetry");
    if (getPref(PREF_TELEMETRY_ENABLED, false)) {
      enabledElement.classList.remove("hidden");
      disabledElement.classList.add("hidden");
      toggleElement.innerHTML = this.disableTelemetry;
    } else {
      enabledElement.classList.add("hidden");
      disabledElement.classList.remove("hidden");
      toggleElement.innerHTML = this.enableTelemetry;
    }
  }
};

let SlowSQL = {

  slowSqlHits: bundle.GetStringFromName("slowSqlHits"),

  slowSqlAverage: bundle.GetStringFromName("slowSqlAverage"),

  slowSqlStatement: bundle.GetStringFromName("slowSqlStatement"),

  mainThreadTitle: bundle.GetStringFromName("slowSqlMain"),

  otherThreadTitle: bundle.GetStringFromName("slowSqlOther"),

  /**
   * Render slow SQL statistics
   */
  render: function SlowSQL_render() {
    let debugSlowSql = getPref(PREF_DEBUG_SLOW_SQL, false);
    let {mainThread, otherThreads} =
      Telemetry[debugSlowSql ? "debugSlowSQL" : "slowSQL"];

    let mainThreadCount = Object.keys(mainThread).length;
    let otherThreadCount = Object.keys(otherThreads).length;
    if (mainThreadCount == 0 && otherThreadCount == 0) {
      showEmptySectionMessage("slow-sql-section");
      return;
    }

    if (debugSlowSql) {
      document.getElementById("sql-warning").classList.remove("hidden");
    }

    let slowSqlDiv = document.getElementById("slow-sql-tables");

    // Main thread
    if (mainThreadCount > 0) {
      let table = document.createElement("table");
      this.renderTableHeader(table, this.mainThreadTitle);
      this.renderTable(table, mainThread);

      slowSqlDiv.appendChild(table);
      slowSqlDiv.appendChild(document.createElement("hr"));
    }

    // Other threads
    if (otherThreadCount > 0) {
      let table = document.createElement("table");
      this.renderTableHeader(table, this.otherThreadTitle);
      this.renderTable(table, otherThreads);

      slowSqlDiv.appendChild(table);
      slowSqlDiv.appendChild(document.createElement("hr"));
    }
  },

  /**
   * Creates a header row for a Slow SQL table
   *
   * @param aTable Parent table element
   * @param aTitle Table's title
   */
  renderTableHeader: function SlowSQL_renderTableHeader(aTable, aTitle) {
    let caption = document.createElement("caption");
    caption.appendChild(document.createTextNode(aTitle));
    aTable.appendChild(caption);

    let headings = document.createElement("tr");
    this.appendColumn(headings, "th", this.slowSqlHits);
    this.appendColumn(headings, "th", this.slowSqlAverage);
    this.appendColumn(headings, "th", this.slowSqlStatement);
    aTable.appendChild(headings);
  },

  /**
   * Fills out the table body
   *
   * @param aTable Parent table element
   * @param aSql SQL stats object
   */
  renderTable: function SlowSQL_renderTable(aTable, aSql) {
    for (let [sql, [hitCount, totalTime]] of Iterator(aSql)) {
      let averageTime = totalTime / hitCount;

      let sqlRow = document.createElement("tr");

      this.appendColumn(sqlRow, "td", hitCount);
      this.appendColumn(sqlRow, "td", averageTime.toFixed(0));
      this.appendColumn(sqlRow, "td", sql);

      aTable.appendChild(sqlRow);
    }
  },

  /**
   * Helper function for appending a column to a Slow SQL table.
   *
   * @param aRowElement Parent row element
   * @param aColType Column's tag name
   * @param aColText Column contents
   */
  appendColumn: function SlowSQL_appendColumn(aRowElement, aColType, aColText) {
    let colElement = document.createElement(aColType);
    let aColTextElement = document.createTextNode(aColText);
    colElement.appendChild(aColTextElement);
    aRowElement.appendChild(colElement);
  }
};

let ChromeHangs = {

  symbolRequest: null,

  stackTitle: bundle.GetStringFromName("stackTitle"),

  memoryMapTitle: bundle.GetStringFromName("memoryMapTitle"),

  errorMessage: bundle.GetStringFromName("errorFetchingSymbols"),

  /**
   * Renders raw chrome hang data
   */
  render: function ChromeHangs_render() {
    let hangsDiv = document.getElementById("chrome-hangs-data");
    this.clearHangData(hangsDiv);
    document.getElementById("fetch-symbols").classList.remove("hidden");
    document.getElementById("hide-symbols").classList.add("hidden");

    let hangs = Telemetry.chromeHangs;
    let stacks = hangs.stacks;
    if (stacks.length == 0) {
      showEmptySectionMessage("chrome-hangs-section");
      return;
    }

    this.renderMemoryMap(hangsDiv);

    let durations = hangs.durations;
    for (let i = 0; i < stacks.length; ++i) {
      let stack = stacks[i];
      this.renderHangHeader(hangsDiv, i + 1, durations[i]);
      this.renderStack(hangsDiv, stack)
    }
  },

  /**
   * Renders the title of the hang: e.g. "Hang Report #1 (6 seconds)"
   *
   * @param aDiv Output div
   * @param aIndex The number of the hang
   * @param aDuration The duration of the hang
   */
  renderHangHeader: function ChromeHangs_renderHangHeader(aDiv, aIndex, aDuration) {
    let titleElement = document.createElement("span");
    titleElement.className = "hang-title";

    let titleText = bundle.formatStringFromName(
      "hangTitle", [aIndex, aDuration], 2);
    titleElement.appendChild(document.createTextNode(titleText));

    aDiv.appendChild(titleElement);
    aDiv.appendChild(document.createElement("br"));
  },

  /**
   * Outputs the raw PCs from the hang's stack
   *
   * @param aDiv Output div
   * @param aStack Array of PCs from the hang stack
   */
  renderStack: function ChromeHangs_renderStack(aDiv, aStack) {
    aDiv.appendChild(document.createTextNode(this.stackTitle));
    let stackText = " " + aStack.join(" ");
    aDiv.appendChild(document.createTextNode(stackText));

    aDiv.appendChild(document.createElement("br"));
    aDiv.appendChild(document.createElement("br"));
  },

  /**
   * Outputs the memory map associated with this hang report
   *
   * @param aDiv Output div
   */
  renderMemoryMap: function ChromeHangs_renderMemoryMap(aDiv) {
    aDiv.appendChild(document.createTextNode(this.memoryMapTitle));
    aDiv.appendChild(document.createElement("br"));

    let hangs = Telemetry.chromeHangs;
    let memoryMap = hangs.memoryMap;
    for (let currentModule of memoryMap) {
      aDiv.appendChild(document.createTextNode(currentModule.join(" ")));
      aDiv.appendChild(document.createElement("br"));
    }

    aDiv.appendChild(document.createElement("br"));
  },

  /**
   * Sends a symbolication request for the recorded hangs
   */
  fetchSymbols: function ChromeHangs_fetchSymbols() {
    let symbolServerURI =
      getPref(PREF_SYMBOL_SERVER_URI, DEFAULT_SYMBOL_SERVER_URI);

    let hangs = Telemetry.chromeHangs;
    let memoryMap = hangs.memoryMap;
    let stacks = hangs.stacks;
    let request = {"memoryMap" : memoryMap, "stacks" : stacks,
                   "version" : 2};
    let requestJSON = JSON.stringify(request);

    this.symbolRequest = XMLHttpRequest();
    this.symbolRequest.open("POST", symbolServerURI, true);
    this.symbolRequest.setRequestHeader("Content-type", "application/json");
    this.symbolRequest.setRequestHeader("Content-length", requestJSON.length);
    this.symbolRequest.setRequestHeader("Connection", "close");

    this.symbolRequest.onreadystatechange = this.handleSymbolResponse.bind(this);
    this.symbolRequest.send(requestJSON);
  },

  /**
   * Called when the 'readyState' of the XMLHttpRequest changes. We only care
   * about state 4 ("completed") - handling the response data.
   */
  handleSymbolResponse: function ChromeHangs_handleSymbolResponse() {
    if (this.symbolRequest.readyState != 4)
      return;

    document.getElementById("fetch-symbols").classList.add("hidden");
    document.getElementById("hide-symbols").classList.remove("hidden");

    let hangsDiv = document.getElementById("chrome-hangs-data");
    this.clearHangData(hangsDiv);

    if (this.symbolRequest.status != 200) {
      hangsDiv.appendChild(document.createTextNode(this.errorMessage));
      return;
    }

    let jsonResponse = {};
    try {
      jsonResponse = JSON.parse(this.symbolRequest.responseText);
    } catch (e) {
      hangsDiv.appendChild(document.createTextNode(this.errorMessage));
      return;
    }

    let hangs = Telemetry.chromeHangs;
    let stacks = hangs.stacks;
    let durations = hangs.durations;
    for (let i = 0; i < jsonResponse.length; ++i) {
      let stack = jsonResponse[i];
      let hangDuration = durations[i];
      this.renderHangHeader(hangsDiv, i + 1, hangDuration);

      for (let symbol of stack) {
        hangsDiv.appendChild(document.createTextNode(symbol));
        hangsDiv.appendChild(document.createElement("br"));
      }
      hangsDiv.appendChild(document.createElement("br"));
    }
  },

  /**
   * Removes child elements from the supplied div
   *
   * @param aDiv Element to be cleared
   */
  clearHangData: function ChromeHangs_clearHangData(aDiv) {
    while (aDiv.hasChildNodes()) {
      aDiv.removeChild(aDiv.lastChild);
    }
  }
};

let Histogram = {

  hgramSamplesCaption: bundle.GetStringFromName("histogramSamples"),

  hgramAverageCaption: bundle.GetStringFromName("histogramAverage"),

  hgramSumCaption: bundle.GetStringFromName("histogramSum"),

  /**
   * Renders a single Telemetry histogram
   *
   * @param aParent Parent element
   * @param aName Histogram name
   * @param aHgram Histogram information
   */
  render: function Histogram_render(aParent, aName, aHgram) {
    let hgram = this.unpack(aHgram);

    let outerDiv = document.createElement("div");
    outerDiv.className = "histogram";
    outerDiv.id = aName;

    let divTitle = document.createElement("div");
    divTitle.className = "histogram-title";
    divTitle.appendChild(document.createTextNode(aName));
    outerDiv.appendChild(divTitle);

    let stats = hgram.sample_count + " " + this.hgramSamplesCaption + ", " +
                this.hgramAverageCaption + " = " + hgram.pretty_average + ", " +
                this.hgramSumCaption + " = " + hgram.sum;

    let divStats = document.createElement("div");
    divStats.appendChild(document.createTextNode(stats));
    outerDiv.appendChild(divStats);

    if (isRTL())
      hgram.values.reverse();

    this.renderValues(outerDiv, hgram.values, hgram.max);

    aParent.appendChild(outerDiv);
  },

  /**
   * Unpacks histogram values
   *
   * @param aHgram Packed histogram
   *
   * @return Unpacked histogram representation
   */
  unpack: function Histogram_unpack(aHgram) {
    let sample_count = aHgram.counts.reduceRight(function (a, b) a + b);
    let buckets = [0, 1];
    if (aHgram.histogram_type != Telemetry.HISTOGRAM_BOOLEAN) {
      buckets = aHgram.ranges;
    }

    let average =  Math.round(aHgram.sum * 10 / sample_count) / 10;
    let max_value = Math.max.apply(Math, aHgram.counts);

    let first = true;
    let last = 0;
    let values = [];
    for (let i = 0; i < buckets.length; i++) {
      let count = aHgram.counts[i];
      if (!count)
        continue;
      if (first) {
        first = false;
        if (i) {
          values.push([buckets[i - 1], 0]);
        }
      }
      last = i + 1;
      values.push([buckets[i], count]);
    }
    if (last && last < buckets.length) {
      values.push([buckets[last], 0]);
    }

    let result = {
      values: values,
      pretty_average: average,
      max: max_value,
      sample_count: sample_count,
      sum: aHgram.sum
    };

    return result;
  },

  /**
   * Create histogram bars
   *
   * @param aDiv Outer parent div
   * @param aValues Histogram values
   * @param aMaxValue Largest histogram value in set
   */
  renderValues: function Histogram_renderValues(aDiv, aValues, aMaxValue) {
    for (let [label, value] of aValues) {
      let belowEm = Math.round(MAX_BAR_HEIGHT * (value / aMaxValue) * 10) / 10;
      let aboveEm = MAX_BAR_HEIGHT - belowEm;

      let barDiv = document.createElement("div");
      barDiv.className = "bar";
      barDiv.style.paddingTop = aboveEm + "em";

      // Add value label or an nbsp if no value
      barDiv.appendChild(document.createTextNode(value ? value : '\u00A0'));

      // Create the blue bar
      let bar = document.createElement("div");
      bar.className = "bar-inner";
      bar.style.height = belowEm + "em";
      barDiv.appendChild(bar);

      // Add bucket label
      barDiv.appendChild(document.createTextNode(label));

      aDiv.appendChild(barDiv);
    }
  }
};

let KeyValueTable = {

  keysHeader: bundle.GetStringFromName("keysHeader"),

  valuesHeader: bundle.GetStringFromName("valuesHeader"),

  /**
   * Fill out a 2-column table with keys and values
   */
  render: function KeyValueTable_render(aTableID, aMeasurements) {
    let table = document.getElementById(aTableID);
    this.renderHeader(table);
    this.renderBody(table, aMeasurements);
  },

  /**
   * Create the table header
   *
   * @param aTable Table element
   */
  renderHeader: function KeyValueTable_renderHeader(aTable) {
    let headerRow = document.createElement("tr");
    aTable.appendChild(headerRow);

    let keysColumn = document.createElement("th");
    keysColumn.appendChild(document.createTextNode(this.keysHeader));
    let valuesColumn = document.createElement("th");
    valuesColumn.appendChild(document.createTextNode(this.valuesHeader));

    headerRow.appendChild(keysColumn);
    headerRow.appendChild(valuesColumn);
  },

  /**
   * Create the table body
   *
   * @param aTable Table element
   * @param aMeasurements Key/value map
   */
  renderBody: function KeyValueTable_renderBody(aTable, aMeasurements) {
    for (let [key, value] of Iterator(aMeasurements)) {
      if (typeof value == "object") {
        value = JSON.stringify(value);
      }

      let newRow = document.createElement("tr");
      aTable.appendChild(newRow);

      let keyField = document.createElement("td");
      keyField.appendChild(document.createTextNode(key));
      newRow.appendChild(keyField);

      let valueField = document.createElement("td");
      valueField.appendChild(document.createTextNode(value));
      newRow.appendChild(valueField);
    }
  }
};

/**
 * Helper function for showing "No data collected" message for a section
 *
 * @param aSectionID ID of the section element that needs to be changed
 */
function showEmptySectionMessage(aSectionID) {
  let sectionElement = document.getElementById(aSectionID);

  // Hide toggle captions
  let toggleElements = sectionElement.getElementsByClassName("toggle-caption");
  toggleElements[0].classList.add("hidden");
  toggleElements[1].classList.add("hidden");

  // Show "No data collected" message
  let messageElement = sectionElement.getElementsByClassName("empty-caption")[0];
  messageElement.classList.remove("hidden");

  // Don't allow section to be expanded by clicking on the header text
  let sectionHeaders = sectionElement.getElementsByClassName("section-name");
  for (let sectionHeader of sectionHeaders) {
    sectionHeader.removeEventListener("click", toggleSection);
    sectionHeader.style.cursor = "auto";
  }

  // Don't allow section to be expanded by clicking on the toggle text
  let toggleLinks = sectionElement.getElementsByClassName("toggle-caption");
  for (let toggleLink of toggleLinks) {
    toggleLink.removeEventListener("click", toggleSection);
  }
}

/**
 * Helper function that expands and collapses sections +
 * changes caption on the toggle text
 */
function toggleSection(aEvent) {
  let parentElement = aEvent.target.parentElement;
  let sectionDiv = parentElement.getElementsByTagName("div")[0];
  sectionDiv.classList.toggle("hidden");

  let toggleLinks = parentElement.getElementsByClassName("toggle-caption");
  toggleLinks[0].classList.toggle("hidden");
  toggleLinks[1].classList.toggle("hidden");
}

/**
 * Sets the text of the page header based on a config pref + bundle strings
 */
function setupPageHeader()
{
  let serverOwner = getPref(PREF_TELEMETRY_SERVER_OWNER, "Mozilla");
  let brandName = brandBundle.GetStringFromName("brandFullName");
  let subtitleText = bundle.formatStringFromName(
    "pageSubtitle", [serverOwner, brandName], 2);

  let subtitleElement = document.getElementById("page-subtitle");
  subtitleElement.appendChild(document.createTextNode(subtitleText));
}

/**
 * Initializes load/unload, pref change and mouse-click listeners
 */
function setupListeners() {
  Services.prefs.addObserver(PREF_TELEMETRY_ENABLED, observer, false);
  observer.updatePrefStatus();

  // Clean up observers when page is closed
  window.addEventListener("unload",
    function unloadHandler(aEvent) {
      window.removeEventListener("unload", unloadHandler);
      Services.prefs.removeObserver(PREF_TELEMETRY_ENABLED, observer);
  }, false);

  document.getElementById("toggle-telemetry").addEventListener("click",
    function () {
      let value = getPref(PREF_TELEMETRY_ENABLED, false);
      Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, !value);
  }, false);

  document.getElementById("fetch-symbols").addEventListener("click",
    function () {
      ChromeHangs.fetchSymbols();
  }, false);

  document.getElementById("hide-symbols").addEventListener("click",
    function () {
      ChromeHangs.render();
  }, false);

  // Clicking on the section name will toggle its state
  let sectionHeaders = document.getElementsByClassName("section-name");
  for (let sectionHeader of sectionHeaders) {
    sectionHeader.addEventListener("click", toggleSection, false);
  }

  // Clicking on the "collapse"/"expand" text will also toggle section's state
  let toggleLinks = document.getElementsByClassName("toggle-caption");
  for (let toggleLink of toggleLinks) {
    toggleLink.addEventListener("click", toggleSection, false);
  }
}

function onLoad() {
  window.removeEventListener("load", onLoad);

  // Set the text in the page header
  setupPageHeader();

  // Set up event listeners
  setupListeners();

#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
  /**
   * When telemetry is opt-out, verify if the user explicitly rejected the
   * telemetry prompt, and if so reflect his choice in the current preference
   * value. This doesn't cover the case where the user refused telemetry in the
   * prompt but later enabled it in preferences in builds before the fix for
   * bug 737600.
   */
  if (getPref(PREF_TELEMETRY_ENABLED, false) &&
      getPref(PREF_TELEMETRY_REJECTED, false)) {
    Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, false);
  }
#endif

  // Show slow SQL stats
  SlowSQL.render();

  // Show chrome hang stacks
  ChromeHangs.render();

  // Show histogram data
  let histograms = Telemetry.histogramSnapshots;
  if (Object.keys(histograms).length) {
    let hgramDiv = document.getElementById("histograms");
    for (let [name, hgram] of Iterator(histograms)) {
      Histogram.render(hgramDiv, name, hgram);
    }
  } else {
    showEmptySectionMessage("histograms-section");
  }

  // Show addon histogram data
  histograms = Telemetry.addonHistogramSnapshots;
  if (Object.keys(histograms).length) {
    let addonDiv = document.getElementById("addon-histograms");
    for (let [name, hgram] of Iterator(histograms)) {
      Histogram.render(addonDiv, "ADDON_" + name, hgram);
    }
  } else {
    showEmptySectionMessage("addon-histograms-section");
  }

  // Get the Telemetry Ping payload
  Telemetry.asyncFetchTelemetryData(displayPingData);
}

function displayPingData() {
  let ping = TelemetryPing.getPayload();

  // Show simple measurements
  if (Object.keys(ping.simpleMeasurements).length) {
    KeyValueTable.render("simple-measurements-table", ping.simpleMeasurements);
  } else {
    showEmptySectionMessage("simple-measurements-section");
  }

  // Show basic system info gathered
  if (Object.keys(ping.info).length) {
    KeyValueTable.render("system-info-table", ping.info);
  } else {
    showEmptySectionMessage("system-info-section");
  }
}

window.addEventListener("load", onLoad, false);
