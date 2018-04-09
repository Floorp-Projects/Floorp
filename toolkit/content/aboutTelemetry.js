/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/TelemetryTimestamps.jsm");
ChromeUtils.import("resource://gre/modules/TelemetryController.jsm");
ChromeUtils.import("resource://gre/modules/TelemetryArchive.jsm");
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "Preferences",
                               "resource://gre/modules/Preferences.jsm");

const Telemetry = Services.telemetry;
const bundle = Services.strings.createBundle(
  "chrome://global/locale/aboutTelemetry.properties");
const brandBundle = Services.strings.createBundle(
  "chrome://branding/locale/brand.properties");

// Maximum height of a histogram bar (in em for html, in chars for text)
const MAX_BAR_HEIGHT = 8;
const MAX_BAR_CHARS = 25;
const PREF_TELEMETRY_SERVER_OWNER = "toolkit.telemetry.server_owner";
const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";
const PREF_DEBUG_SLOW_SQL = "toolkit.telemetry.debugSlowSql";
const PREF_SYMBOL_SERVER_URI = "profiler.symbolicationUrl";
const DEFAULT_SYMBOL_SERVER_URI = "https://symbols.mozilla.org/symbolicate/v4";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

// ms idle before applying the filter (allow uninterrupted typing)
const FILTER_IDLE_TIMEOUT = 500;

const isWindows = (Services.appinfo.OS == "WINNT");
const EOL = isWindows ? "\r\n" : "\n";

// This is the ping object currently displayed in the page.
var gPingData = null;

// Cached value of document's RTL mode
var documentRTLMode = "";

/**
 * Helper function for determining whether the document direction is RTL.
 * Caches result of check on first invocation.
 */
function isRTL() {
  if (!documentRTLMode)
    documentRTLMode = window.getComputedStyle(document.body).direction;
  return (documentRTLMode == "rtl");
}

function isFlatArray(obj) {
  if (!Array.isArray(obj)) {
    return false;
  }
  return !obj.some(e => typeof(e) == "object");
}

/**
 * This is a helper function for explodeObject.
 */
function flattenObject(obj, map, path, array) {
  for (let k of Object.keys(obj)) {
    let newPath = [...path, array ? "[" + k + "]" : k];
    let v = obj[k];
    if (!v || (typeof(v) != "object")) {
      map.set(newPath.join("."), v);
    } else if (isFlatArray(v)) {
      map.set(newPath.join("."), "[" + v.join(", ") + "]");
    } else {
      flattenObject(v, map, newPath, Array.isArray(v));
    }
  }
}

/**
 * This turns a JSON object into a "flat" stringified form.
 *
 * For an object like {a: "1", b: {c: "2", d: "3"}} it returns a Map of the
 * form Map(["a","1"], ["b.c", "2"], ["b.d", "3"]).
 */
function explodeObject(obj) {
  let map = new Map();
  flattenObject(obj, map, []);
  return map;
}

function filterObject(obj, filterOut) {
  let ret = {};
  for (let k of Object.keys(obj)) {
    if (!filterOut.includes(k)) {
      ret[k] = obj[k];
    }
  }
  return ret;
}

/**
 * This turns a JSON object into a "flat" stringified form, separated into top-level sections.
 *
 * For an object like:
 *   {
 *     a: {b: "1"},
 *     c: {d: "2", e: {f: "3"}}
 *   }
 * it returns a Map of the form:
 *   Map([
 *     ["a", Map(["b","1"])],
 *     ["c", Map([["d", "2"], ["e.f", "3"]])]
 *   ])
 */
function sectionalizeObject(obj) {
  let map = new Map();
  for (let k of Object.keys(obj)) {
    map.set(k, explodeObject(obj[k]));
  }
  return map;
}

/**
 * Obtain the main DOMWindow for the current context.
 */
function getMainWindow() {
  return window.QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIWebNavigation)
               .QueryInterface(Ci.nsIDocShellTreeItem)
               .rootTreeItem
               .QueryInterface(Ci.nsIInterfaceRequestor)
               .getInterface(Ci.nsIDOMWindow);
}

/**
 * Obtain the DOMWindow that can open a preferences pane.
 *
 * This is essentially "get the browser chrome window" with the added check
 * that the supposed browser chrome window is capable of opening a preferences
 * pane.
 *
 * This may return null if we can't find the browser chrome window.
 */
function getMainWindowWithPreferencesPane() {
  let mainWindow = getMainWindow();
  if (mainWindow && "openPreferences" in mainWindow) {
    return mainWindow;
  }
  return null;
}

/**
 * Remove all child nodes of a document node.
 */
function removeAllChildNodes(node) {
  while (node.hasChildNodes()) {
    node.removeChild(node.lastChild);
  }
}

var Settings = {
  SETTINGS: [
    // data upload
    {
      pref: PREF_FHR_UPLOAD_ENABLED,
      defaultPrefValue: false,
    },
    // extended "Telemetry" recording
    {
      pref: PREF_TELEMETRY_ENABLED,
      defaultPrefValue: false,
    },
  ],

  attachObservers() {
    for (let s of this.SETTINGS) {
      let setting = s;
      Preferences.observe(setting.pref, this.render, this);
    }

    let elements = document.getElementsByClassName("change-data-choices-link");
    for (let el of elements) {
      el.addEventListener("click", function() {
        if (AppConstants.platform == "android") {
          ChromeUtils.import("resource://gre/modules/Messaging.jsm");
          EventDispatcher.instance.sendRequest({
            type: "Settings:Show",
            resource: "preferences_privacy",
          });
        } else {
          // Show the data choices preferences on desktop.
          let mainWindow = getMainWindowWithPreferencesPane();
          mainWindow.openPreferences("privacy-reports", { origin: "aboutTelemetry" });
        }
      });
    }
  },

  detachObservers() {
    for (let setting of this.SETTINGS) {
      Preferences.ignore(setting.pref, this.render, this);
    }
  },

  getStatusStringForSetting(setting) {
    let enabled = Preferences.get(setting.pref, setting.defaultPrefValue);
    let status = bundle.GetStringFromName(enabled ? "telemetryUploadEnabled" : "telemetryUploadDisabled");
    return status;
  },

  /**
   * Updates the button & text at the top of the page to reflect Telemetry state.
   */
  render() {
    let settingsExplanation = document.getElementById("settings-explanation");
    let uploadEnabled = this.getStatusStringForSetting(this.SETTINGS[0]);
    let extendedEnabled = Services.telemetry.canRecordExtended;
    let collectedData = bundle.GetStringFromName(extendedEnabled ? "prereleaseData" : "releaseData");

    let parameters = [
      collectedData,
      this.convertStringToLink(uploadEnabled),
    ];
    let explanation = bundle.formatStringFromName("settingsExplanation", parameters, 2);

    // eslint-disable-next-line no-unsanitized/property
    settingsExplanation.innerHTML = explanation;
    this.attachObservers();
  },

  convertStringToLink(string) {
    return "<a href=\"#\" class=\"change-data-choices-link\">" + string + "</a>";
  },
};

