/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

let activeRecording;
let recordings;

function recordNewTab() {
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  const id = createRecordingId();
  const startTime = formatTime();

  gBrowser.selectedTab = gBrowser.addWebTab("about:blank", {
    recordExecution: "*",
  });

  activeRecording = { tab: gBrowser.selectedTab, id, start: Date.now() };
  recordings[id] = { id, startTime };

  const container = document.querySelector(".container");
  container.classList.add("is-recording");

  Services.telemetry.scalarAdd("devtools.webreplay.new_recording", 1);
}

function openRecording({ id }) {
  const { gBrowser } = Services.wm.getMostRecentWindow("navigator:browser");
  const dir = replayDir();
  dir.append(id);
  const path = dir.path;

  gBrowser.selectedTab = gBrowser.addWebTab(null, {
    replayExecution: path,
  });

  Services.telemetry.scalarAdd("devtools.webreplay.load_recording", 1);
}

function createRecordingId() {
  const d = new Date();
  return `${d.getFullYear()}-${d.getMonth() +
    1}-${d.getDate()}-${d.getHours()}-${d.getMinutes()}-${d.getSeconds()}.rec`;
}

function formatTime() {
  return new Intl.DateTimeFormat("default", {
    month: "short",
    day: "numeric",
    hour: "numeric",
    minute: "numeric",
  }).format(new Date());
}

function formatDuration(time) {
  if (time < 60) {
    return time + " seconds";
  }

  if (time < 60 * 60) {
    return time + " minutes";
  }

  if (time < 60 * 60 * 30) {
    return time + " days";
  }

  return time + " months";
}

function saveRecording() {
  if (!activeRecording) {
    return;
  }

  const remoteTab = activeRecording.tab.linkedBrowser.frameLoader.remoteTab;
  const url =
    activeRecording.tab.linkedBrowser.frameLoader.ownerElement.documentURI
      .displaySpec;
  const title = activeRecording.tab.label;
  const duration = formatDuration(
    Math.floor((Date.now() - activeRecording.start) / 1000)
  );
  const endTime = formatTime();

  const path = replayDir();
  const id = activeRecording.id;
  path.append(id);
  const recordingPath = path.path;

  const recordingData = {
    ...recordings[id],
    url,
    title,
    endTime,
    duration,
  };
  console.log(recordingData);

  if (!remoteTab || !remoteTab.saveRecording(recordingPath)) {
    window.alert("Current tab is not recording");
  }

  recordings[id] = recordingData;
  OS.File.writeAtomic(getRecordingsPath(), JSON.stringify(recordings));
  showRecordings();

  const container = document.querySelector(".container");
  container.classList.remove("is-recording");
}

function replayDir() {
  let dir = Services.dirsvc.get("UAppData", Ci.nsIFile);
  dir.append("Replay");

  if (!dir.exists()) {
    OS.File.makeDir(dir.path);
  }

  return dir;
}

function getRecordingsPath() {
  const dir = replayDir();
  dir.append("recordings.json");
  return dir.path;
}

async function readRecordingsFile() {
  if (!(await OS.File.exists(getRecordingsPath()))) {
    return {};
  }
  const file = await OS.File.read(getRecordingsPath());
  const string = new TextDecoder("utf-8").decode(file);
  return JSON.parse(string);
}

async function showRecordings() {
  const container = document.querySelector(".recordings-list");
  const template = document.querySelector("#recording-row");

  container.innerHTML = "";

  for (const id in recordings) {
    if (!id) {
      continue;
    }
    const recording = recordings[id];
    var newRow = document.importNode(template.content, true);
    newRow.querySelector(".recording-title").innerText = recording.title;
    newRow.querySelector(".recording-url").innerText = recording.url;
    newRow.querySelector(".recording-start").innerText = recording.startTime;
    newRow.querySelector(".recording-duration").innerText = recording.duration;
    newRow.querySelector(".recording").addEventListener("click", e => {
      e.preventDefault();
      openRecording(recording);
    });
    container.appendChild(newRow);
  }
}

window.onload = async function() {
  document
    .querySelector("#start-recording")
    .addEventListener("click", () => recordNewTab());
  document
    .querySelector("#save-recording")
    .addEventListener("click", () => saveRecording());

  recordings = await readRecordingsFile();
  showRecordings();
};
