/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/TelemetryTimestamps.jsm");
Cu.import("resource://gre/modules/TelemetryPing.jsm");
Cu.import("resource://gre/modules/TelemetrySession.jsm");

const Telemetry = Services.telemetry;
const bundle = Services.strings.createBundle(
  "chrome://global/locale/aboutTelemetry.properties");
const brandBundle = Services.strings.createBundle(
  "chrome://branding/locale/brand.properties");

// Maximum height of a histogram bar (in em for html, in chars for text)
const MAX_BAR_HEIGHT = 18;
const MAX_BAR_CHARS = 25;
const PREF_TELEMETRY_SERVER_OWNER = "toolkit.telemetry.server_owner";
const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";
const PREF_DEBUG_SLOW_SQL = "toolkit.telemetry.debugSlowSql";
const PREF_SYMBOL_SERVER_URI = "profiler.symbolicationUrl";
const DEFAULT_SYMBOL_SERVER_URI = "http://symbolapi.mozilla.org";

// ms idle before applying the filter (allow uninterrupted typing)
const FILTER_IDLE_TIMEOUT = 500;

#ifdef XP_WIN
const EOL = "\r\n";
#else
const EOL = "\n";
#endif

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

let GeneralData = {
  /**
   * Renders the general data
   */
  render: function() {
    setHasData("general-data-section", true);

    let table = document.createElement("table");

    let caption = document.createElement("caption");
    let captionString = bundle.GetStringFromName("generalDataTitle");
    caption.appendChild(document.createTextNode(captionString + "\n"));
    table.appendChild(caption);

    let headings = document.createElement("tr");
    this.appendColumn(headings, "th", bundle.GetStringFromName("generalDataHeadingName") + "\t");
    this.appendColumn(headings, "th", bundle.GetStringFromName("generalDataHeadingValue") + "\t");
    table.appendChild(headings);

    let row = document.createElement("tr");
    this.appendColumn(row, "td", "Client ID\t");
    this.appendColumn(row, "td", TelemetryPing.clientID + "\t");
    table.appendChild(row);

    let dataDiv = document.getElementById("general-data");
    dataDiv.appendChild(table);
  },

  /**
   * Helper function for appending a column to the data table.
   *
   * @param aRowElement Parent row element
   * @param aColType Column's tag name
   * @param aColText Column contents
   */
  appendColumn: function(aRowElement, aColType, aColText) {
    let colElement = document.createElement(aColType);
    let colTextElement = document.createTextNode(aColText);
    colElement.appendChild(colTextElement);
    aRowElement.appendChild(colElement);
  },
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
      return;
    }

    setHasData("slow-sql-section", true);

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
   * Tabs & newlines added to cells to make it easier to copy-paste.
   *
   * @param aTable Parent table element
   * @param aTitle Table's title
   */
  renderTableHeader: function SlowSQL_renderTableHeader(aTable, aTitle) {
    let caption = document.createElement("caption");
    caption.appendChild(document.createTextNode(aTitle + "\n"));
    aTable.appendChild(caption);

    let headings = document.createElement("tr");
    this.appendColumn(headings, "th", this.slowSqlHits + "\t");
    this.appendColumn(headings, "th", this.slowSqlAverage + "\t");
    this.appendColumn(headings, "th", this.slowSqlStatement + "\n");
    aTable.appendChild(headings);
  },

  /**
   * Fills out the table body
   * Tabs & newlines added to cells to make it easier to copy-paste.
   *
   * @param aTable Parent table element
   * @param aSql SQL stats object
   */
  renderTable: function SlowSQL_renderTable(aTable, aSql) {
    for (let [sql, [hitCount, totalTime]] of Iterator(aSql)) {
      let averageTime = totalTime / hitCount;

      let sqlRow = document.createElement("tr");

      this.appendColumn(sqlRow, "td", hitCount + "\t");
      this.appendColumn(sqlRow, "td", averageTime.toFixed(0) + "\t");
      this.appendColumn(sqlRow, "td", sql + "\n");

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
    let colTextElement = document.createTextNode(aColText);
    colElement.appendChild(colTextElement);
    aRowElement.appendChild(colElement);
  }
};

/**
 * Removes child elements from the supplied div
 *
 * @param aDiv Element to be cleared
 */