var PingPicker = {
  viewCurrentPingData: null,
  _archivedPings: null,
  TYPE_ALL: bundle.GetStringFromName("telemetryPingTypeAll"),

  attachObservers() {
    let pingSourceElements = document.getElementsByName("choose-ping-source");
    for (let el of pingSourceElements) {
      el.addEventListener("change", () => this.onPingSourceChanged());
    }

    let displays = document.getElementsByName("choose-ping-display");
    for (let el of displays) {
      el.addEventListener("change", () => this.onPingDisplayChanged());
    }

    document.getElementById("show-subsession-data").addEventListener("change", () => {
      this._updateCurrentPingData();
    });

    document.getElementById("choose-ping-id").addEventListener("change", () => {
      this._updateArchivedPingData();
    });
    document.getElementById("choose-ping-type").addEventListener("change", () => {
      this.filterDisplayedPings();
    });


    document.getElementById("newer-ping")
            .addEventListener("click", () => this._movePingIndex(-1));
    document.getElementById("older-ping")
            .addEventListener("click", () => this._movePingIndex(1));

    let pingPickerNeedHide = false;
    let pingPicker = document.getElementById("ping-picker");
    pingPicker.addEventListener("mouseenter", () => pingPickerNeedHide = false);
    pingPicker.addEventListener("mouseleave", () => pingPickerNeedHide = true);
    document.addEventListener("click", (ev) => {
      if (pingPickerNeedHide) {
        pingPicker.classList.add("hidden");
      }
    });
    document.getElementById("processes")
            .addEventListener("change", () => displayPingData(gPingData));
    Array.from(document.querySelectorAll(".change-ping")).forEach(el => {
      el.addEventListener("click", (event) => {
        if (!pingPicker.classList.contains("hidden")) {
          pingPicker.classList.add("hidden");
        } else {
          pingPicker.classList.remove("hidden");
          event.stopPropagation();
        }
      });
    });
  },

  onPingSourceChanged() {
    this.update();
  },

  onPingDisplayChanged() {
    this.update();
  },

  render() {
    let pings = bundle.GetStringFromName("pingExplanationLink");
    let pingLink = "<a href=\"https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/concepts/pings.html\">" + pings + "</a>";
    let pingName = this._getSelectedPingName();

    // Display the type and controls if the ping is not current
    let pingDate = document.getElementById("ping-date");
    let pingType = document.getElementById("ping-type");
    let controls = document.getElementById("controls");
    let explanation;
    if (!this.viewCurrentPingData) {
      // Change sidebar heading text.
      pingDate.textContent = pingName;
      pingDate.setAttribute("title", pingName);
      let pingTypeText = this._getSelectedPingType();
      controls.classList.remove("hidden");
      pingType.textContent = pingTypeText;

      // Change home page text.
      pingName = bundle.formatStringFromName("namedPing", [pingName, pingTypeText], 2);
      let pingNameHtml = "<span class=\"change-ping\">" + pingName + "</span>";
      let parameters = [pingLink, pingNameHtml, pingTypeText];
      explanation = bundle.formatStringFromName("pingDetails", parameters, 3);
    } else {
      // Change sidebar heading text.
      controls.classList.add("hidden");
      pingType.textContent = bundle.GetStringFromName("currentPingSidebar");

      // Change home page text.
      let pingNameHtml = "<span class=\"change-ping\">" + pingName + "</span>";
      explanation = bundle.formatStringFromName("pingDetailsCurrent", [pingLink, pingNameHtml], 2);
    }

    let pingExplanation = document.getElementById("ping-explanation");

    // eslint-disable-next-line no-unsanitized/property
    pingExplanation.innerHTML = explanation;
    pingExplanation.querySelector(".change-ping").addEventListener("click", (ev) => {
      document.getElementById("ping-picker").classList.remove("hidden");
      ev.stopPropagation();
    });

    GenericSubsection.deleteAllSubSections();
  },

  async update() {
    let viewCurrent = document.getElementById("ping-source-current").checked;
    let currentChanged = viewCurrent !== this.viewCurrentPingData;
    this.viewCurrentPingData = viewCurrent;

    // If we have no archived pings, disable the ping archive selection.
    // This can happen on new profiles or if the ping archive is disabled.
    let archivedPingList = await TelemetryArchive.promiseArchivedPingList();
    let sourceArchived = document.getElementById("ping-source-archive");
    let sourceArchivedContainer = document.getElementById("ping-source-archive-container");
    let archivedDisabled = (archivedPingList.length == 0);
    sourceArchived.disabled = archivedDisabled;
    sourceArchivedContainer.classList.toggle("disabled", archivedDisabled);

    if (currentChanged) {
      if (this.viewCurrentPingData) {
        document.getElementById("current-ping-picker").hidden = false;
        document.getElementById("archived-ping-picker").hidden = true;
        this._updateCurrentPingData();
      } else {
        document.getElementById("current-ping-picker").hidden = true;
        await this._updateArchivedPingList(archivedPingList);
        document.getElementById("archived-ping-picker").hidden = false;
      }
    }
  },

  _updateCurrentPingData() {
    const subsession = document.getElementById("show-subsession-data").checked;
    const ping = TelemetryController.getCurrentPingData(subsession);
    if (!ping) {
      return;
    }
    displayPingData(ping, true);
  },

  _updateArchivedPingData() {
    let id = this._getSelectedPingId();
    let res = Promise.resolve();
    if (id) {
      res = TelemetryArchive.promiseArchivedPingById(id)
                            .then((ping) => displayPingData(ping, true));
    }
    return res;
  },

  async _updateArchivedPingList(pingList) {
    // The archived ping list is sorted in ascending timestamp order,
    // but descending is more practical for the operations we do here.
    pingList.reverse();
    this._archivedPings = pingList;
    // Render the archive data.
    this._renderPingList();
    // Update the displayed ping.
    await this._updateArchivedPingData();
  },

  _renderPingList() {
    let pingSelector = document.getElementById("choose-ping-id");
    Array.from(pingSelector.children).forEach((child) => removeAllChildNodes(child));

    let pingTypes = new Set();
    pingTypes.add(this.TYPE_ALL);

    const today = new Date();
    today.setHours(0, 0, 0, 0);
    const yesterday = new Date(today);
    yesterday.setDate(today.getDate() - 1);

    for (let p of this._archivedPings) {
      pingTypes.add(p.type);
      const pingDate = new Date(p.timestampCreated);
      const datetimeText = new Services.intl.DateTimeFormat(undefined, {
          dateStyle: "short",
          timeStyle: "medium"
        }).format(pingDate);
      const pingName = `${datetimeText}, ${p.type}`;

      let option = document.createElement("option");
      let content = document.createTextNode(pingName);
      option.appendChild(content);
      option.setAttribute("value", p.id);
      option.dataset.type = p.type;
      option.dataset.date = datetimeText;

      pingDate.setHours(0, 0, 0, 0);
      if (pingDate.getTime() === today.getTime()) {
        pingSelector.children[0].appendChild(option);
      } else if (pingDate.getTime() === yesterday.getTime()) {
        pingSelector.children[1].appendChild(option);
      } else {
        pingSelector.children[2].appendChild(option);
      }
    }
    this._renderPingTypes(pingTypes);
  },

  _renderPingTypes(pingTypes) {
    let pingTypeSelector = document.getElementById("choose-ping-type");
    removeAllChildNodes(pingTypeSelector);
    pingTypes.forEach((type) => {
      let option = document.createElement("option");
      option.appendChild(document.createTextNode(type));
      option.setAttribute("value", type);
      pingTypeSelector.appendChild(option);
    });
  },

  _movePingIndex(offset) {
    if (this.viewCurrentPingData) {
      return;
    }
    let typeSelector = document.getElementById("choose-ping-type");
    let type = typeSelector.selectedOptions.item(0).value;

    let id = this._getSelectedPingId();
    let index = this._archivedPings.findIndex((p) => p.id == id);
    let newIndex = Math.min(Math.max(0, index + offset), this._archivedPings.length - 1);

    let pingList;
    if (offset > 0) {
      pingList = this._archivedPings.slice(newIndex);
    } else {
      pingList = this._archivedPings.slice(0, newIndex);
      pingList.reverse();
    }

    let ping = pingList.find((p) => {
      return type == this.TYPE_ALL || p.type == type;
    });

    if (ping) {
      this.selectPing(ping);
      this._updateArchivedPingData();
    }
  },

  selectPing(ping) {
    let pingSelector = document.getElementById("choose-ping-id");
    // Use some() to break if we find the ping.
    Array.from(pingSelector.children).some((group) => {
      return Array.from(group.children).some((option) => {
        if (option.value == ping.id) {
          option.selected = true;
          return true;
        }
        return false;
      });
    });
  },

  filterDisplayedPings() {
    let pingSelector = document.getElementById("choose-ping-id");
    let typeSelector = document.getElementById("choose-ping-type");
    let type = typeSelector.selectedOptions.item(0).value;
    let first = true;
    Array.from(pingSelector.children).forEach((group) => {
      Array.from(group.children).forEach((option) => {
        if (first && option.dataset.type == type) {
          option.selected = true;
          first = false;
        }
        option.hidden = (type != this.TYPE_ALL) && (option.dataset.type != type);
        // Arrow keys should only iterate over visible options
        option.disabled = option.hidden;
      });
    });
    this._updateArchivedPingData();
  },

  _getSelectedPingName() {
    if (this.viewCurrentPingData) return bundle.GetStringFromName("currentPing");

    let pingSelector = document.getElementById("choose-ping-id");
    let selected = pingSelector.selectedOptions.item(0);
    return selected.dataset.date;
  },

  _getSelectedPingType() {
    let pingSelector = document.getElementById("choose-ping-id");
    let selected = pingSelector.selectedOptions.item(0);
    return selected.dataset.type;
  },

  _getSelectedPingId() {
    let pingSelector = document.getElementById("choose-ping-id");
    let selected = pingSelector.selectedOptions.item(0);
    return selected.getAttribute("value");
  },

  _showRawPingData() {
    show(document.getElementById("category-raw"));
  },

  _showStructuredPingData() {
    show(document.getElementById("category-home"));
  },
};

