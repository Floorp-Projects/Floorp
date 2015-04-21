/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-*/
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});
const { AddonWatcher } = Cu.import("resource://gre/modules/AddonWatcher.jsm", {});
const { PerformanceStats } = Cu.import("resource://gre/modules/PerformanceStats.jsm", {});

/**
 * The various measures we display.
 */
const MEASURES = [
  {key: "longestDuration", percentOfDeltaT: false, label: "Jank level"},
  {key: "totalUserTime", percentOfDeltaT: true, label: "User (%)"},
  {key: "totalSystemTime", percentOfDeltaT: true, label: "System (%)"},
  {key: "totalCPOWTime", percentOfDeltaT: true, label: "Cross-Process (%)"},
  {key: "ticks", percentOfDeltaT: false, label: "Activations"},
];

let State = {
  /**
   * @type{PerformanceData}
   */
  _processData: null,
  /**
   * A mapping from name to PerformanceData
   *
   * @type{Map}
   */
  _componentsData: new Map(),

  /**
   * A number of milliseconds since the high-performance epoch.
   */
  _date: window.performance.now(),

  /**
   * Fetch the latest information, compute diffs.
   *
   * @return {object} An object with the following fields:
   * - `components`: an array of `PerformanceDiff` representing
   *   the components, sorted by `longestDuration`, then by `totalUserTime`
   * - `process`: a `PerformanceDiff` representing the entire process;
   * - `deltaT`: the number of milliseconds elapsed since the data
   *   was last displayed.
   */
  update: function() {
    let snapshot = PerformanceStats.getSnapshot();
    let newData = new Map();
    let deltas = [];
    for (let componentNew of snapshot.componentsData) {
      let {name, addonId, isSystem} = componentNew;
      let key = JSON.stringify({name, addonId, isSystem});
      let componentOld = State._componentsData.get(key);
      deltas.push(componentNew.substract(componentOld));
      newData.set(key, componentNew);
    }
    State._componentsData = newData;
    let now = window.performance.now();
    let result = {
      components: deltas.filter(x => x.ticks > 0),
      process: snapshot.processData.substract(State._processData),
      deltaT: now - State._date
    };
    result.components.sort((a, b) => {
      if (a.longestDuration < b.longestDuration) {
        return true;
      }
      if (a.longestDuration == b.longestDuration) {
        return a.totalUserTime <= b.totalUserTime
      }
      return false;
    });
    State._processData = snapshot.processData;
    State._date = now;
    return result;
  }
};


function update() {
  updateLiveData();
  updateSlowAddons();
}

/**
 * Update the list of slow addons
 */
function updateSlowAddons() {
  try {
    let data = AddonWatcher.alerts;
    if (data.size == 0) {
      // Nothing to display.
      return;
    }
    let alerts = 0;
    for (let [addonId, details] of data) {
      for (let k of Object.keys(details.alerts)) {
        alerts += details.alerts[k];
      }
    }

    if (!alerts) {
      // Still nothing to display.
      return;
    }


    let elData = document.getElementById("slowAddonsList");
    elData.innerHTML = "";
    let elTable = document.createElement("table");
    elData.appendChild(elTable);

    // Generate header
    let elHeader = document.createElement("tr");
    elTable.appendChild(elHeader);
    for (let name of [
      "Alerts",
      "Jank level alerts",
      "(highest jank)",
      "Cross-Process alerts",
      "(highest CPOW)"
    ]) {
      let elName = document.createElement("td");
      elName.textContent = name;
      elHeader.appendChild(elName);
      elName.classList.add("header");
    }
    for (let [addonId, details] of data) {
      let elAddon = document.createElement("tr");

      // Display the number of occurrences of each alerts
      let elTotal = document.createElement("td");
      let total = 0;
      for (let k of Object.keys(details.alerts)) {
        total += details.alerts[k];
      }
      elTotal.textContent = total;
      elAddon.appendChild(elTotal);

      for (let filter of ["longestDuration", "totalCPOWTime"]) {
        for (let stat of ["alerts", "peaks"]) {
          let el = document.createElement("td");
          el.textContent = details[stat][filter] || 0;
          elAddon.appendChild(el);
        }
      }

      // Display the name of the add-on
      let elName = document.createElement("td");
      elAddon.appendChild(elName);
      AddonManager.getAddonByID(addonId, a => {
        elName.textContent = a ? a.name : addonId
      });

      elTable.appendChild(elAddon);
    }
  } catch (ex) {
    console.error(ex);
  }
}

/**
 * Update the table of live data.
 */
function updateLiveData() {
  try {
    let dataElt = document.getElementById("liveData");
    dataElt.innerHTML = "";

    // Generate table headers
    let headerElt = document.createElement("tr");
    dataElt.appendChild(headerElt);
    headerElt.classList.add("header");
    for (let column of [...MEASURES, {key: "name", name: ""}]) {
      let el = document.createElement("td");
      el.classList.add(column.key);
      el.textContent = column.label;
      headerElt.appendChild(el);
    }

    let deltas = State.update();

    for (let item of deltas.components) {
      let row = document.createElement("tr");
      if (item.addonId) {
        row.classList.add("addon");
      } else if (item.isSystem) {
        row.classList.add("platform");
      } else {
        row.classList.add("content");
      }
      dataElt.appendChild(row);

      // Measures
      for (let {key, percentOfDeltaT} of MEASURES) {
        let el = document.createElement("td");
        el.classList.add(key);
        el.classList.add("contents");
        row.appendChild(el);

        let value = percentOfDeltaT ? Math.round(item[key] / deltas.deltaT) : item[key];
        if (key == "longestDuration") {
          value += 1;
          el.classList.add("jank" + value);
        }
        el.textContent = value;
      }

      // Name
      let el = document.createElement("td");
      let id = item.id;
      el.classList.add("contents");
      el.classList.add("name");
      row.appendChild(el);
      if (item.addonId) {
        let _el = el;
        let _item = item;
        AddonManager.getAddonByID(item.addonId, a => {
          _el.textContent = a ? a.name : _item.name
        });
      } else {
        el.textContent = item.name;
      }
    }
  } catch (ex) {
    console.error(ex);
  }
}

function go() {
  // Compute initial state immediately, then wait a little
  // before we start computing diffs and refreshing.
  State.update();

  window.setTimeout(() => {
    window.setInterval(update, 10000);
  }, 1000);
}