function clearDivData(aDiv) {
  while (aDiv.hasChildNodes()) {
    aDiv.removeChild(aDiv.lastChild);
  }
};

let StackRenderer = {

  stackTitle: bundle.GetStringFromName("stackTitle"),

  memoryMapTitle: bundle.GetStringFromName("memoryMapTitle"),

  /**
   * Outputs the memory map associated with this hang report
   *
   * @param aDiv Output div
   */
  renderMemoryMap: function StackRenderer_renderMemoryMap(aDiv, memoryMap) {
    aDiv.appendChild(document.createTextNode(this.memoryMapTitle));
    aDiv.appendChild(document.createElement("br"));

    for (let currentModule of memoryMap) {
      aDiv.appendChild(document.createTextNode(currentModule.join(" ")));
      aDiv.appendChild(document.createElement("br"));
    }

    aDiv.appendChild(document.createElement("br"));
  },

  /**
   * Outputs the raw PCs from the hang's stack
   *
   * @param aDiv Output div
   * @param aStack Array of PCs from the hang stack
   */
  renderStack: function StackRenderer_renderStack(aDiv, aStack) {
    aDiv.appendChild(document.createTextNode(this.stackTitle));
    let stackText = " " + aStack.join(" ");
    aDiv.appendChild(document.createTextNode(stackText));

    aDiv.appendChild(document.createElement("br"));
    aDiv.appendChild(document.createElement("br"));
  },
  renderStacks: function StackRenderer_renderStacks(aPrefix, aStacks,
                                                    aMemoryMap, aRenderHeader) {
    let div = document.getElementById(aPrefix + '-data');
    clearDivData(div);

    let fetchE = document.getElementById(aPrefix + '-fetch-symbols');
    if (fetchE) {
      fetchE.classList.remove("hidden");
    }
    let hideE = document.getElementById(aPrefix + '-hide-symbols');
    if (hideE) {
      hideE.classList.add("hidden");
    }

    if (aStacks.length == 0) {
      return;
    }

    setHasData(aPrefix + '-section', true);

    this.renderMemoryMap(div, aMemoryMap);

    for (let i = 0; i < aStacks.length; ++i) {
      let stack = aStacks[i];
      aRenderHeader(i);
      this.renderStack(div, stack)
    }
  },

  /**
   * Renders the title of the stack: e.g. "Late Write #1" or
   * "Hang Report #1 (6 seconds)".
   *
   * @param aFormatArgs formating args to be passed to formatStringFromName.
   */
  renderHeader: function StackRenderer_renderHeader(aPrefix, aFormatArgs) {
    let div = document.getElementById(aPrefix + "-data");

    let titleElement = document.createElement("span");
    titleElement.className = "stack-title";

    let titleText = bundle.formatStringFromName(
      aPrefix + "-title", aFormatArgs, aFormatArgs.length);
    titleElement.appendChild(document.createTextNode(titleText));

    div.appendChild(titleElement);
    div.appendChild(document.createElement("br"));
  }
};

function SymbolicationRequest(aPrefix, aRenderHeader, aMemoryMap, aStacks) {
  this.prefix = aPrefix;
  this.renderHeader = aRenderHeader;
  this.memoryMap = aMemoryMap;
  this.stacks = aStacks;
}
/**
 * A callback for onreadystatechange. It replaces the numeric stack with
 * the symbolicated one returned by the symbolication server.
 */
SymbolicationRequest.prototype.handleSymbolResponse =
function SymbolicationRequest_handleSymbolResponse() {
  if (this.symbolRequest.readyState != 4)
    return;

  let fetchElement = document.getElementById(this.prefix + "-fetch-symbols");
  fetchElement.classList.add("hidden");
  let hideElement = document.getElementById(this.prefix + "-hide-symbols");
  hideElement.classList.remove("hidden");
  let div = document.getElementById(this.prefix + "-data");
  clearDivData(div);
  let errorMessage = bundle.GetStringFromName("errorFetchingSymbols");

  if (this.symbolRequest.status != 200) {
    div.appendChild(document.createTextNode(errorMessage));
    return;
  }

  let jsonResponse = {};
  try {
    jsonResponse = JSON.parse(this.symbolRequest.responseText);
  } catch (e) {
    div.appendChild(document.createTextNode(errorMessage));
    return;
  }

  for (let i = 0; i < jsonResponse.length; ++i) {
    let stack = jsonResponse[i];
    this.renderHeader(i);

    for (let symbol of stack) {
      div.appendChild(document.createTextNode(symbol));
      div.appendChild(document.createElement("br"));
    }
    div.appendChild(document.createElement("br"));
  }
};
/**
 * Send a request to the symbolication server to symbolicate this stack.
 */