var GeneralData = {
  /**
   * Renders the general data
   */
  render(aPing) {
    setHasData("general-data-section", true);
    let generalDataSection = document.getElementById("general-data");
    removeAllChildNodes(generalDataSection);

    const headings = [
      "namesHeader",
      "valuesHeader",
    ].map(h => bundle.GetStringFromName(h));

    // The payload & environment parts are handled by other renderers.
    let ignoreSections = ["payload", "environment"];
    let data = explodeObject(filterObject(aPing, ignoreSections));

    const table = GenericTable.render(data, headings);
    generalDataSection.appendChild(table);
  },
};

var EnvironmentData = {
  /**
   * Renders the environment data
   */
  render(ping) {
    let dataDiv = document.getElementById("environment-data");
    removeAllChildNodes(dataDiv);
    const hasData = !!ping.environment;
    setHasData("environment-data-section", hasData);
    if (!hasData) {
      return;
    }

    let ignore = ["addons"];
    let env = filterObject(ping.environment, ignore);
    let sections = sectionalizeObject(env);
    GenericSubsection.render(sections, dataDiv, "environment-data-section");

    // We use specialized rendering here to make the addon and plugin listings
    // more readable.
    this.createAddonSection(dataDiv, ping);
  },

  renderPersona(addonObj, addonSection, sectionTitle) {
    let table = document.createElement("table");
    table.setAttribute("id", sectionTitle);
    this.appendAddonSubsectionTitle(sectionTitle, table);
    this.appendRow(table, "persona", addonObj.persona);
    addonSection.appendChild(table);
  },

  renderActivePlugins(addonObj, addonSection, sectionTitle) {
    let table = document.createElement("table");
    table.setAttribute("id", sectionTitle);
    this.appendAddonSubsectionTitle(sectionTitle, table);

    for (let plugin of addonObj) {
      let data = explodeObject(plugin);
      this.appendHeadingName(table, data.get("name"));

      for (let [key, value] of data) {
        this.appendRow(table, key, value);
      }
    }

    addonSection.appendChild(table);
  },

  renderAddonsObject(addonObj, addonSection, sectionTitle) {
    let table = document.createElement("table");
    table.setAttribute("id", sectionTitle);
    this.appendAddonSubsectionTitle(sectionTitle, table);

    for (let id of Object.keys(addonObj)) {
      let addon = addonObj[id];
      this.appendHeadingName(table, addon.name || id);
      this.appendAddonID(table, id);
      let data = explodeObject(addon);

      for (let [key, value] of data) {
        this.appendRow(table, key, value);
      }
    }

    addonSection.appendChild(table);
  },

  renderKeyValueObject(addonObj, addonSection, sectionTitle) {
    let data = explodeObject(addonObj);
    let table = GenericTable.render(data);
    table.setAttribute("class", sectionTitle);
    this.appendAddonSubsectionTitle(sectionTitle, table);
    addonSection.appendChild(table);
  },

  appendAddonID(table, addonID) {
    this.appendRow(table, "id", addonID);
  },

  appendHeadingName(table, name) {
    let headings = document.createElement("tr");
    this.appendColumn(headings, "th", name);
    headings.cells[0].colSpan = 2;
    table.appendChild(headings);
  },

  appendAddonSubsectionTitle(section, table) {
    let caption = document.createElement("caption");
    caption.appendChild(document.createTextNode(section));
    table.appendChild(caption);
  },

  createAddonSection(dataDiv, ping) {
    if (!ping || !("environment" in ping) || !("addons" in ping.environment)) {
      return;
    }
    let addonSection = document.createElement("div");
    addonSection.setAttribute("class", "subsection-data subdata");
    let addons = ping.environment.addons;
    this.renderAddonsObject(addons.activeAddons, addonSection, "activeAddons");
    this.renderActivePlugins(addons.activePlugins, addonSection, "activePlugins");
    this.renderKeyValueObject(addons.theme, addonSection, "theme");
    this.renderKeyValueObject(addons.activeExperiment, addonSection, "activeExperiment");
    this.renderAddonsObject(addons.activeGMPlugins, addonSection, "activeGMPlugins");
    this.renderPersona(addons, addonSection, "persona");

    let hasAddonData = Object.keys(ping.environment.addons).length > 0;
    let s = GenericSubsection.renderSubsectionHeader("addons", hasAddonData, "environment-data-section");
    s.appendChild(addonSection);
    dataDiv.appendChild(s);
  },

  appendRow(table, id, value) {
    let row = document.createElement("tr");
    row.id = id;
    this.appendColumn(row, "td", id);
    this.appendColumn(row, "td", value);
    table.appendChild(row);
  },
  /**
   * Helper function for appending a column to the data table.
   *
   * @param aRowElement Parent row element
   * @param aColType Column's tag name
   * @param aColText Column contents
   */
  appendColumn(aRowElement, aColType, aColText) {
    let colElement = document.createElement(aColType);
    let colTextElement = document.createTextNode(aColText);
    colElement.appendChild(colTextElement);
    aRowElement.appendChild(colElement);
  },
};

