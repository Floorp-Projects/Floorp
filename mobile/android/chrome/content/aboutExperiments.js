/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AndroidLog: "resource://gre/modules/AndroidLog.jsm",
  EventDispatcher: "resource://gre/modules/Messaging.jsm",
});

const LOGTAG = "Experiments";
const EXPERIMENTS_CONFIGURATION = "https://firefox.settings.services.mozilla.com/v1/buckets/fennec/collections/experiments/records";
const Experiments = Services.wm.getMostRecentWindow("navigator:browser").Experiments;

document.addEventListener("DOMContentLoaded", initList);

function log(msg) {
  AndroidLog.d(LOGTAG, msg);
}

function initList() {
  const list = document.getElementById("list");
  list.addEventListener("click", toggleOverride);

  Promise.all([promiseEnabledExperiments(), promiseExperimentsConfiguration()]).then(values => {
    const enabledExperiments = values[0];
    const serverConfiguration = values[1];

    serverConfiguration.data.forEach(function(experiment) {
      try {
        let item = document.createElement("li");
        item.textContent = experiment.name;
        item.setAttribute("name", experiment.name);
        item.setAttribute("isEnabled", enabledExperiments.includes(experiment.name));
        list.appendChild(item);
      } catch (e) {
          log(`Error while setting experiments list: ${e.error}`);
      }
    });
  });
}

function toggleOverride(experiment) {
  const item = experiment.originalTarget;
  const name = item.getAttribute("name");
  const isEnabled = item.getAttribute("isEnabled") === "true";

  log(`toggleOverride: ${name}`);

  Experiments.setOverride(name, !isEnabled);
  item.setAttribute("isEnabled", !isEnabled);
}

/**
 * Get the list of locally enabled experiments.
 */
function promiseEnabledExperiments() {
  log("Getting the locally enabled experiments");

  return EventDispatcher.instance.sendRequestForResult({
    type: "Experiments:GetActive"
  }).then(experiments => {
    log("List of locally enabled experiments ready");
    return experiments;
  });
}

/**
 * Fetch the list of experiments from server configuration.
 */
function promiseExperimentsConfiguration() {
  log("Fetching server experiments");

  return new Promise((resolve, reject) => {
    const xhr = new XMLHttpRequest();

    try {
      xhr.open("GET", EXPERIMENTS_CONFIGURATION, true);
    } catch (e) {
      reject(`Error opening request: ${e}`);
      return;
    }

    xhr.onerror = function(e) {
      reject(`Error making request: ${e.error}`);
    };

    xhr.onload = function(event) {
      if (xhr.readyState === 4) {
        if (xhr.status === 200) {
          try {
            resolve(JSON.parse(xhr.responseText));
          } catch (e) {
            const errorMessage = `Error while parsing request: ${e}`;
            log(errorMessage);
            reject(errorMessage);
          }
        } else {
          const errorMessage = `Request to ${url} returned status ${xhr.status}`;
          log(errorMessage);
          reject(errorMessage);
        }
      }
      log("Finished fetching server experiments");
    };

    xhr.send(null);
  });
}