SymbolicationRequest.prototype.fetchSymbols =
function SymbolicationRequest_fetchSymbols() {
  let symbolServerURI =
    getPref(PREF_SYMBOL_SERVER_URI, DEFAULT_SYMBOL_SERVER_URI);
  let request = {"memoryMap" : this.memoryMap, "stacks" : this.stacks,
                 "version" : 3};
  let requestJSON = JSON.stringify(request);

  this.symbolRequest = new XMLHttpRequest();
  this.symbolRequest.open("POST", symbolServerURI, true);
  this.symbolRequest.setRequestHeader("Content-type", "application/json");
  this.symbolRequest.setRequestHeader("Content-length",
                                      requestJSON.length);
  this.symbolRequest.setRequestHeader("Connection", "close");
  this.symbolRequest.onreadystatechange = this.handleSymbolResponse.bind(this);
  this.symbolRequest.send(requestJSON);
}

let ChromeHangs = {

  symbolRequest: null,

  /**
   * Renders raw chrome hang data
   */
  render: function ChromeHangs_render() {
    let hangs = Telemetry.chromeHangs;
    let stacks = hangs.stacks;
    let memoryMap = hangs.memoryMap;

    StackRenderer.renderStacks("chrome-hangs", stacks, memoryMap,
			       this.renderHangHeader);
  },

  renderHangHeader: function ChromeHangs_renderHangHeader(aIndex) {
    let durations = Telemetry.chromeHangs.durations;
    StackRenderer.renderHeader("chrome-hangs", [aIndex + 1, durations[aIndex]]);
  }
};

let ThreadHangStats = {

  /**
   * Renders raw thread hang stats data
   */
  render: function() {
    let div = document.getElementById("thread-hang-stats");
    clearDivData(div);

    let stats = Telemetry.threadHangStats;
    stats.forEach((thread) => {
      div.appendChild(this.renderThread(thread));
    });
    if (stats.length) {
      setHasData("thread-hang-stats-section", true);
    }
  },

  /**
   * Creates and fills data corresponding to a thread
   */
  renderThread: function(aThread) {
    let div = document.createElement("div");

    let title = document.createElement("h2");
    title.textContent = aThread.name;
    div.appendChild(title);

    // Don't localize the histogram name, because the
    // name is also used as the div element's ID
    Histogram.render(div, aThread.name + "-Activity",
                     aThread.activity, {exponential: true});
    aThread.hangs.forEach((hang, index) => {
      let hangName = aThread.name + "-Hang-" + (index + 1);
      let hangDiv = Histogram.render(
        div, hangName, hang.histogram, {exponential: true});
      let stackDiv = document.createElement("div");
      let stack = hang.nativeStack || hang.stack;
      stack.forEach((frame) => {
        stackDiv.appendChild(document.createTextNode(frame));
        // Leave an extra <br> at the end of the stack listing
        stackDiv.appendChild(document.createElement("br"));
      });
      // Insert stack after the histogram title
      hangDiv.insertBefore(stackDiv, hangDiv.childNodes[1]);
    });
    return div;
  },
};