var SlowSQL = {

  slowSqlHits: bundle.GetStringFromName("slowSqlHits"),

  slowSqlAverage: bundle.GetStringFromName("slowSqlAverage"),

  slowSqlStatement: bundle.GetStringFromName("slowSqlStatement"),

  mainThreadTitle: bundle.GetStringFromName("slowSqlMain"),

  otherThreadTitle: bundle.GetStringFromName("slowSqlOther"),

  /**
   * Render slow SQL statistics
   */
  render: function SlowSQL_render(aPing) {
    // We can add the debug SQL data to the current ping later.
    // However, we need to be careful to never send that debug data
    // out due to privacy concerns.
    // We want to show the actual ping data for archived pings,
    // so skip this there.
    let debugSlowSql = PingPicker.viewCurrentPingData && Preferences.get(PREF_DEBUG_SLOW_SQL, false);
    let slowSql = debugSlowSql ? Telemetry.debugSlowSQL : aPing.payload.slowSQL;
    if (!slowSql) {
      setHasData("slow-sql-section", false);
      return;
    }

    let {mainThread, otherThreads} =
      debugSlowSql ? Telemetry.debugSlowSQL : aPing.payload.slowSQL;

    let mainThreadCount = Object.keys(mainThread).length;
    let otherThreadCount = Object.keys(otherThreads).length;
    if (mainThreadCount == 0 && otherThreadCount == 0) {
      setHasData("slow-sql-section", false);
      return;
    }

    setHasData("slow-sql-section", true);
    if (debugSlowSql) {
      document.getElementById("sql-warning").hidden = false;
    }

    let slowSqlDiv = document.getElementById("slow-sql-tables");
    removeAllChildNodes(slowSqlDiv);

    // Main thread
    if (mainThreadCount > 0) {
      let table = document.createElement("table");
      this.renderTableHeader(table, this.mainThreadTitle);
      this.renderTable(table, mainThread);

      slowSqlDiv.appendChild(table);
    }

    // Other threads
    if (otherThreadCount > 0) {
      let table = document.createElement("table");
      this.renderTableHeader(table, this.otherThreadTitle);
      this.renderTable(table, otherThreads);

      slowSqlDiv.appendChild(table);
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
    for (let [sql, [hitCount, totalTime]] of Object.entries(aSql)) {
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

var StackRenderer = {

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
    let div = document.getElementById(aPrefix);
    removeAllChildNodes(div);

    let fetchE = document.getElementById(aPrefix + "-fetch-symbols");
    if (fetchE) {
      fetchE.hidden = false;
    }
    let hideE = document.getElementById(aPrefix + "-hide-symbols");
    if (hideE) {
      hideE.hidden = true;
    }

    if (aStacks.length == 0) {
      return;
    }

    setHasData(aPrefix + "-section", true);

    this.renderMemoryMap(div, aMemoryMap);

    for (let i = 0; i < aStacks.length; ++i) {
      let stack = aStacks[i];
      aRenderHeader(i);
      this.renderStack(div, stack);
    }
  },

  /**
   * Renders the title of the stack: e.g. "Late Write #1" or
   * "Hang Report #1 (6 seconds)".
   *
   * @param aFormatArgs formating args to be passed to formatStringFromName.
   */
  renderHeader: function StackRenderer_renderHeader(aPrefix, aFormatArgs) {
    let div = document.getElementById(aPrefix);

    let titleElement = document.createElement("span");
    titleElement.className = "stack-title";

    let titleText = bundle.formatStringFromName(
      aPrefix + "-title", aFormatArgs, aFormatArgs.length);
    titleElement.appendChild(document.createTextNode(titleText));

    div.appendChild(titleElement);
    div.appendChild(document.createElement("br"));
  }
};

var RawPayloadData = {
  /**
   * Renders the raw pyaload.
   */
  render(aPing) {
    setHasData("raw-payload-section", true);
    let pre = document.getElementById("raw-payload-data");
    pre.textContent = JSON.stringify(aPing.payload, null, 2);
  },

  attachObservers() {
    document.getElementById("payload-json-viewer").addEventListener("click", (e) => {
      openJsonInFirefoxJsonViewer(JSON.stringify(gPingData.payload, null, 2));
    });
  }
};

function SymbolicationRequest(aPrefix, aRenderHeader,
                              aMemoryMap, aStacks, aDurations = null) {
  this.prefix = aPrefix;
  this.renderHeader = aRenderHeader;
  this.memoryMap = aMemoryMap;
  this.stacks = aStacks;
  this.durations = aDurations;
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
  fetchElement.hidden = true;
  let hideElement = document.getElementById(this.prefix + "-hide-symbols");
  hideElement.hidden = false;
  let div = document.getElementById(this.prefix);
  removeAllChildNodes(div);
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
    this.renderHeader(i, this.durations);

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
    Preferences.get(PREF_SYMBOL_SERVER_URI, DEFAULT_SYMBOL_SERVER_URI);
  let request = {"memoryMap": this.memoryMap, "stacks": this.stacks,
                 "version": 3};
  let requestJSON = JSON.stringify(request);

  this.symbolRequest = new XMLHttpRequest();
  this.symbolRequest.open("POST", symbolServerURI, true);
  this.symbolRequest.setRequestHeader("Content-type", "application/json");
  this.symbolRequest.setRequestHeader("Content-length",
                                      requestJSON.length);
  this.symbolRequest.setRequestHeader("Connection", "close");
  this.symbolRequest.onreadystatechange = this.handleSymbolResponse.bind(this);
  this.symbolRequest.send(requestJSON);
};

var ChromeHangs = {

  symbolRequest: null,

  /**
   * Renders raw chrome hang data
   */
  render: function ChromeHangs_render(chromeHangs) {
    setHasData("chrome-hangs-section", !!chromeHangs);
    if (!chromeHangs) {
      return;
    }

    let stacks = chromeHangs.stacks;
    let memoryMap = chromeHangs.memoryMap;
    let durations = chromeHangs.durations;

    StackRenderer.renderStacks("chrome-hangs", stacks, memoryMap,
                               (index) => this.renderHangHeader(index, durations));
  },

  renderHangHeader: function ChromeHangs_renderHangHeader(aIndex, aDurations) {
    StackRenderer.renderHeader("chrome-hangs", [aIndex + 1, aDurations[aIndex]]);
  }
};

var CapturedStacks = {
  symbolRequest: null,

  render: function CapturedStacks_render(payload) {
    // Retrieve captured stacks from telemetry payload.
    let capturedStacks = "processes" in payload && "parent" in payload.processes
      ? payload.processes.parent.capturedStacks
      : false;
    let hasData = capturedStacks && capturedStacks.stacks &&
                  capturedStacks.stacks.length > 0;
    setHasData("captured-stacks-section", hasData);
    if (!hasData) {
      return;
    }

    let stacks = capturedStacks.stacks;
    let memoryMap = capturedStacks.memoryMap;
    let captures = capturedStacks.captures;

    StackRenderer.renderStacks("captured-stacks", stacks, memoryMap,
                              (index) => this.renderCaptureHeader(index, captures));
  },

  renderCaptureHeader: function CaptureStacks_renderCaptureHeader(index, captures) {
    let key = captures[index][0];
    let cardinality = captures[index][2];
    StackRenderer.renderHeader("captured-stacks", [key, cardinality]);
  }
};

var Histogram = {

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
    let options = aOptions || {};
    let hgram = this.processHistogram(aHgram, aName);

    let outerDiv = document.createElement("div");
    outerDiv.className = "histogram";
    outerDiv.id = aName;

    let divTitle = document.createElement("div");
    divTitle.classList.add("histogram-title");
    divTitle.appendChild(document.createTextNode(aName));
    outerDiv.appendChild(divTitle);

    let stats = hgram.sample_count + " " + this.hgramSamplesCaption + ", " +
                this.hgramAverageCaption + " = " + hgram.pretty_average + ", " +
                this.hgramSumCaption + " = " + hgram.sum;

    let divStats = document.createElement("div");
    divStats.classList.add("histogram-stats");
    divStats.appendChild(document.createTextNode(stats));
    outerDiv.appendChild(divStats);

    if (isRTL()) {
      hgram.values.reverse();
    }

    let textData = this.renderValues(outerDiv, hgram, options);

    // The 'Copy' button contains the textual data, copied to clipboard on click
    let copyButton = document.createElement("button");
    copyButton.className = "copy-node";
    copyButton.appendChild(document.createTextNode(this.hgramCopyCaption));
    copyButton.histogramText = aName + EOL + stats + EOL + EOL + textData;
    copyButton.addEventListener("click", function() {
      Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper)
                                                 .copyString(this.histogramText);
    });
    outerDiv.appendChild(copyButton);

    aParent.appendChild(outerDiv);
    return outerDiv;
  },

  processHistogram(aHgram, aName) {
    const values = Object.keys(aHgram.values).map(k => aHgram.values[k]);
    if (!values.length) {
      // If we have no values collected for this histogram, just return
      // zero values so we still render it.
      return {
        values: [],
        pretty_average: 0,
        max: 0,
        sample_count: 0,
        sum: 0
      };
    }

    const sample_count = values.reduceRight((a, b) => a + b);
    const average = Math.round(aHgram.sum * 10 / sample_count) / 10;
    const max_value = Math.max(...values);

    const labelledValues = Object.keys(aHgram.values)
                           .map(k => [Number(k), aHgram.values[k]]);

    let result = {
      values: labelledValues,
      pretty_average: average,
      max: max_value,
      sample_count,
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
  getLogValue(aNumber) {
    return Math.max(0, Math.log10(aNumber) + 1);
  },

  /**
   * Create histogram HTML bars, also returns a textual representation
   * Both aMaxValue and aSumValues must be positive.
   * Values are assumed to use 0 as baseline.
   *
   * @param aDiv Outer parent div
   * @param aHgram The histogram data
   * @param aOptions Object with render options (@see #render)
   */
  renderValues: function Histogram_renderValues(aDiv, aHgram, aOptions) {
    let text = "";
    // If the last label is not the longest string, alignment will break a little
    let labelPadTo = 0;
    if (aHgram.values.length) {
      labelPadTo = String(aHgram.values[aHgram.values.length - 1][0]).length;
    }
    let maxBarValue = aOptions.exponential ? this.getLogValue(aHgram.max) : aHgram.max;

    for (let [label, value] of aHgram.values) {
      label = String(label);
      let barValue = aOptions.exponential ? this.getLogValue(value) : value;

      // Create a text representation: <right-aligned-label> |<bar-of-#><value>  <percentage>
      text += EOL
              + " ".repeat(Math.max(0, labelPadTo - label.length)) + label // Right-aligned label
              + " |" + "#".repeat(Math.round(MAX_BAR_CHARS * barValue / maxBarValue)) // Bar
              + "  " + value // Value
              + "  " + Math.round(100 * value / aHgram.sample_count) + "%"; // Percentage

      // Construct the HTML labels + bars
      let belowEm = Math.round(MAX_BAR_HEIGHT * (barValue / maxBarValue) * 10) / 10;
      let aboveEm = MAX_BAR_HEIGHT - belowEm;

      let barDiv = document.createElement("div");
      barDiv.className = "bar";
      barDiv.style.paddingTop = aboveEm + "em";

      // Add value label or an nbsp if no value
      barDiv.appendChild(document.createTextNode(value ? value : "\u00A0"));

      // Create the blue bar
      let bar = document.createElement("div");
      bar.className = "bar-inner";
      bar.style.height = belowEm + "em";
      barDiv.appendChild(bar);

      // Add a special class to move the text down to prevent text overlap
      if (label.length > 3) {
          bar.classList.add("long-label");
      }
      // Add bucket label
      barDiv.appendChild(document.createTextNode(label));

      aDiv.appendChild(barDiv);
    }

    return text.substr(EOL.length); // Trim the EOL before the first line
  },
};


var Search = {

  HASH_SEARCH: "search=",

  // A list of ids of sections that do not support search.
  blacklist: [
    "late-writes-section",
    "chrome-hangs-section",
    "raw-payload-section"
  ],

  // Pass if: all non-empty array items match (case-sensitive)
  isPassText(subject, filter) {
    for (let item of filter) {
      if (item.length && !subject.includes(item)) {
        return false; // mismatch and not a spurious space
      }
    }
    return true;
  },

  isPassRegex(subject, filter) {
    return filter.test(subject);
  },

  chooseFilter(filterText) {
    let filter = filterText.toString();
    // Setup normalized filter string (trimmed, lower cased and split on spaces if not RegEx)
    let isPassFunc; // filter function, set once, then applied to all elements
    filter = filter.trim();
    if (filter[0] != "/") { // Plain text: case insensitive, AND if multi-string
      isPassFunc = this.isPassText;
      filter = filter.toLowerCase().split(" ");
    } else {
      isPassFunc = this.isPassRegex;
      var r = filter.match(/^\/(.*)\/(i?)$/);
      try {
        filter = RegExp(r[1], r[2]);
      } catch (e) { // Incomplete or bad RegExp - always no match
        isPassFunc = function() {
          return false;
        };
      }
    }
    return [isPassFunc, filter];
  },

  filterElements(elements, filterText) {
    let [isPassFunc, filter] = this.chooseFilter(filterText);
    let allElementHidden = true;

    let needLowerCase = (isPassFunc === this.isPassText);
    for (let element of elements) {
      let subject = needLowerCase ? element.id.toLowerCase() : element.id;
      element.hidden = !isPassFunc(subject, filter);
      if (allElementHidden && !element.hidden) {
        allElementHidden = false;
      }
    }
    return allElementHidden;
  },

  filterKeyedElements(keyedElements, filterText) {
    let [isPassFunc, filter] = this.chooseFilter(filterText);
    let allElementsHidden = true;

    let needLowerCase = (isPassFunc === this.isPassText);
    keyedElements.forEach((keyedElement) => {
      let subject = needLowerCase ? keyedElement.key.id.toLowerCase() : keyedElement.key.id;
      if (!isPassFunc(subject, filter)) { // If the keyedHistogram's name is not matched
        let allKeyedElementsHidden = true;
        for (let element of keyedElement.datas) {
          let subject = needLowerCase ? element.id.toLowerCase() : element.id;
          let match = isPassFunc(subject, filter);
          element.hidden = !match;
          if (match) {
            allKeyedElementsHidden = false;
          }
        }
        if (allElementsHidden && !allKeyedElementsHidden) {
          allElementsHidden = false;
        }
        keyedElement.key.hidden = allKeyedElementsHidden;
      } else { // If the keyedHistogram's name is matched
        allElementsHidden = false;
        keyedElement.key.hidden = false;
        for (let element of keyedElement.datas) {
          element.hidden = false;
        }
      }
    });
    return allElementsHidden;
  },

  searchHandler(e) {
    if (this.idleTimeout) {
      clearTimeout(this.idleTimeout);
    }
    this.idleTimeout = setTimeout(() => Search.search(e.target.value), FILTER_IDLE_TIMEOUT);
  },

  search(text, sectionParam = null) {
    let section = sectionParam;
    if (!section) {
      let sectionId = document.querySelector(".category.selected").getAttribute("value");
      section = document.getElementById(sectionId);
    }
    if (Search.blacklist.includes(section.id)) {
      return false;
    }
    let noSearchResults = true;
    if (section.id === "home-section") {
      return this.homeSearch(text);
    } else if (section.id === "histograms-section") {
      let histograms = section.getElementsByClassName("histogram");
      noSearchResults = this.filterElements(histograms, text);
    } else if (section.id === "keyed-histograms-section") {
      let keyedElements = [];
      let keyedHistograms = section.getElementsByClassName("keyed-histogram");
      for (let key of keyedHistograms) {
        let datas = key.getElementsByClassName("histogram");
        keyedElements.push({key, datas});
      }
      noSearchResults = this.filterKeyedElements(keyedElements, text);
    } else if (section.id === "keyed-scalars-section") {
      let keyedElements = [];
      let keyedScalars = section.getElementsByClassName("keyed-scalar");
      for (let key of keyedScalars) {
        let datas = key.querySelector("table").rows;
        keyedElements.push({key, datas});
      }
      noSearchResults = this.filterKeyedElements(keyedElements, text);
    } else if (section.querySelector(".sub-section")) {
      let keyedSubSections = [];
      let subsections = section.querySelectorAll(".sub-section");
      for (let section of subsections) {
        let datas = section.querySelector("table").rows;
        keyedSubSections.push({key: section, datas});
      }
      noSearchResults = this.filterKeyedElements(keyedSubSections, text);
    } else {
      let tables = section.querySelectorAll("table");
      for (let table of tables) {
        noSearchResults = this.filterElements(table.rows, text);
        if (table.caption) {
          table.caption.hidden = noSearchResults;
        }
      }
    }

    changeUrlSearch(text);

    if (!sectionParam) { // If we are not searching in all section.
      this.updateNoResults(text, noSearchResults);
    }
    return noSearchResults;
  },

  updateNoResults(text, noSearchResults) {
    document.getElementById("no-search-results").classList.toggle("hidden", !noSearchResults);
    if (noSearchResults) {
      let searchStatus;
      let section = document.querySelector(".category.selected > span");
      if (section.parentElement.id === "category-home") {
        searchStatus = bundle.formatStringFromName("noSearchResultsAll", [text], 1);
      } else {
        let sectionName = section.textContent.trim();
        searchStatus = (text === "")
          ? bundle.formatStringFromName("noDataToDisplay", [sectionName], 1)
          : bundle.formatStringFromName("noSearchResults", [sectionName, text], 2);
      }
      document.getElementById("no-search-results-text").textContent = searchStatus;
    }
  },

  resetHome() {
    document.getElementById("main").classList.remove("search");
    document.getElementById("no-search-results").classList.add("hidden");
    adjustHeaderState();
    Array.from(document.querySelectorAll("section")).forEach((section) => {
      section.classList.toggle("active", section.id == "home-section");
    });
  },

  homeSearch(text) {
    changeUrlSearch(text);
    removeSearchSectionTitles();
    if (text === "") {
      this.resetHome();
      return;
    }
    document.getElementById("main").classList.add("search");
    let title = bundle.formatStringFromName("resultsForSearch", [text], 1);
    adjustHeaderState(title);
    let noSearchResults = true;
    Array.from(document.querySelectorAll("section")).forEach((section) => {
      if (section.id == "home-section" || section.id == "raw-payload-section") {
        section.classList.remove("active");
        return;
      }
      section.classList.add("active");
      let sectionHidden = this.search(text, section);
      if (!sectionHidden) {
        let sectionTitle = document.querySelector(`.category[value="${section.id}"] .category-name`).textContent;
        let sectionDataDiv = document.querySelector(`#${section.id}.has-data.active .data`);
        let titleDiv = document.createElement("h1");
        titleDiv.classList.add("data", "search-section-title");
        titleDiv.textContent = sectionTitle;
        section.insertBefore(titleDiv, sectionDataDiv);
        noSearchResults = false;
      }
    });
    this.updateNoResults(text, noSearchResults);
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
}

var GenericSubsection = {

  addSubSectionToSidebar(id, title) {
    let category = document.querySelector("#categories > [value=" + id + "]");
    category.classList.add("has-subsection");
    let subCategory = document.createElement("div");
    subCategory.classList.add("category-subsection");
    subCategory.setAttribute("value", id + "-" + title);
    subCategory.addEventListener("click", (ev) => {
      let section = ev.target;
      showSubSection(section);
    });
    subCategory.appendChild(document.createTextNode(title));
    category.appendChild(subCategory);
  },

  render(data, dataDiv, sectionID) {
    for (let [title, sectionData] of data) {
      let hasData = sectionData.size > 0;
      let s = this.renderSubsectionHeader(title, hasData, sectionID);
      s.appendChild(this.renderSubsectionData(title, sectionData));
      dataDiv.appendChild(s);
    }
  },

  renderSubsectionHeader(title, hasData, sectionID) {
    this.addSubSectionToSidebar(sectionID, title);
    let section = document.createElement("div");
    section.setAttribute("id", sectionID + "-" + title);
    section.classList.add("sub-section");
    if (hasData) {
      section.classList.add("has-subdata");
    }
    return section;
  },

  renderSubsectionData(title, data) {
    // Create data container
    let dataDiv = document.createElement("div");
    dataDiv.setAttribute("class", "subsection-data subdata");
    // Instanciate the data
    let table = GenericTable.render(data);
    let caption = document.createElement("caption");
    caption.textContent = title;
    table.appendChild(caption);
    dataDiv.appendChild(table);

    return dataDiv;
  },

  deleteAllSubSections() {
    let subsections = document.querySelectorAll(".category-subsection");
    subsections.forEach((el) => {
      el.parentElement.removeChild(el);
    });
  },

};

var GenericTable = {

  defaultHeadings: [
    bundle.GetStringFromName("keysHeader"),
    bundle.GetStringFromName("valuesHeader")
  ],

  /**
   * Returns a n-column table.
   * @param rows An array of arrays, each containing data to render
   *             for one row.
   * @param headings The column header strings.
   */
  render(rows, headings = this.defaultHeadings) {
    let table = document.createElement("table");
    this.renderHeader(table, headings);
    this.renderBody(table, rows);
    return table;
  },

  /**
   * Create the table header.
   * Tabs & newlines added to cells to make it easier to copy-paste.
   *
   * @param table Table element
   * @param headings Array of column header strings.
   */
  renderHeader(table, headings) {
    let headerRow = document.createElement("tr");
    table.appendChild(headerRow);

    for (let i = 0; i < headings.length; ++i) {
      let suffix = (i == (headings.length - 1)) ? "\n" : "\t";
      let column = document.createElement("th");
      column.appendChild(document.createTextNode(headings[i] + suffix));
      headerRow.appendChild(column);
    }
  },

  /**
   * Create the table body
   * Tabs & newlines added to cells to make it easier to copy-paste.
   *
   * @param table Table element
   * @param rows An array of arrays, each containing data to render
   *             for one row.
   */
  renderBody(table, rows) {
    for (let row of rows) {
      row = row.map(value => {
        // use .valueOf() to unbox Number, String, etc. objects
        if (value &&
           (typeof value == "object") &&
           (typeof value.valueOf() == "object")) {
          return RenderObject(value);
        }
        return value;
      });

      let newRow = document.createElement("tr");
      newRow.id = row[0];
      table.appendChild(newRow);

      for (let i = 0; i < row.length; ++i) {
        let suffix = (i == (row.length - 1)) ? "\n" : "\t";
        let field = document.createElement("td");
        field.appendChild(document.createTextNode(row[i] + suffix));
        newRow.appendChild(field);
      }
    }
  },
};

var KeyedHistogram = {
  render(parent, id, keyedHistogram) {
    let outerDiv = document.createElement("div");
    outerDiv.className = "keyed-histogram";
    outerDiv.id = id;

    let divTitle = document.createElement("div");
    divTitle.classList.add("keyed-title");
    divTitle.appendChild(document.createTextNode(id));
    outerDiv.appendChild(divTitle);

    for (let [name, hgram] of Object.entries(keyedHistogram)) {
      Histogram.render(outerDiv, name, hgram);
    }

    parent.appendChild(outerDiv);
    return outerDiv;
  },
};

var AddonDetails = {
  tableIDTitle: bundle.GetStringFromName("addonTableID"),
  tableDetailsTitle: bundle.GetStringFromName("addonTableDetails"),

  /**
   * Render the addon details section as a series of headers followed by key/value tables
   * @param aPing A ping object to render the data from.
   */
  render: function AddonDetails_render(aPing) {
    let addonSection = document.getElementById("addon-details");
    removeAllChildNodes(addonSection);
    let addonDetails = aPing.payload.addonDetails;
    const hasData = addonDetails && Object.keys(addonDetails).length > 0;
    setHasData("addon-details-section", hasData);
    if (!hasData) {
      return;
    }

    for (let provider in addonDetails) {
      let providerSection = document.createElement("caption");
      let titleText = bundle.formatStringFromName("addonProvider", [provider], 1);
      providerSection.appendChild(document.createTextNode(titleText));

      let headingStrings = [this.tableIDTitle, this.tableDetailsTitle ];
      let table = GenericTable.render(explodeObject(addonDetails[provider]),
                                      headingStrings);
      table.appendChild(providerSection);
      addonSection.appendChild(table);
    }
  },
};

var Scalars = {
  /**
   * Render the scalar data - if present - from the payload in a simple key-value table.
   * @param aPayload A payload object to render the data from.
   */
  render(aPayload) {
    let scalarsSection = document.getElementById("scalars");
    removeAllChildNodes(scalarsSection);

    let processesSelect = document.getElementById("processes");
    let selectedProcess = processesSelect.selectedOptions.item(0).getAttribute("value");

    if (!aPayload.processes ||
        !selectedProcess ||
        !(selectedProcess in aPayload.processes)) {
      return;
    }

    let scalars = aPayload.processes[selectedProcess].scalars || {};
    let hasData = Array.from(processesSelect.options).some((option) => {
      let value = option.getAttribute("value");
      let sclrs = aPayload.processes[value].scalars;
      return sclrs && Object.keys(sclrs).length > 0;
    });
    setHasData("scalars-section", hasData);
    if (Object.keys(scalars).length > 0) {
      const headings = [
        "namesHeader",
        "valuesHeader",
      ].map(h => bundle.GetStringFromName(h));
      const table = GenericTable.render(explodeObject(scalars), headings);
      scalarsSection.appendChild(table);
    }
  },
};

var KeyedScalars = {
  /**
   * Render the keyed scalar data - if present - from the payload in a simple key-value table.
   * @param aPayload A payload object to render the data from.
   */
  render(aPayload) {
    let scalarsSection = document.getElementById("keyed-scalars");
    removeAllChildNodes(scalarsSection);

    let processesSelect = document.getElementById("processes");
    let selectedProcess = processesSelect.selectedOptions.item(0).getAttribute("value");

    if (!aPayload.processes ||
        !selectedProcess ||
        !(selectedProcess in aPayload.processes)) {
      return;
    }

    let keyedScalars = aPayload.processes[selectedProcess].keyedScalars || {};
    let hasData = Array.from(processesSelect.options).some((option) => {
      let value = option.getAttribute("value");
      let keyedS = aPayload.processes[value].keyedScalars;
      return keyedS && Object.keys(keyedS).length > 0;
    });
    setHasData("keyed-scalars-section", hasData);
    if (!Object.keys(keyedScalars).length > 0) {
      return;
    }

    const headings = [
      "namesHeader",
      "valuesHeader",
    ].map(h => bundle.GetStringFromName(h));
    for (let scalar in keyedScalars) {
      // Add the name of the scalar.
      let container = document.createElement("div");
      container.classList.add("keyed-scalar");
      container.id = scalar;
      let scalarNameSection = document.createElement("p");
      scalarNameSection.classList.add("keyed-title");
      scalarNameSection.appendChild(document.createTextNode(scalar));
      container.appendChild(scalarNameSection);
      // Populate the section with the key-value pairs from the scalar.
      const table = GenericTable.render(explodeObject(keyedScalars[scalar]), headings);
      container.appendChild(table);
      scalarsSection.appendChild(container);
    }
  },
};

var Events = {
  /**
   * Render the event data - if present - from the payload in a simple table.
   * @param aPayload A payload object to render the data from.
   */
  render(aPayload) {
    let eventsSection = document.getElementById("events");
    removeAllChildNodes(eventsSection);

    let processesSelect = document.getElementById("processes");
    let selectedProcess = processesSelect.selectedOptions.item(0).getAttribute("value");

    if (!aPayload.processes ||
        !selectedProcess ||
        !(selectedProcess in aPayload.processes)) {
      return;
    }

    let events = aPayload.processes[selectedProcess].events || {};
    let hasData = Array.from(processesSelect.options).some((option) => {
      let value = option.getAttribute("value");
      let evts = aPayload.processes[value].events;
      return evts && Object.keys(evts).length > 0;
    });
    setHasData("events-section", hasData);
    if (Object.keys(events).length > 0) {
      const headings = [
        "timestampHeader",
        "categoryHeader",
        "methodHeader",
        "objectHeader",
        "valuesHeader",
        "extraHeader",
      ].map(h => bundle.GetStringFromName(h));

      const table = GenericTable.render(events, headings);
      eventsSection.appendChild(table);
    }
  },
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

  // Display or Hide the section in the sidebar
  let sectionCategory = document.querySelector(".category[value=" + aSectionID + "]");
  sectionCategory.classList[aHasData ? "add" : "remove"]("has-data");
}

/**
 * Sets the text of the page header based on a config pref + bundle strings
 */
function setupPageHeader() {
  let serverOwner = Preferences.get(PREF_TELEMETRY_SERVER_OWNER, "Mozilla");
  let brandName = brandBundle.GetStringFromName("brandFullName");
  let subtitleText = bundle.formatStringFromName(
    "pageSubtitle", [serverOwner, brandName], 2);

  let subtitleElement = document.getElementById("page-subtitle");
  subtitleElement.appendChild(document.createTextNode(subtitleText));

  let links = [
    "https://docs.telemetry.mozilla.org/",
    "https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/index.html",
    "https://telemetry.mozilla.org/",
  ];
  let htmlLink = document.querySelectorAll("#home-section > ul > li > a");
  htmlLink.forEach((a, index) => {
    a.href = links[index];
  });
}

function displayProcessesSelector(selectedSection) {
  let whitelist = [
    "scalars-section",
    "keyed-scalars-section",
    "histograms-section",
    "keyed-histograms-section",
    "events-section"
  ];
  let processes = document.getElementById("processes");
  processes.hidden = !whitelist.includes(selectedSection);
}

function refreshSearch() {
  removeSearchSectionTitles();
  let selectedSection = document.querySelector(".category.selected").getAttribute("value");
  let search = document.getElementById("search");
  if (!Search.blacklist.includes(selectedSection)) {
    Search.search(search.value);
  }
}

function adjustSearchState() {
  removeSearchSectionTitles();
  let selectedSection = document.querySelector(".category.selected").getAttribute("value");
  let search = document.getElementById("search");
  search.value = "";
  search.hidden = Search.blacklist.includes(selectedSection);
  document.getElementById("no-search-results").classList.add("hidden");
  Search.search(""); // reinitialize search state.
}

function removeSearchSectionTitles() {
    for (let sectionTitleDiv of Array.from(document.getElementsByClassName("search-section-title"))) {
        sectionTitleDiv.remove();
    }
}

function adjustSection() {
  let selectedCategory = document.querySelector(".category.selected");
  if (!selectedCategory.classList.contains("has-data")) {
    PingPicker._showStructuredPingData();
  }
}

function adjustHeaderState(title = null) {
  let selected = document.querySelector(".category.selected .category-name");
  let selectedTitle = selected.textContent.trim();
  document.getElementById("sectionTitle").textContent = title ? title : selectedTitle;

  let placeholder;
  if (selected.parentElement.id === "category-home") {
    placeholder = bundle.GetStringFromName("filterAllPlaceholder");
  } else {
    placeholder = bundle.formatStringFromName("filterPlaceholder", [ selectedTitle ], 1);
  }
  let search = document.getElementById("search");
  search.setAttribute("placeholder", placeholder);
}

/**
 * Change the url according to the current section displayed
 * e.g about:telemetry#general-data
 */
function changeUrlPath(selectedSection, subSection) {
  if (subSection) {
    let hash = window.location.hash.split("_")[0] + "_" + selectedSection;
    window.location.hash = hash;
  } else {
    window.location.hash = selectedSection.replace("-section", "-tab");
  }
}

/**
 * Change the url according to the current search text
 */
function changeUrlSearch(searchText) {
  let currentHash = window.location.hash;
  let hashWithoutSearch = currentHash.split(Search.HASH_SEARCH)[0];
  let hash = "";

  if (!currentHash && !searchText) {
    return;
  }
  if (!currentHash.includes(Search.HASH_SEARCH) && hashWithoutSearch) {
    hashWithoutSearch += "_";
  }
  if (searchText) {
    hash = hashWithoutSearch + Search.HASH_SEARCH + searchText.replace(/ /g, "+");
  } else if (hashWithoutSearch) {
    hash = hashWithoutSearch.slice(0, hashWithoutSearch.length - 1);
  }

  window.location.hash = hash;
}

/**
 * Change the section displayed
 */
function show(selected) {
  let selectedValue = selected.getAttribute("value");
  if (selectedValue === "raw-json-viewer") {
    openJsonInFirefoxJsonViewer(JSON.stringify(gPingData, null, 2));
    return;
  }

  let selected_section = document.getElementById(selectedValue);
  let subsections = selected_section.querySelectorAll(".sub-section");
  if (selected.classList.contains("has-subsection")) {
    for (let subsection of selected.children) {
      subsection.classList.remove("selected");
    }
  }
  if (subsections) {
    for (let subsection of subsections) {
      subsection.hidden = false;
    }
  }

  let current_button = document.querySelector(".category.selected");
  if (current_button == selected)
    return;
  current_button.classList.remove("selected");
  selected.classList.add("selected");

  document.querySelectorAll("section").forEach((section) => {
    section.classList.remove("active");
  });
  selected_section.classList.add("active");

  adjustHeaderState();
  displayProcessesSelector(selectedValue);
  adjustSearchState();
  changeUrlPath(selectedValue);
}

function showSubSection(selected) {
  if (!selected) {
    return;
  }
  let current_selection = document.querySelector(".category-subsection.selected");
  if (current_selection)
    current_selection.classList.remove("selected");
  selected.classList.add("selected");

  let section = document.getElementById(selected.getAttribute("value"));
  section.parentElement.childNodes.forEach((element) => {
    element.hidden = true;
  });
  section.hidden = false;

  let title = selected.parentElement.querySelector(".category-name").textContent;
  let subsection = selected.textContent;
  document.getElementById("sectionTitle").textContent = title + " - " + subsection;
  changeUrlPath(subsection, true);
}

/**
 * Initializes load/unload, pref change and mouse-click listeners
 */
function setupListeners() {
  Settings.attachObservers();
  PingPicker.attachObservers();
  RawPayloadData.attachObservers();

  let menu = document.getElementById("categories");
  menu.addEventListener("click", (e) => {
    if (e.target && e.target.parentNode == menu) {
      show(e.target);
    }
  });

  let search = document.getElementById("search");
  search.addEventListener("input", Search.searchHandler);

  // Clean up observers when page is closed
  window.addEventListener("unload",
    function(aEvent) {
      Settings.detachObservers();
  }, {once: true});

  document.getElementById("chrome-hangs-fetch-symbols").addEventListener("click",
    function() {
      if (!gPingData) {
        return;
      }

      let hangs = gPingData.payload.chromeHangs;
      let req = new SymbolicationRequest("chrome-hangs",
                                         ChromeHangs.renderHangHeader,
                                         hangs.memoryMap,
                                         hangs.stacks,
                                         hangs.durations);
      req.fetchSymbols();
  });

  document.getElementById("chrome-hangs-hide-symbols").addEventListener("click",
    function() {
      if (!gPingData) {
        return;
      }

      ChromeHangs.render(gPingData.payload.chromeHangs);
  });

  document.getElementById("captured-stacks-fetch-symbols").addEventListener("click",
    function() {
      if (!gPingData) {
        return;
      }
      let capturedStacks = gPingData.payload.processes.parent.capturedStacks;
      let req = new SymbolicationRequest("captured-stacks",
                                         CapturedStacks.renderCaptureHeader,
                                         capturedStacks.memoryMap,
                                         capturedStacks.stacks,
                                         capturedStacks.captures);
      req.fetchSymbols();
  });

  document.getElementById("captured-stacks-hide-symbols").addEventListener("click",
    function() {
      if (gPingData) {
        CapturedStacks.render(gPingData.payload);
      }
  });

  document.getElementById("late-writes-fetch-symbols").addEventListener("click",
    function() {
      if (!gPingData) {
        return;
      }

      let lateWrites = gPingData.payload.lateWrites;
      let req = new SymbolicationRequest("late-writes",
                                         LateWritesSingleton.renderHeader,
                                         lateWrites.memoryMap,
                                         lateWrites.stacks);
      req.fetchSymbols();
  });

  document.getElementById("late-writes-hide-symbols").addEventListener("click",
    function() {
      if (!gPingData) {
        return;
      }

      LateWritesSingleton.renderLateWrites(gPingData.payload.lateWrites);
  });
}

// Restores the sections states
function urlSectionRestore(hash) {
  if (hash) {
    let section = hash.replace("-tab", "-section");
    let subsection = section.split("_")[1];
    section = section.split("_")[0];
    let category = document.querySelector(".category[value=" + section + "]");
    if (category) {
      show(category);
      if (subsection) {
        let selector = ".category-subsection[value=" + section + "-" + subsection + "]";
        let subcategory = document.querySelector(selector);
        showSubSection(subcategory);
      }
    }
  }
}

// Restore sections states and search terms
function urlStateRestore() {
  let hash = window.location.hash;
  let searchQuery = "";
  if (hash) {
    hash = hash.slice(1);
    if (hash.includes(Search.HASH_SEARCH)) {
      searchQuery = hash.split(Search.HASH_SEARCH)[1].replace(/[+]/g, " ");
      hash = hash.split(Search.HASH_SEARCH)[0];
    }
    urlSectionRestore(hash);
  }
  if (searchQuery) {
    let search = document.getElementById("search");
    search.value = searchQuery;
  }
}

function openJsonInFirefoxJsonViewer(json) {
  json = unescape(encodeURIComponent(json));
  try {
    window.open("data:application/json;base64," + btoa(json));
  } catch (e) {
    show(document.querySelector(".category[value=raw-payload-section]"));
  }
}

function onLoad() {
  window.removeEventListener("load", onLoad);

  // Set the text in the page header
  setupPageHeader();

  // Set up event listeners
  setupListeners();

  // Render settings.
  Settings.render();

  adjustHeaderState();

  // Update ping data when async Telemetry init is finished.
  Telemetry.asyncFetchTelemetryData(async () => {
    await PingPicker.update();
    urlStateRestore();
  });
}

var LateWritesSingleton = {
  renderHeader: function LateWritesSingleton_renderHeader(aIndex) {
    StackRenderer.renderHeader("late-writes", [aIndex + 1]);
  },

  renderLateWrites: function LateWritesSingleton_renderLateWrites(lateWrites) {
    setHasData("late-writes-section", !!lateWrites);
    if (!lateWrites) {
      return;
    }

    let stacks = lateWrites.stacks;
    let memoryMap = lateWrites.memoryMap;
    StackRenderer.renderStacks("late-writes", stacks, memoryMap,
                               LateWritesSingleton.renderHeader);
  },
};

var HistogramSection = {
  render(aPayload) {
    let hgramDiv = document.getElementById("histograms");
    removeAllChildNodes(hgramDiv);

    let histograms = {};
    let hgramsSelect = document.getElementById("processes");
    let hgramsOption = hgramsSelect.selectedOptions.item(0);
    let hgramsProcess = hgramsOption.getAttribute("value");

    if (hgramsProcess === "parent") {
      histograms = aPayload.histograms;
    } else if ("processes" in aPayload && hgramsProcess in aPayload.processes &&
               "histograms" in aPayload.processes[hgramsProcess]) {
      histograms = aPayload.processes[hgramsProcess].histograms;
    }

    let hasData = Array.from(hgramsSelect.options).some((option) => {
      let value = option.getAttribute("value");
      if (value == "parent") {
        return Object.keys(aPayload.histograms).length > 0;
      }
      let histos = aPayload.processes[value].histograms;
      return histos && Object.keys(histos).length > 0;
    });
    setHasData("histograms-section", hasData);

    if (Object.keys(histograms).length > 0) {
      for (let [name, hgram] of Object.entries(histograms)) {
        Histogram.render(hgramDiv, name, hgram, {unpacked: true});
      }
    }
  },
};

var KeyedHistogramSection = {
  render(aPayload) {
    let keyedDiv = document.getElementById("keyed-histograms");
    removeAllChildNodes(keyedDiv);

    let keyedHistograms = {};
    let keyedHgramsSelect = document.getElementById("processes");
    let keyedHgramsOption = keyedHgramsSelect.selectedOptions.item(0);
    let keyedHgramsProcess = keyedHgramsOption.getAttribute("value");
    if (keyedHgramsProcess === "parent") {
      keyedHistograms = aPayload.keyedHistograms;
    } else if ("processes" in aPayload && keyedHgramsProcess in aPayload.processes &&
               "keyedHistograms" in aPayload.processes[keyedHgramsProcess]) {
      keyedHistograms = aPayload.processes[keyedHgramsProcess].keyedHistograms;
    }

    let hasData = Array.from(keyedHgramsSelect.options).some((option) => {
      let value = option.getAttribute("value");
      if (value == "parent") {
        return Object.keys(aPayload.keyedHistograms).length > 0;
      }
      let keyedHistos = aPayload.processes[value].keyedHistograms;
      return keyedHistos && Object.keys(keyedHistos).length > 0;
    });
    setHasData("keyed-histograms-section", hasData);
    if (Object.keys(keyedHistograms).length > 0) {
      for (let [id, keyed] of Object.entries(keyedHistograms)) {
        if (Object.keys(keyed).length > 0) {
          KeyedHistogram.render(keyedDiv, id, keyed, {unpacked: true});
        }
      }
    }
  },
};

var SessionInformation = {
  render(aPayload) {
    let infoSection = document.getElementById("session-info");
    removeAllChildNodes(infoSection);

    let hasData = Object.keys(aPayload.info).length > 0;
    setHasData("session-info-section", hasData);

    if (hasData) {
      const table = GenericTable.render(explodeObject(aPayload.info));
      infoSection.appendChild(table);
    }
  },
};

var SimpleMeasurements = {
  render(aPayload) {
    let simpleSection = document.getElementById("simple-measurements");
    removeAllChildNodes(simpleSection);

    let simpleMeasurements = this.sortStartupMilestones(aPayload.simpleMeasurements);
    let hasData = Object.keys(simpleMeasurements).length > 0;
    setHasData("simple-measurements-section", hasData);

    if (hasData) {
      const table = GenericTable.render(explodeObject(simpleMeasurements));
      simpleSection.appendChild(table);
    }
  },

  /**
   * Helper function for sorting the startup milestones in the Simple Measurements
   * section into temporal order.
   *
   * @param aSimpleMeasurements Telemetry ping's "Simple Measurements" data
   * @return Sorted measurements
   */
  sortStartupMilestones(aSimpleMeasurements) {
    const telemetryTimestamps = TelemetryTimestamps.get();
    let startupEvents = Services.startup.getStartupInfo();
    delete startupEvents.process;

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
  },
};

function renderProcessList(ping, selectEl) {
  removeAllChildNodes(selectEl);
  let option = document.createElement("option");
  option.appendChild(document.createTextNode("parent"));
  option.setAttribute("value", "parent");
  option.selected = true;
  selectEl.appendChild(option);

  if (!("processes" in ping.payload)) {
    selectEl.disabled = true;
    return;
  }
  selectEl.disabled = false;

  for (let process of Object.keys(ping.payload.processes)) {
    // TODO: parent hgrams are on root payload, not in payload.processes.parent
    // When/If that gets moved, you'll need to remove this
    if (process === "parent") {
      continue;
    }
    option = document.createElement("option");
    option.appendChild(document.createTextNode(process));
    option.setAttribute("value", process);
    selectEl.appendChild(option);
  }
}

function togglePingSections(isMainPing) {
  // We always show the sections that are "common" to all pings.
  let commonSections = new Set(["heading",
                                "home-section",
                                "general-data-section",
                                "environment-data-section",
                                "raw-json-viewer"]);

  let elements = document.querySelectorAll(".category");
  for (let section of elements) {
    if (commonSections.has(section.getAttribute("value"))) {
      continue;
    }
    // Only show the raw payload for non main ping.
    if (section.getAttribute("value") == "raw-payload-section") {
      section.classList.toggle("has-data", !isMainPing);
    } else {
      section.classList.toggle("has-data", isMainPing);
    }
  }
}

function displayPingData(ping, updatePayloadList = false) {
  gPingData = ping;
  try {
    PingPicker.render();
    displayRichPingData(ping, updatePayloadList);
    adjustSection();
    refreshSearch();
  } catch (err) {
    console.log(err);
    PingPicker._showRawPingData();
  }
}

function displayRichPingData(ping, updatePayloadList) {
  // Update the payload list and process lists
  if (updatePayloadList) {
    renderProcessList(ping, document.getElementById("processes"));
  }

  // Show general data.
  GeneralData.render(ping);

  // Show environment data.
  EnvironmentData.render(ping);

  RawPayloadData.render(ping);

  // We only have special rendering code for the payloads from "main" pings.
  // For any other pings we just render the raw JSON payload.
  let isMainPing = (ping.type == "main" || ping.type == "saved-session");
  togglePingSections(isMainPing);

  if (!isMainPing) {
    return;
  }

  // Show slow SQL stats
  SlowSQL.render(ping);

  // Render Addon details.
  AddonDetails.render(ping);

  let payload = ping.payload;
  // Show basic session info gathered
  SessionInformation.render(payload);

  // Show scalar data.
  Scalars.render(payload);
  KeyedScalars.render(payload);

  // Show histogram data
  HistogramSection.render(payload);

  // Show keyed histogram data
  KeyedHistogramSection.render(payload);

  // Show event data.
  Events.render(payload);

  // Show captured stacks.
  CapturedStacks.render(payload);

  LateWritesSingleton.renderLateWrites(payload.lateWrites);

  // Show chrome hang stacks
  ChromeHangs.render(payload.chromeHangs);

  // Show simple measurements
  SimpleMeasurements.render(payload);

}

window.addEventListener("load", onLoad);
