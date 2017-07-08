/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/TelemetryTimestamps.jsm");
Cu.import("resource://gre/modules/TelemetryController.jsm");
Cu.import("resource://gre/modules/TelemetryArchive.jsm");
Cu.import("resource://gre/modules/TelemetryUtils.jsm");
Cu.import("resource://gre/modules/TelemetryLog.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");

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
    if (filterOut.indexOf(k) == -1) {
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
  if (mainWindow && "openAdvancedPreferences" in mainWindow) {
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

/**
 * Pad a number to two digits with leading "0".
 */
function padToTwoDigits(n) {
  return new String(n).padStart(2, "0");
}

/**
 * Return yesterdays date with the same time.
 */
function yesterday(date) {
  let d = new Date(date);
  d.setDate(d.getDate() - 1);
  return d;
}

/**
 * Return tomorrow's date with the same time.
 */
function tomorrow(date) {
  let d = new Date(date);
  d.setDate(d.getDate() + 1);
  return d;
}

/**
 * This returns a short date string of the form YYYY/MM/DD.
 */
function shortDateString(date) {
  return date.getFullYear()
         + "/" + padToTwoDigits(date.getMonth() + 1)
         + "/" + padToTwoDigits(date.getDate());
}

/**
 * This returns a short time string of the form hh:mm:ss.
 */
function shortTimeString(date) {
  return padToTwoDigits(date.getHours())
         + ":" + padToTwoDigits(date.getMinutes())
         + ":" + padToTwoDigits(date.getSeconds());
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
          Cu.import("resource://gre/modules/Messaging.jsm");
          EventDispatcher.instance.sendRequest({
            type: "Settings:Show",
            resource: "preferences_privacy",
          });
        } else {
          // Show the data choices preferences on desktop.
          let mainWindow = getMainWindowWithPreferencesPane();
          // The advanced subpanes are only supported in the old organization,
          // which will be removed by bug 1349689.
          if (Preferences.get("browser.preferences.useOldOrganization")) {
            mainWindow.openAdvancedPreferences("dataChoicesTab", {origin: "aboutTelemetry"});
          } else {
            mainWindow.openPreferences("privacy-reports", {origin: "aboutTelemetry"});
          }
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
    let status = bundle.GetStringFromName(enabled ? "telemetryEnabled" : "telemetryDisabled");
    return status;
  },

  /**
   * Updates the button & text at the top of the page to reflect Telemetry state.
   */
  render() {
    let homeExplanation = document.getElementById("home-explanation");
    let fhrEnabled = this.getStatusStringForSetting(this.SETTINGS[0]);
    let extendedEnabled = this.getStatusStringForSetting(this.SETTINGS[1]);
    let parameters = [fhrEnabled, extendedEnabled].map(this.convertStringToLink);

    let explanation = bundle.formatStringFromName("homeExplanation", parameters, 2);

    // eslint-disable-next-line no-unsanitized/property
    homeExplanation.innerHTML = explanation;
    this.attachObservers()
  },

  convertStringToLink(string) {
    return "<a href=\"\" class=\"change-data-choices-link\">" + string + "</a>";
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

    document.addEventListener("click", (ev) => {
      if (ev.target.querySelector("#ping-picker")) {
        document.getElementById("ping-picker").classList.add("hidden");
      }
    });
    document.getElementById("choose-payload")
            .addEventListener("change", () => displayPingData(gPingData));
    document.getElementById("processes")
            .addEventListener("change", () => displayPingData(gPingData));
    Array.from(document.querySelectorAll(".change-ping")).forEach(el =>
      el.addEventListener("click", () =>
        document.getElementById("ping-picker").classList.remove("hidden"))
    );
  },

  onPingSourceChanged() {
    this.update();
  },

  onPingDisplayChanged() {
    this.update();
  },

  render() {
    let pings = bundle.GetStringFromName("pingExplanationLink");
    let pingLink = "<a href=\"http://gecko.readthedocs.io/en/latest/toolkit/components/telemetry/telemetry/concepts/pings.html\">&quot;" + pings + "&quot;</a>";
    let pingName = this._getSelectedPingName();

    let pingDate = document.getElementById("ping-date");
    pingDate.textContent = pingName;
    pingDate.setAttribute("title", pingName);

    // Display the type and controls if the ping is not current
    let pingType = document.getElementById("ping-type");
    let older = document.getElementById("older-ping");
    let newer = document.getElementById("newer-ping");
    if (pingName !== "current") {
      pingType.hidden = false;
      older.hidden = false;
      newer.hidden = false;
      pingType.textContent = this._getSelectedPingType();
    } else {
      pingType.hidden = true;
      older.hidden = true;
      newer.hidden = true;
    }

    if (pingName !== "current") {
      pingName += ", " + this._getSelectedPingType();
    }
    let pingNameHtml = "<span class=\"change-ping\">" + pingName + "</span>";

    let explanation = bundle.formatStringFromName("pingExplanation", [pingLink, pingNameHtml], 2);
    let pingExplanation = document.getElementById("ping-explanation");

    // eslint-disable-next-line no-unsanitized/property
    pingExplanation.innerHTML = explanation;
    pingExplanation.querySelector(".change-ping").addEventListener("click", () =>
      document.getElementById("ping-picker").classList.remove("hidden")
    );

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
    sourceArchived.disabled = (archivedPingList.length == 0);

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
    let todayString =  (new Date()).toDateString();
    let yesterdayString = yesterday(new Date()).toDateString();
    for (let p of this._archivedPings) {
      pingTypes.add(p.type);
      let date = new Date(p.timestampCreated);
      let datetext = date.toLocaleDateString() + " " + shortTimeString(date);
      let text = datetext + ", " + p.type;

      let option = document.createElement("option");
      let content = document.createTextNode(text);
      option.appendChild(content);
      option.setAttribute("value", p.id);
      option.dataset.type = p.type;
      option.dataset.date = datetext;

      if (date.toDateString() == todayString) {
        pingSelector.children[0].appendChild(option);
      } else if (date.toDateString() == yesterdayString) {
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
      });
    });
    this._updateArchivedPingData();
  },

  _getSelectedPingName() {
    if (this.viewCurrentPingData) return "current";

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
    caption.setAttribute("class", "addon-caption");
    caption.appendChild(document.createTextNode(section));
    table.appendChild(caption);
  },

  createAddonSection(dataDiv, ping) {
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

var TelLog = {
  /**
   * Renders the telemetry log
   */
  render(aPing) {
    let entries = aPing.payload.log;
    const hasData = entries && entries.length > 0;
    setHasData("telemetry-log-section", hasData);
    if (!hasData) {
      return;
    }

    let table = document.createElement("table");

    let caption = document.createElement("caption");
    let captionString = bundle.GetStringFromName("telemetryLogTitle");
    caption.appendChild(document.createTextNode(captionString + "\n"));
    table.appendChild(caption);

    let headings = document.createElement("tr");
    this.appendColumn(headings, "th", bundle.GetStringFromName("telemetryLogHeadingId") + "\t");
    this.appendColumn(headings, "th", bundle.GetStringFromName("telemetryLogHeadingTimestamp") + "\t");
    this.appendColumn(headings, "th", bundle.GetStringFromName("telemetryLogHeadingData") + "\t");
    table.appendChild(headings);

    for (let entry of entries) {
        let row = document.createElement("tr");
        for (let elem of entry) {
            this.appendColumn(row, "td", elem + "\t");
        }
        table.appendChild(row);
    }

    let dataDiv = document.getElementById("telemetry-log");
    removeAllChildNodes(dataDiv);
    dataDiv.appendChild(table);
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
    let div = document.getElementById(aPrefix + "-data");
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

var RawPayload = {
  /**
   * Renders the raw payload
   */
  render(aPing) {
    setHasData("raw-ping-data-section", true);
    let pre = document.getElementById("raw-ping-data");
    pre.textContent = JSON.stringify(aPing.payload, null, 2);
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
  let div = document.getElementById(this.prefix + "-data");
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
}

var ChromeHangs = {

  symbolRequest: null,

  /**
   * Renders raw chrome hang data
   */
  render: function ChromeHangs_render(aPing) {
    let hangs = aPing.payload.chromeHangs;
    setHasData("chrome-hangs-section", !!hangs);
    if (!hangs) {
      return;
    }

    let stacks = hangs.stacks;
    let memoryMap = hangs.memoryMap;
    let durations = hangs.durations;

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

var ThreadHangStats = {

  /**
   * Renders raw thread hang stats data
   */
  render(aPayload) {
    let div = document.getElementById("thread-hang-stats");
    removeAllChildNodes(div);

    let stats = aPayload.threadHangStats;
    setHasData("thread-hang-stats-section", stats && (stats.length > 0));
    if (!stats) {
      return;
    }

    stats.forEach((thread) => {
      div.appendChild(this.renderThread(thread));
    });
  },

  /**
   * Creates and fills data corresponding to a thread
   */
  renderThread(aThread) {
    let div = document.createElement("div");

    let title = document.createElement("h2");
    title.textContent = aThread.name;
    div.appendChild(title);

    // Don't localize the histogram name, because the
    // name is also used as the div element's ID
    Histogram.render(div, aThread.name + "-Activity",
                     aThread.activity, {exponential: true}, true);
    aThread.hangs.forEach((hang, index) => {
      let hangName = aThread.name + "-Hang-" + (index + 1);
      let hangDiv = Histogram.render(
        div, hangName, hang.histogram, {exponential: true}, true);
      let stackDiv = document.createElement("div");
      hang.stack.forEach((frame) => {
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
   * @param aIsBHR whether or not requires fixing the labels for TimeHistogram
   */
  render: function Histogram_render(aParent, aName, aHgram, aOptions, aIsBHR) {
    let options = aOptions || {};
    let hgram = this.processHistogram(aHgram, aName, aIsBHR);

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

    if (isRTL()) {
      hgram.buckets.reverse();
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

  processHistogram(aHgram, aName, aIsBHR) {
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

    function labelFunc(k) {
      // - BHR histograms are TimeHistograms: Exactly power-of-two buckets (from 0)
      //   (buckets: [0..1], [2..3], [4..7], [8..15], ... note the 0..1 anomaly - same bucket)
      // - TimeHistogram's JS representation adds a dummy (empty) "0" bucket, and
      //   the rest of the buckets have the label as the upper value of the
      //   bucket (non TimeHistograms have the lower value of the bucket as label).
      //   So JS TimeHistograms bucket labels are: 0 (dummy), 1, 3, 7, 15, ...
      // - see toolkit/components/telemetry/Telemetry.cpp
      //   (CreateJSTimeHistogram, CreateJSThreadHangStats, CreateJSHangHistogram)
      // - see toolkit/components/telemetry/ThreadHangStats.h
      // Fix BHR labels to the "standard" format for about:telemetry as follows:
      //   - The dummy 0 label+bucket will be filtered before arriving here
      //   - If it's 1 -> manually correct it to 0 (the 0..1 anomaly)
      //   - For the rest, set the label as the bottom value instead of the upper.
      //   --> so we'll end with the following (non dummy) labels: 0, 2, 4, 8, 16, ...
      if (!aIsBHR) {
        return k;
      }
      return k == 1 ? 0 : (k + 1) / 2;
    }

    const labelledValues = Object.keys(aHgram.values)
                           .filter(label => !aIsBHR || Number(label) != 0) // remove dummy 0 label for BHR
                           .map(k => [labelFunc(Number(k)), aHgram.values[k]]);

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
      let barValue = aOptions.exponential ? this.getLogValue(value) : value;

      // Create a text representation: <right-aligned-label> |<bar-of-#><value>  <percentage>
      text += EOL
              + " ".repeat(Math.max(0, labelPadTo - String(label).length)) + label // Right-aligned label
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

      // Add bucket label
      barDiv.appendChild(document.createTextNode(label));

      aDiv.appendChild(barDiv);
    }

    return text.substr(EOL.length); // Trim the EOL before the first line
  },
};


var Search = {

  // Pass if: all non-empty array items match (case-sensitive)
  isPassText(subject, filter) {
    for (let item of filter) {
      if (item.length && subject.indexOf(item) < 0) {
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
    return {filter, isPassFunc}
  },

  filterElements(elements, filterText) {
    let [isPassFunc, filter] = this.chooseFilter(filterText);

    let needLowerCase = (isPassFunc === this.isPassText);
    for (let element of elements) {
      let subject = needLowerCase ? element.id.toLowerCase() : element.id;
      element.hidden = !isPassFunc(subject, filter);
    }
  },

  filterKeyedElements(keyedElements, filterText) {
    let res = this.chooseFilter(filterText);
    let isPassFunc = res.isPassFunc;
    let filter = res.filter;

    let needLowerCase = (isPassFunc === this.isPassText);
    keyedElements.forEach((keyedElement) => {
      let subject = needLowerCase ? keyedElement.key.id.toLowerCase() : keyedElement.key.id;
      if (!isPassFunc(subject, filter)) { // If the keyedHistogram's name is not matched
        let allElementHidden = true;
        for (let element of keyedElement.datas) {
          let subject = needLowerCase ? element.id.toLowerCase() : element.id;
          let match = isPassFunc(subject, filter);
          element.hidden = !match;
          if (match) {
            allElementHidden = false;
          }
        }
        keyedElement.key.hidden = allElementHidden;
      } else { // If the keyedHistogram's name is matched
        keyedElement.key.hidden = false;
        for (let element of keyedElement.datas) {
          element.hidden = false;
        }
      }
    });
  },

  searchHandler(e) {
    if (this.idleTimeout) {
      clearTimeout(this.idleTimeout);
    }
    this.idleTimeout = setTimeout(() => Search.search(e.target.value), FILTER_IDLE_TIMEOUT);
  },

  search(text) {
    let selectedSection = document.querySelector(".data-section.active");
    if (selectedSection.id === "histograms-section") {
      let histograms = selectedSection.getElementsByClassName("histogram");
      this.filterElements(histograms, text);
    } else if (selectedSection.id === "keyed-histograms-section") {
      let keyedElements = [];
      let keyedHistograms = selectedSection.getElementsByClassName("keyed-histogram");
      for (let key of keyedHistograms) {
        let datas = key.getElementsByClassName("histogram");
        keyedElements.push({key, datas});
      }
      this.filterKeyedElements(keyedElements, text);
    } else if (selectedSection.id === "keyed-scalars-section") {
      let keyedElements = [];
      let keyedScalars = selectedSection.getElementsByClassName("keyed-scalar");
      for (let key of keyedScalars) {
        let datas = key.querySelector("table").rows;
        keyedElements.push({key, datas});
      }
      this.filterKeyedElements(keyedElements, text);
    } else {
      let tables = selectedSection.querySelectorAll("table");
      for (let table of tables) {
        this.filterElements(table.rows, text);
      }
    }
  },
}

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
    subCategory.appendChild(document.createTextNode(title))
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
    let section = document.createElement("section");
    section.setAttribute("id", sectionID + "-" + title);
    section.classList.add("data-subsection", "expanded");
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
    })
  },

}

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
    divTitle.className = "keyed-histogram-title";
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
      let providerSection = document.createElement("h2");
      let titleText = bundle.formatStringFromName("addonProvider", [provider], 1);
      providerSection.appendChild(document.createTextNode(titleText));
      addonSection.appendChild(providerSection);

      let headingStrings = [this.tableIDTitle, this.tableDetailsTitle ]
      let table = GenericTable.render(explodeObject(addonDetails[provider]),
                                      headingStrings);
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

    let scalars = aPayload.processes[selectedProcess].scalars;
    const hasData = scalars && Object.keys(scalars).length > 0;
    setHasData("scalars-section", hasData || processesSelect.options.length);
    if (!hasData) {
      return;
    }

    const headings = [
      "namesHeader",
      "valuesHeader",
    ].map(h => bundle.GetStringFromName(h));
    const table = GenericTable.render(explodeObject(scalars), headings);
    scalarsSection.appendChild(table);
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

    let keyedScalars = aPayload.processes[selectedProcess].keyedScalars;
    const hasData = keyedScalars && Object.keys(keyedScalars).length > 0;
    setHasData("keyed-scalars-section", hasData || processesSelect.options.length);
    if (!hasData) {
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
      let scalarNameSection = document.createElement("h2");
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

    if (!aPayload.processes || !aPayload.processes.parent) {
      return;
    }

    let processesSelect = document.getElementById("processes");
    let selectedProcess = processesSelect.selectedOptions.item(0).getAttribute("value");

    if (!aPayload.processes ||
        !selectedProcess ||
        !(selectedProcess in aPayload.processes)) {
      return;
    }

    let events = aPayload.processes[selectedProcess].events;
    const hasData = events && Object.keys(events).length > 0;
    setHasData("events-section", hasData);
    if (!hasData) {
      return;
    }

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
 * Helper function that expands and collapses sections +
 * changes caption on the toggle text
 */
function toggleSection(aEvent) {
  let parentElement = aEvent.target.parentElement;
  if (!parentElement.classList.contains("has-data") &&
      !parentElement.classList.contains("has-subdata")) {
    return; // nothing to toggle
  }

  parentElement.classList.toggle("expanded");

  // Store section opened/closed state in a hidden checkbox (which is then used on reload)
  let statebox = parentElement.getElementsByClassName("statebox")[0];
  if (statebox) {
    statebox.checked = parentElement.classList.contains("expanded");
  }
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

function displaySearch(selectedSection) {
  let blacklist = [
    "home",
  ];
  // TODO: Implement global search for the Home section
  let search = document.getElementById("search");
  search.hidden = blacklist.includes(selectedSection);
}

/**
 * Change the section displayed
 */
function show(selected) {
  let current_button = document.querySelector(".category.selected");
  current_button.classList.remove("selected");
  selected.classList.add("selected");
  // Hack because subsection text appear selected. See Bug 1375114.
  document.getSelection().empty();

  let selectedValue = selected.getAttribute("value");
  let current_section = document.querySelector(".active");
  let selected_section = document.getElementById(selectedValue);
  if (current_section == selected_section)
    return;
  current_section.classList.remove("active");
  current_section.hidden = true;
  selected_section.classList.add("active");
  selected_section.hidden = false;

  let title = selected.querySelector(".category-name").textContent.trim();
  document.getElementById("sectionTitle").textContent = title;

  let search = document.getElementById("search");
  let placeholder = bundle.formatStringFromName("filterPlaceholder", [ title ], 1);
  search.setAttribute("placeholder", placeholder);
  displayProcessesSelector(selectedValue);
  displaySearch(selectedValue);
}

function showSubSection(selected) {
  let current_selection = document.querySelector(".category-subsection.selected");
  if (current_selection)
    current_selection.classList.remove("selected");
  selected.classList.add("selected");

  let section = document.getElementById(selected.getAttribute("value"));
  section.parentElement.childNodes.forEach((element) => {
    element.classList.remove("expanded");
  }, this);
  section.classList.add("expanded");

  let title = selected.parentElement.querySelector(".category-name").textContent;
  document.getElementById("sectionTitle").textContent = title + " - " + selected.textContent;
  document.getSelection().empty(); // prevent subsection text selection
}

/**
 * Initializes load/unload, pref change and mouse-click listeners
 */
function setupListeners() {
  Settings.attachObservers();
  PingPicker.attachObservers();

  let menu = document.getElementById("categories");
  menu.addEventListener("click", (e) => {
    if (e.target && e.target.parentNode == menu) {
      show(e.target)
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

      ChromeHangs.render(gPingData);
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

  // Clicking on the section name will toggle its state
  let sectionHeaders = document.getElementsByClassName("section-name");
  for (let sectionHeader of sectionHeaders) {
    sectionHeader.addEventListener("click", toggleSection);
  }

  // Clicking on the "toggle" text will also toggle section's state
  let toggleLinks = document.getElementsByClassName("toggle-caption");
  for (let toggleLink of toggleLinks) {
    toggleLink.addEventListener("click", toggleSection);
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

  // Restore sections states
  let stateboxes = document.getElementsByClassName("statebox");
  for (let box of stateboxes) {
    if (box.checked) { // Was open. Will still display as empty if not has-data
        box.parentElement.classList.add("expanded");
    }
  }

  // Update ping data when async Telemetry init is finished.
  Telemetry.asyncFetchTelemetryData(() => PingPicker.update());
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

    let histograms = aPayload.histograms;

    let hgramsSelect = document.getElementById("processes");
    let hgramsOption = hgramsSelect.selectedOptions.item(0);
    let hgramsProcess = hgramsOption.getAttribute("value");
    // "parent" histograms/keyedHistograms aren't under "parent". Fix that up.
    if (hgramsProcess === "parent") {
      hgramsProcess = "";
    }
    if (hgramsProcess &&
        "processes" in aPayload &&
        hgramsProcess in aPayload.processes) {
      histograms = aPayload.processes[hgramsProcess].histograms;
    }

    let hasData = Object.keys(histograms).length > 0;
    setHasData("histograms-section", hasData || hgramsSelect.options.length);

    if (hasData) {
      for (let [name, hgram] of Object.entries(histograms)) {
        Histogram.render(hgramDiv, name, hgram, {unpacked: true});
      }

      setHasData("histograms-section", true);
    }
  },
}

var KeyedHistogramSection = {
  render(aPayload) {
    let keyedDiv = document.getElementById("keyed-histograms");
    removeAllChildNodes(keyedDiv);

    let keyedHistograms = aPayload.keyedHistograms;

    let keyedHgramsSelect = document.getElementById("processes");
    let keyedHgramsOption = keyedHgramsSelect.selectedOptions.item(0);
    let keyedHgramsProcess = keyedHgramsOption.getAttribute("value");
    // "parent" histograms/keyedHistograms aren't under "parent". Fix that up.
    if (keyedHgramsProcess === "parent") {
      keyedHgramsProcess = "";
    }
    if (keyedHgramsProcess &&
        "processes" in aPayload &&
        keyedHgramsProcess in aPayload.processes) {
      keyedHistograms = aPayload.processes[keyedHgramsProcess].keyedHistograms;
    }

    setHasData("keyed-histograms-section", keyedHgramsSelect.options.length);
    if (keyedHistograms) {
      let hasData = false;
      for (let [id, keyed] of Object.entries(keyedHistograms)) {
        if (Object.keys(keyed).length > 0) {
          hasData = true;
          KeyedHistogram.render(keyedDiv, id, keyed, {unpacked: true});
        }
      }
      setHasData("keyed-histograms-section", hasData || keyedHgramsSelect.options.length);
    }
  },
}

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
}

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
    delete startupEvents["process"];

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
}

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

function renderPayloadList(ping) {
  // Rebuild the payload select with options:
  //   Parent Payload (selected)
  //   Child Payload 1..ping.payload.childPayloads.length
  let listEl = document.getElementById("choose-payload");
  removeAllChildNodes(listEl);

  let option = document.createElement("option");
  let text = bundle.GetStringFromName("parentPayload");
  let content = document.createTextNode(text);
  let payloadIndex = 0;
  option.appendChild(content);
  option.setAttribute("value", payloadIndex++);
  option.selected = true;
  listEl.appendChild(option);

  if (!ping.payload.childPayloads) {
    listEl.disabled = true;
    return
  }
  listEl.disabled = false;

  for (; payloadIndex <= ping.payload.childPayloads.length; ++payloadIndex) {
    option = document.createElement("option");
    text = bundle.formatStringFromName("childPayloadN", [payloadIndex], 1);
    content = document.createTextNode(text);
    option.appendChild(content);
    option.setAttribute("value", payloadIndex);
    listEl.appendChild(option);
  }
}

function togglePingSections(isMainPing) {
  // We always show the sections that are "common" to all pings.
  let commonSections = new Set(["heading",
                                "home",
                                "general-data-section",
                                "environment-data-section",
                                "raw-ping-data-section"]);

  let elements = document.querySelectorAll(".category");
  for (let section of elements) {
    if (commonSections.has(section.getAttribute("value"))) {
      continue;
    }
    section.classList.toggle("has-data", isMainPing);
  }
}

function displayPingData(ping, updatePayloadList = false) {
  gPingData = ping;
  // Render raw ping data.
  RawPayload.render(ping);

  try {
    PingPicker.render();
    displayRichPingData(ping, updatePayloadList);
  } catch (err) {
    console.log(err);
    PingPicker._showRawPingData();
  }
}

function displayRichPingData(ping, updatePayloadList) {
  // Update the payload list and process lists
  if (updatePayloadList) {
    renderPayloadList(ping);
    renderProcessList(ping, document.getElementById("processes"));
  }

  // Show general data.
  GeneralData.render(ping);

  // Show environment data.
  EnvironmentData.render(ping);

  // We only have special rendering code for the payloads from "main" pings.
  // For any other pings we just render the raw JSON payload.
  let isMainPing = (ping.type == "main" || ping.type == "saved-session");
  togglePingSections(isMainPing);

  if (!isMainPing) {
    return;
  }

  // Show telemetry log.
  TelLog.render(ping);

  // Show slow SQL stats
  SlowSQL.render(ping);

  // Show chrome hang stacks
  ChromeHangs.render(ping);

  // Render Addon details.
  AddonDetails.render(ping);

  // Select payload to render
  let payloadSelect = document.getElementById("choose-payload");
  let payloadOption = payloadSelect.selectedOptions.item(0);
  let payloadIndex = payloadOption.getAttribute("value");

  let payload = ping.payload;
  if (payloadIndex > 0) {
    payload = ping.payload.childPayloads[payloadIndex - 1];
  }

  // Show thread hang stats
  ThreadHangStats.render(payload);

  // Show captured stacks.
  CapturedStacks.render(payload);

  // Show simple measurements
  SimpleMeasurements.render(payload);

  LateWritesSingleton.renderLateWrites(payload.lateWrites);

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
}

window.addEventListener("load", onLoad);