let Histogram = {

  hgramSamplesCaption: bundle.GetStringFromName("histogramSamples"),

  hgramAverageCaption: bundle.GetStringFromName("histogramAverage"),

  hgramSumCaption: bundle.GetStringFromName("histogramSum"),

  hgramCopyCaption: bundle.GetStringFromName("histogramCopy"),

  /**
   * Renders a single Telemetry histogram
   *
   * @param aParent Parent element
   * @param aName Histogram name
   * @param aHgram Histogram information
   * @param aOptions Object with render options
   *                 * exponential: bars follow logarithmic scale
   */
  render: function Histogram_render(aParent, aName, aHgram, aOptions) {
    let hgram = this.unpack(aHgram);
    let options = aOptions || {};

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

    let textData = this.renderValues(outerDiv, hgram.values, hgram.max,
                                     hgram.sample_count, options);

    // The 'Copy' button contains the textual data, copied to clipboard on click
    let copyButton = document.createElement("button");
    copyButton.className = "copy-node";
    copyButton.appendChild(document.createTextNode(this.hgramCopyCaption));
    copyButton.histogramText = aName + EOL + stats + EOL + EOL + textData;
    copyButton.addEventListener("click", function(){
      Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper)
                                                 .copyString(this.histogramText);
    });
    outerDiv.appendChild(copyButton);

    aParent.appendChild(outerDiv);
    return outerDiv;
  },

  /**
   * Unpacks histogram values
   *
   * @param aHgram Packed histogram
   *
   * @return Unpacked histogram representation
   */
  unpack: function Histogram_unpack(aHgram) {
    let sample_count = aHgram.counts.reduceRight((a, b) => a + b);
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
   * Return a non-negative, logarithmic representation of a non-negative number.
   * e.g. 0 => 0, 1 => 1, 10 => 2, 100 => 3
   *
   * @param aNumber Non-negative number
   */
  getLogValue: function(aNumber) {
    return Math.max(0, Math.log10(aNumber) + 1);
  },

  /**
   * Create histogram HTML bars, also returns a textual representation
   * Both aMaxValue and aSumValues must be positive.
   * Values are assumed to use 0 as baseline.
   *
   * @param aDiv Outer parent div
   * @param aValues Histogram values
   * @param aMaxValue Value of the longest bar (length, not label)
   * @param aSumValues Sum of all bar values
   * @param aOptions Object with render options (@see #render)
   */
  renderValues: function Histogram_renderValues(aDiv, aValues, aMaxValue, aSumValues, aOptions) {
    let text = "";
    // If the last label is not the longest string, alignment will break a little
    let labelPadTo = String(aValues[aValues.length -1][0]).length;
    let maxBarValue = aOptions.exponential ? this.getLogValue(aMaxValue) : aMaxValue;

    for (let [label, value] of aValues) {
      let barValue = aOptions.exponential ? this.getLogValue(value) : value;

      // Create a text representation: <right-aligned-label> |<bar-of-#><value>  <percentage>
      text += EOL
              + " ".repeat(Math.max(0, labelPadTo - String(label).length)) + label // Right-aligned label
              + " |" + "#".repeat(Math.round(MAX_BAR_CHARS * barValue / maxBarValue)) // Bar
              + "  " + value // Value
              + "  " + Math.round(100 * value / aSumValues) + "%"; // Percentage

      // Construct the HTML labels + bars
      let belowEm = Math.round(MAX_BAR_HEIGHT * (barValue / maxBarValue) * 10) / 10;
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

    return text.substr(EOL.length); // Trim the EOL before the first line
  },

  /**
   * Helper function for filtering histogram elements by their id
   * Adds the "filter-blocked" class to histogram nodes whose IDs don't match the filter.
   *
   * @param aContainerNode Container node containing the histogram class nodes to filter
   * @param aFilterText either text or /RegEx/. If text, case-insensitive and AND words
   */
  filterHistograms: function _filterHistograms(aContainerNode, aFilterText) {
    let filter = aFilterText.toString();

    // Pass if: all non-empty array items match (case-sensitive)
    function isPassText(subject, filter) {
      for (let item of filter) {
        if (item.length && subject.indexOf(item) < 0) {
          return false; // mismatch and not a spurious space
        }
      }
      return true;
    }

    function isPassRegex(subject, filter) {
      return filter.test(subject);
    }

    // Setup normalized filter string (trimmed, lower cased and split on spaces if not RegEx)
    let isPassFunc; // filter function, set once, then applied to all elements
    filter = filter.trim();
    if (filter[0] != "/") { // Plain text: case insensitive, AND if multi-string
      isPassFunc = isPassText;
      filter = filter.toLowerCase().split(" ");
    } else {
      isPassFunc = isPassRegex;
      var r = filter.match(/^\/(.*)\/(i?)$/);
      try {
        filter = RegExp(r[1], r[2]);
      }
      catch (e) { // Incomplete or bad RegExp - always no match
        isPassFunc = function() {
          return false;
        };
      }
    }

    let needLower = (isPassFunc === isPassText);

    let histograms = aContainerNode.getElementsByClassName("histogram");
    for (let hist of histograms) {
      hist.classList[isPassFunc((needLower ? hist.id.toLowerCase() : hist.id), filter) ? "remove" : "add"]("filter-blocked");
    }
  },

  /**
   * Event handler for change at histograms filter input
   *
   * When invoked, 'this' is expected to be the filter HTML node.
   */
  histogramFilterChanged: function _histogramFilterChanged() {
    if (this.idleTimeout) {
      clearTimeout(this.idleTimeout);
    }

    this.idleTimeout = setTimeout( () => {
      Histogram.filterHistograms(document.getElementById(this.getAttribute("target_id")), this.value);
    }, FILTER_IDLE_TIMEOUT);
  }
};

/*
 * Helper function to render JS objects with white space between top level elements
 * so that they look better in the browser
 * @param   aObject JavaScript object or array to render
 * @return  String
 */
function RenderObject(aObject) {
  let output = "";
  if (Array.isArray(aObject)) {
    if (aObject.length == 0) {
      return "[]";
    }
    output = "[" + JSON.stringify(aObject[0]);
    for (let i = 1; i < aObject.length; i++) {
      output += ", " + JSON.stringify(aObject[i]);
    }
    return output + "]";
  }
  let keys = Object.keys(aObject);
  if (keys.length == 0) {
    return "{}";
  }
  output = "{\"" + keys[0] + "\":\u00A0" + JSON.stringify(aObject[keys[0]]);
  for (let i = 1; i < keys.length; i++) {
    output += ", \"" + keys[i] + "\":\u00A0" + JSON.stringify(aObject[keys[i]]);
  }
  return output + "}";
};

let KeyValueTable = {
  /**
   * Returns a 2-column table with keys and values
   * @param aMeasurements Each key in this JS object is rendered as a row in
   *                      the table with its corresponding value
   * @param aKeysLabel    Column header for the keys column
   * @param aValuesLabel  Column header for the values column
   */
  render: function KeyValueTable_render(aMeasurements, aKeysLabel, aValuesLabel) {
    let table = document.createElement("table");
    this.renderHeader(table, aKeysLabel, aValuesLabel);
    this.renderBody(table, aMeasurements);
    return table;
  },

  /**
   * Create the table header
   * Tabs & newlines added to cells to make it easier to copy-paste.
   *
   * @param aTable Table element
   * @param aKeysLabel    Column header for the keys column
   * @param aValuesLabel  Column header for the values column
   */
  renderHeader: function KeyValueTable_renderHeader(aTable, aKeysLabel, aValuesLabel) {
    let headerRow = document.createElement("tr");
    aTable.appendChild(headerRow);

    let keysColumn = document.createElement("th");
    keysColumn.appendChild(document.createTextNode(aKeysLabel + "\t"));
    let valuesColumn = document.createElement("th");
    valuesColumn.appendChild(document.createTextNode(aValuesLabel + "\n"));

    headerRow.appendChild(keysColumn);
    headerRow.appendChild(valuesColumn);
  },

  /**
   * Create the table body
   * Tabs & newlines added to cells to make it easier to copy-paste.
   *
   * @param aTable Table element
   * @param aMeasurements Key/value map
   */
  renderBody: function KeyValueTable_renderBody(aTable, aMeasurements) {
    for (let [key, value] of Iterator(aMeasurements)) {
      // use .valueOf() to unbox Number, String, etc. objects
      if ((typeof value == "object") && (typeof value.valueOf() == "object")) {
        value = RenderObject(value);
      }

      let newRow = document.createElement("tr");
      aTable.appendChild(newRow);

      let keyField = document.createElement("td");
      keyField.appendChild(document.createTextNode(key + "\t"));
      newRow.appendChild(keyField);

      let valueField = document.createElement("td");
      valueField.appendChild(document.createTextNode(value + "\n"));
      newRow.appendChild(valueField);
    }
  }
};

let KeyedHistogram = {
  render: function(parent, id, keyedHistogram) {
    let outerDiv = document.createElement("div");
    outerDiv.className = "keyed-histogram";
    outerDiv.id = id;

    let divTitle = document.createElement("div");
    divTitle.className = "keyed-histogram-title";
    divTitle.appendChild(document.createTextNode(id));
    outerDiv.appendChild(divTitle);

    for (let [name, hgram] of Iterator(keyedHistogram)) {
      Histogram.render(outerDiv, name, hgram);
    }

    parent.appendChild(outerDiv);
    return outerDiv;
  },
};

let AddonDetails = {
  tableIDTitle: bundle.GetStringFromName("addonTableID"),
  tableDetailsTitle: bundle.GetStringFromName("addonTableDetails"),

  /**
   * Render the addon details section as a series of headers followed by key/value tables
   * @param aSections Object containing the details sections to render
   */
  render: function AddonDetails_render(aSections) {
    let addonSection = document.getElementById("addon-details");
    for (let provider in aSections) {
      let providerSection = document.createElement("h2");
      let titleText = bundle.formatStringFromName("addonProvider", [provider], 1);
      providerSection.appendChild(document.createTextNode(titleText));
      addonSection.appendChild(providerSection);
      addonSection.appendChild(
        KeyValueTable.render(aSections[provider],
                             this.tableIDTitle, this.tableDetailsTitle));
    }
  }
};

/**
 * Helper function for showing either the toggle element or "No data collected" message for a section
 *
 * @param aSectionID ID of the section element that needs to be changed
 * @param aHasData true (default) indicates that toggle should be displayed
 */
function setHasData(aSectionID, aHasData) {
  let sectionElement = document.getElementById(aSectionID);
  sectionElement.classList[aHasData ? "add" : "remove"]("has-data");
}

/**
 * Helper function that expands and collapses sections +
 * changes caption on the toggle text
 */
function toggleSection(aEvent) {
  let parentElement = aEvent.target.parentElement;
  if (!parentElement.classList.contains("has-data")) {
    return; // nothing to toggle
  }

  parentElement.classList.toggle("expanded");

  // Store section opened/closed state in a hidden checkbox (which is then used on reload)
  let statebox = parentElement.getElementsByClassName("statebox")[0];
  statebox.checked = parentElement.classList.contains("expanded");
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

  document.getElementById("chrome-hangs-fetch-symbols").addEventListener("click",
    function () {
      let hangs = Telemetry.chromeHangs;
      let req = new SymbolicationRequest("chrome-hangs",
                                         ChromeHangs.renderHangHeader,
                                         hangs.memoryMap, hangs.stacks);
      req.fetchSymbols();
  }, false);

  document.getElementById("chrome-hangs-hide-symbols").addEventListener("click",
    function () {
      ChromeHangs.render();
  }, false);

  document.getElementById("late-writes-fetch-symbols").addEventListener("click",
    function () {
      let lateWrites = TelemetrySession.getPayload().lateWrites;
      let req = new SymbolicationRequest("late-writes",
                                         LateWritesSingleton.renderHeader,
                                         lateWrites.memoryMap,
                                         lateWrites.stacks);
      req.fetchSymbols();
  }, false);

  document.getElementById("late-writes-hide-symbols").addEventListener("click",
    function () {
      let ping = TelemetrySession.getPayload();
      LateWritesSingleton.renderLateWrites(ping.lateWrites);
  }, false);

  // Clicking on the section name will toggle its state
  let sectionHeaders = document.getElementsByClassName("section-name");
  for (let sectionHeader of sectionHeaders) {
    sectionHeader.addEventListener("click", toggleSection, false);
  }

  // Clicking on the "toggle" text will also toggle section's state
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

  // Show general data.
  GeneralData.render();

  // Show slow SQL stats
  SlowSQL.render();

  // Show chrome hang stacks
  ChromeHangs.render();

  // Show thread hang stats
  ThreadHangStats.render();

  // Show histogram data
  let histograms = Telemetry.histogramSnapshots;
  if (Object.keys(histograms).length) {
    let hgramDiv = document.getElementById("histograms");
    for (let [name, hgram] of Iterator(histograms)) {
      Histogram.render(hgramDiv, name, hgram);
    }

    let filterBox = document.getElementById("histograms-filter");
    filterBox.addEventListener("input", Histogram.histogramFilterChanged, false);
    if (filterBox.value.trim() != "") { // on load, no need to filter if empty
      Histogram.filterHistograms(hgramDiv, filterBox.value);
    }

    setHasData("histograms-section", true);
  }

  // Show keyed histogram data
  let keyedHistograms = Telemetry.keyedHistogramSnapshots;
  if (Object.keys(keyedHistograms).length) {
    let keyedDiv = document.getElementById("keyed-histograms");
    for (let [id, keyed] of Iterator(keyedHistograms)) {
      KeyedHistogram.render(keyedDiv, id, keyed);
    }

    setHasData("keyed-histograms-section", true);
  }

  // Show addon histogram data
  let addonDiv = document.getElementById("addon-histograms");
  let addonHistogramsRendered = false;
  let addonData = Telemetry.addonHistogramSnapshots;
  for (let [addon, histograms] of Iterator(addonData)) {
    for (let [name, hgram] of Iterator(histograms)) {
      addonHistogramsRendered = true;
      Histogram.render(addonDiv, addon + ": " + name, hgram);
    }
  }

  if (addonHistogramsRendered) {
   setHasData("addon-histograms-section", true);
  }

  // Get the Telemetry Ping payload
  Telemetry.asyncFetchTelemetryData(displayPingData);

  // Restore sections states
  let stateboxes = document.getElementsByClassName("statebox");
  for (let box of stateboxes) {
    if (box.checked) { // Was open. Will still display as empty if not has-data
        box.parentElement.classList.add("expanded");
    }
  }
}

let LateWritesSingleton = {
  renderHeader: function LateWritesSingleton_renderHeader(aIndex) {
    StackRenderer.renderHeader("late-writes", [aIndex + 1]);
  },

  renderLateWrites: function LateWritesSingleton_renderLateWrites(lateWrites) {
    let stacks = lateWrites.stacks;
    let memoryMap = lateWrites.memoryMap;
    StackRenderer.renderStacks('late-writes', stacks, memoryMap,
                               LateWritesSingleton.renderHeader);
  }
};

/**
 * Helper function for sorting the startup milestones in the Simple Measurements
 * section into temporal order.
 *
 * @param aSimpleMeasurements Telemetry ping's "Simple Measurements" data
 * @return Sorted measurements
 */
function sortStartupMilestones(aSimpleMeasurements) {
  const telemetryTimestamps = TelemetryTimestamps.get();
  let startupEvents = Services.startup.getStartupInfo();
  delete startupEvents['process'];

  function keyIsMilestone(k) {
    return (k in startupEvents) || (k in telemetryTimestamps);
  }

  let sortedKeys = Object.keys(aSimpleMeasurements);

  // Sort the measurements, with startup milestones at the front + ordered by time
  sortedKeys.sort(function keyCompare(keyA, keyB) {
    let isKeyAMilestone = keyIsMilestone(keyA);
    let isKeyBMilestone = keyIsMilestone(keyB);

    // First order by startup vs non-startup measurement
    if (isKeyAMilestone && !isKeyBMilestone)
      return -1;
    if (!isKeyAMilestone && isKeyBMilestone)
      return 1;
    // Don't change order of non-startup measurements
    if (!isKeyAMilestone && !isKeyBMilestone)
      return 0;

    // If both keys are startup measurements, order them by value
    return aSimpleMeasurements[keyA] - aSimpleMeasurements[keyB];
  });

  // Insert measurements into a result object in sort-order
  let result = {};
  for (let key of sortedKeys) {
    result[key] = aSimpleMeasurements[key];
  }

  return result;
}

function displayPingData() {
  let ping = TelemetrySession.getPayload();

  let keysHeader = bundle.GetStringFromName("keysHeader");
  let valuesHeader = bundle.GetStringFromName("valuesHeader");

  // Show simple measurements
  let simpleMeasurements = sortStartupMilestones(ping.simpleMeasurements);
  if (Object.keys(simpleMeasurements).length) {
    let simpleSection = document.getElementById("simple-measurements");
    simpleSection.appendChild(KeyValueTable.render(simpleMeasurements,
                                                   keysHeader, valuesHeader));
    setHasData("simple-measurements-section", true);
  }

  LateWritesSingleton.renderLateWrites(ping.lateWrites);

  // Show basic system info gathered
  if (Object.keys(ping.info).length) {
    let infoSection = document.getElementById("system-info");
    infoSection.appendChild(KeyValueTable.render(ping.info,
                                                 keysHeader, valuesHeader));
    setHasData("system-info-section", true);
  }

  let addonDetails = ping.addonDetails;
  if (Object.keys(addonDetails).length) {
    AddonDetails.render(addonDetails);
    setHasData("addon-details-section", true);
  }
}

window.addEventListener("load", onLoad, false);
