/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global WebrtcGlobalInformation, document */

var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "FilePicker",
                                   "@mozilla.org/filepicker;1", "nsIFilePicker");
XPCOMUtils.defineLazyGetter(this, "strings", () => {
  return Services.strings.createBundle("chrome://global/locale/aboutWebrtc.properties");
});

const getString = strings.GetStringFromName;
const formatString = strings.formatStringFromName;

const LOGFILE_NAME_DEFAULT = "aboutWebrtc.html";
const WEBRTC_TRACE_ALL = 65535;

function getStats() {
  return new Promise(resolve =>
    WebrtcGlobalInformation.getAllStats(stats => resolve(stats)));
}

function getLog() {
  return new Promise(resolve =>
    WebrtcGlobalInformation.getLogging("", log => resolve(log)));
}

// Begin initial data queries as page loads. Store returned Promises for
// later use.
var reportsRetrieved = getStats();
var logRetrieved = getLog();

function onLoad() {
  document.title = getString("document_title");
  let controls = document.querySelector("#controls");
  if (controls) {
    let set = ControlSet.render();
    ControlSet.add(new SavePage());
    ControlSet.add(new DebugMode());
    ControlSet.add(new AecLogging());
    controls.appendChild(set);
  }

  let contentElem = document.querySelector("#content");
  if (!contentElem) {
    return;
  }

  let contentInit = function(data) {
    AboutWebRTC.init(onClearStats, onClearLog);
    AboutWebRTC.render(contentElem, data);
  };

  Promise.all([reportsRetrieved, logRetrieved])
    .then(([stats, log]) => contentInit({reports: stats.reports, log: log}))
    .catch(error => contentInit({error: error}));
}

function onClearLog() {
  WebrtcGlobalInformation.clearLogging();
  getLog()
    .then(log => AboutWebRTC.refresh({log: log}))
    .catch(error => AboutWebRTC.refresh({logError: error}));
}

function onClearStats() {
  WebrtcGlobalInformation.clearAllStats();
  getStats()
    .then(stats => AboutWebRTC.refresh({reports: stats.reports}))
    .catch(error => AboutWebRTC.refresh({reportError: error}));
}

var ControlSet = {
  render: function() {
    let controls = document.createElement("div");
    let control = document.createElement("div");
    let message = document.createElement("div");

    controls.className = "controls";
    control.className = "control";
    message.className = "message";
    controls.appendChild(control);
    controls.appendChild(message);

    this.controlSection = control;
    this.messageSection = message;
    return controls;
  },

  add: function(controlObj) {
    let [controlElem, messageElem] = controlObj.render();
    this.controlSection.appendChild(controlElem);
    this.messageSection.appendChild(messageElem);
  }
};

function Control() {
  this._label = null;
  this._message = null;
  this._messageHeader = null;
}

Control.prototype = {
  render: function () {
    let controlElem = document.createElement("button");
    let messageElem = document.createElement("p");

    this.ctrl = controlElem;
    controlElem.onclick = this.onClick.bind(this);
    this.msg = messageElem;
    this.update();

    return [controlElem, messageElem];
  },

  set label(val) {
    return this._labelVal = val || "\xA0";
  },

  get label() {
    return this._labelVal;
  },

  set message(val) {
    return this._messageVal = val;
  },

  get message() {
    return this._messageVal;
  },

  update: function() {
    this.ctrl.textContent = this._label;

    if (this._message) {
      this.msg.innerHTML =
        `<span class="info-label">${this._messageHeader}:</span> ${this._message}`;
    } else {
      this.msg.innerHTML = null;
    }
  },

  onClick: function(event) {
    return true;
  }
};

function SavePage() {
  Control.call(this);
  this._messageHeader = getString("save_page_label");
  this._label = getString("save_page_label");
}

SavePage.prototype = Object.create(Control.prototype);
SavePage.prototype.constructor = SavePage;

SavePage.prototype.onClick = function() {
  let content = document.querySelector("#content");

  if (!content)
    return;

  FoldEffect.expandAll();
  FilePicker.init(window, getString("save_page_dialog_title"), FilePicker.modeSave);
  FilePicker.defaultString = LOGFILE_NAME_DEFAULT;
  let rv = FilePicker.show();

  if (rv == FilePicker.returnOK || rv == FilePicker.returnReplace) {
    let fout = FileUtils.openAtomicFileOutputStream(
      FilePicker.file, FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE);

    let nodes = content.querySelectorAll(".no-print");
    let noPrintList = [];
    for (let node of nodes) {
      noPrintList.push(node);
      node.style.setProperty("display", "none");
    }

    fout.write(content.outerHTML, content.outerHTML.length);
    FileUtils.closeAtomicFileOutputStream(fout);

    for (let node of noPrintList) {
      node.style.removeProperty("display");
    }

    this._message = formatString("save_page_msg", [FilePicker.file.path], 1);
    this.update();
  }
};

function DebugMode() {
  Control.call(this);
  this._messageHeader = getString("debug_mode_msg_label");

  if (WebrtcGlobalInformation.debugLevel > 0) {
    this.onState();
  } else {
    this._label = getString("debug_mode_off_state_label");
    this._message = null;
  }
}

DebugMode.prototype = Object.create(Control.prototype);
DebugMode.prototype.constructor = DebugMode;

DebugMode.prototype.onState = function() {
  this._label = getString("debug_mode_on_state_label");
  try {
    let file = Services.prefs.getCharPref("media.webrtc.debug.log_file");
    this._message = formatString("debug_mode_on_state_msg", [file], 1);
  } catch (e) {
    this._message = null;
  }
};

DebugMode.prototype.offState = function() {
  this._label = getString("debug_mode_off_state_label");
  try {
    let file = Services.prefs.getCharPref("media.webrtc.debug.log_file");
    this._message = formatString("debug_mode_off_state_msg", [file], 1);
  } catch (e) {
    this._message = null;
  }
};

DebugMode.prototype.onClick = function() {
  if (WebrtcGlobalInformation.debugLevel > 0) {
    WebrtcGlobalInformation.debugLevel = 0;
    this.offState();
  } else {
    WebrtcGlobalInformation.debugLevel = WEBRTC_TRACE_ALL;
    this.onState();
  }

  this.update();
};

function AecLogging() {
  Control.call(this);
  this._messageHeader = getString("aec_logging_msg_label");

  if (WebrtcGlobalInformation.aecDebug) {
    this.onState();
  } else {
    this._label = getString("aec_logging_off_state_label");
    this._message = null;
  }
}

AecLogging.prototype = Object.create(Control.prototype);
AecLogging.prototype.constructor = AecLogging;

AecLogging.prototype.offState = function () {
  this._label = getString("aec_logging_off_state_label");
  try {
    let file = Services.prefs.getCharPref("media.webrtc.debug.aec_log_dir");
    this._message = formatString("aec_logging_off_state_msg", [file], 1);
  } catch (e) {
    this._message = null;
  }
};

AecLogging.prototype.onState = function () {
  this._label = getString("aec_logging_on_state_label");
  try {
    let file = Services.prefs.getCharPref("media.webrtc.debug.aec_log_dir");
    this._message = getString("aec_logging_on_state_msg");
  } catch (e) {
    this._message = null;
  }
};

AecLogging.prototype.onClick = function () {
  if (WebrtcGlobalInformation.aecDebug) {
    WebrtcGlobalInformation.aecDebug = false;
    this.offState();
  } else {
    WebrtcGlobalInformation.aecDebug = true;
    this.onState();
  }
  this.update();
};

var AboutWebRTC = {
  _reports: [],
  _log: [],

  init: function(onClearStats, onClearLog) {
    this._onClearStats = onClearStats;
    this._onClearLog = onClearLog;
  },

  render: function(parent, data) {
    this._content = parent;
    this._setData(data);

    if (data.error) {
      let msg = document.createElement("h3");
      msg.textContent = getString("cannot_retrieve_log");
      parent.appendChild(msg);
      msg = document.createElement("p");
      msg.innerHTML = `${data.error.name}: ${data.error.message}`;
      parent.appendChild(msg);
      return;
    }

    this._peerConnections = this.renderPeerConnections();
    this._connectionLog = this.renderConnectionLog();
    this._content.appendChild(this._peerConnections);
    this._content.appendChild(this._connectionLog);
  },

  _setData: function(data) {
    if (data.reports) {
      this._reports = data.reports;
    }

    if (data.log) {
      this._log = data.log;
    }
  },

  refresh: function(data) {
    this._setData(data);
    let pc = this._peerConnections;
    this._peerConnections = this.renderPeerConnections();
    let log = this._connectionLog;
    this._connectionLog = this.renderConnectionLog();
    this._content.replaceChild(this._peerConnections, pc);
    this._content.replaceChild(this._connectionLog, log);
  },

  renderPeerConnections: function() {
    let connections = document.createElement("div");
    connections.className = "stats";

    let heading = document.createElement("span");
    heading.className = "section-heading";
    let elem = document.createElement("h3");
    elem.textContent = getString("stats_heading");
    heading.appendChild(elem);

    elem = document.createElement("button");
    elem.textContent = "Clear History";
    elem.className = "no-print";
    elem.onclick = this._onClearStats;
    heading.appendChild(elem);
    connections.appendChild(heading);

    if (!this._reports || !this._reports.length) {
      return connections;
    }

    let reports = [...this._reports];
    reports.sort((a, b) => b.timestamp - a.timestamp);
    for (let report of reports) {
      let peerConnection = new PeerConnection(report);
      connections.appendChild(peerConnection.render());
    }

    return connections;
  },

  renderConnectionLog: function() {
    let content = document.createElement("div");
    content.className = "log";

    let heading = document.createElement("span");
    heading.className = "section-heading";
    let elem = document.createElement("h3");
    elem.textContent = getString("log_heading");
    heading.appendChild(elem);
    elem = document.createElement("button");
    elem.textContent = "Clear Log";
    elem.className = "no-print";
    elem.onclick = this._onClearLog;
    heading.appendChild(elem);
    content.appendChild(heading);

    if (!this._log || !this._log.length) {
      return content;
    }

    let div = document.createElement("div");
    let sectionCtrl = document.createElement("div");
    sectionCtrl.className = "section-ctrl no-print";
    let foldEffect = new FoldEffect(div, {
      showMsg: getString("log_show_msg"),
      hideMsg: getString("log_hide_msg")
    });
    sectionCtrl.appendChild(foldEffect.render());
    content.appendChild(sectionCtrl);

    for (let line of this._log) {
      elem = document.createElement("p");
      elem.textContent = line;
      div.appendChild(elem);
    }

    content.appendChild(div);
    return content;
  }
};

function PeerConnection(report) {
  this._report = report;
}

PeerConnection.prototype = {
  render: function() {
    let pc = document.createElement("div");
    pc.className = "peer-connection";
    pc.appendChild(this.renderHeading());

    let div = document.createElement("div");
    let sectionCtrl = document.createElement("div");
    sectionCtrl.className = "section-ctrl no-print";
    let foldEffect = new FoldEffect(div);
    sectionCtrl.appendChild(foldEffect.render());
    pc.appendChild(sectionCtrl);

    div.appendChild(this.renderDesc());
    div.appendChild(new ICEStats(this._report).render());
    div.appendChild(new SDPStats(this._report).render());
    div.appendChild(new RTPStats(this._report).render());

    pc.appendChild(div);
    return pc;
  },

  renderHeading: function () {
    let pcInfo = this.getPCInfo(this._report);
    let heading = document.createElement("h3");
    let now = new Date(this._report.timestamp).toTimeString();
    heading.textContent =
      `[ ${pcInfo.id} ] ${pcInfo.url} ${pcInfo.closed ? `(${getString("connection_closed")})` : ""} ${now}`;
    return heading;
  },

  renderDesc: function() {
    let info = document.createElement("div");
    let label = document.createElement("span");
    let body = document.createElement("span");

    label.className = "info-label";
    label.textContent = `${getString("peer_connection_id_label")}: `;
    info.appendChild(label);

    body.className = "info-body";
    body.textContent = this._report.pcid;
    info.appendChild(body);

    return info;
  },

  getPCInfo: function(report) {
    return {
      id: report.pcid.match(/id=(\S+)/)[1],
      url: report.pcid.match(/url=([^)]+)/)[1],
      closed: report.closed
    };
  }
};

function SDPStats(report) {
  this._report = report;
}

SDPStats.prototype = {
  render: function() {
    let div = document.createElement("div");
    let elem = document.createElement("h4");

    elem.textContent = getString("sdp_heading");
    div.appendChild(elem);

    elem = document.createElement("h5");
    elem.textContent = getString("local_sdp_heading");
    div.appendChild(elem);

    elem = document.createElement("pre");
    elem.textContent = this._report.localSdp;
    div.appendChild(elem);

    elem = document.createElement("h5");
    elem.textContent = getString("remote_sdp_heading");
    div.appendChild(elem);

    elem = document.createElement("pre");
    elem.textContent = this._report.remoteSdp;
    div.appendChild(elem);

    return div;
  }
};

function RTPStats(report) {
  this._report = report;
  this._stats = [];
}

RTPStats.prototype = {
  render: function() {
    let div = document.createElement("div");
    let heading = document.createElement("h4");

    heading.textContent = getString("rtp_stats_heading");
    div.appendChild(heading);

    this.generateRTPStats();

    for (let statSet of this._stats) {
      div.appendChild(this.renderRTPStatSet(statSet));
    }

    return div;
  },

  generateRTPStats: function() {
    let remoteRtpStats = {};
    let rtpStats = [].concat((this._report.inboundRTPStreamStats  || []),
                             (this._report.outboundRTPStreamStats || []));

    // Generate an id-to-streamStat index for each streamStat that is marked
    // as a remote. This will be used next to link the remote to its local side.
    for (let stats of rtpStats) {
      if (stats.isRemote) {
        remoteRtpStats[stats.id] = stats;
      }
    }

    // If a streamStat has a remoteId attribute, create a remoteRtpStats
    // attribute that references the remote streamStat entry directly.
    // That is, the index generated above is merged into the returned list.
    for (let stats of rtpStats) {
      if (stats.remoteId) {
        stats.remoteRtpStats = remoteRtpStats[stats.remoteId];
      }
    }

    this._stats = rtpStats;
  },

  renderAvStats: function(stats) {
    let statsString = "";

    if (stats.mozAvSyncDelay) {
      statsString += `${getString("av_sync_label")}: ${stats.mozAvSyncDelay} ms `;
    }
    if (stats.mozJitterBufferDelay) {
      statsString += `${getString("jitter_buffer_delay_label")}: ${stats.mozJitterBufferDelay} ms`;
    }

    let line = document.createElement("p");
    line.textContent = statsString;
    return line;
  },

  renderCoderStats: function(stats) {
    let statsString = "";
    let label;

    if (stats.bitrateMean) {
      statsString += ` ${getString("avg_bitrate_label")}: ${(stats.bitrateMean / 1000000).toFixed(2)} Mbps`;
      if (stats.bitrateStdDev) {
        statsString += ` (${(stats.bitrateStdDev / 1000000).toFixed(2)} SD)`;
      }
    }

    if (stats.framerateMean) {
      statsString += ` ${getString("avg_framerate_label")}: ${(stats.framerateMean).toFixed(2)} fps`;
      if (stats.framerateStdDev) {
        statsString += ` (${stats.framerateStdDev.toFixed(2)} SD)`;
      }
    }

    if (stats.droppedFrames) {
      statsString += ` ${getString("dropped_frames_label")}: ${stats.droppedFrames}`;
    }
    if (stats.discardedPackets) {
      statsString += ` ${getString("discarded_packets_label")}: ${stats.discardedPackets}`;
    }

    if (statsString) {
      label = (stats.packetsReceived ? ` ${getString("decoder_label")}:` : ` ${getString("encoder_label")}:`);
      statsString = label + statsString;
    }

    let line = document.createElement("p");
    line.textContent = statsString;
    return line;
  },

  renderTransportStats: function(stats, typeLabel) {
    let time  = new Date(stats.timestamp).toTimeString();
    let statsString = `${typeLabel}: ${time} ${stats.type} SSRC: ${stats.ssrc}`;

    if (stats.packetsReceived) {
      statsString += ` ${getString("received_label")}: ${stats.packetsReceived} ${getString("packets")}`;

      if (stats.bytesReceived) {
        statsString += ` (${(stats.bytesReceived / 1024).toFixed(2)} Kb)`;
      }

      statsString += ` ${getString("lost_label")}: ${stats.packetsLost} ${getString("jitter_label")}: ${stats.jitter}`;

      if (stats.mozRtt) {
        statsString += ` RTT: ${stats.mozRtt} ms`;
      }
    } else if (stats.packetsSent) {
      statsString += ` ${getString("sent_label")}: ${stats.packetsSent} ${getString("packets")}`;
      if (stats.bytesSent) {
        statsString += ` (${(stats.bytesSent / 1024).toFixed(2)} Kb)`;
      }
    }

    let line = document.createElement("p");
    line.textContent = statsString;
    return line;
  },

  renderRTPStatSet: function(stats) {
    let div = document.createElement("div");
    let heading = document.createElement("h5");

    heading.textContent = stats.id;
    div.appendChild(heading);

    if (stats.MozAvSyncDelay || stats.mozJitterBufferDelay) {
      div.appendChild(this.renderAvStats(stats));
    }

    div.appendChild(this.renderCoderStats(stats));
    div.appendChild(this.renderTransportStats(stats, getString("typeLocal")));

    if (stats.remoteId && stats.remoteRtpStats) {
      div.appendChild(this.renderTransportStats(stats.remoteRtpStats, getString("typeRemote")));
    }

    return div;
  },
};

function ICEStats(report) {
  this._report = report;
}

ICEStats.prototype = {
  render: function() {
    let tbody = [];
    for (let stat of this.generateICEStats()) {
      tbody.push([
        stat.localcandidate || "",
        stat.remotecandidate || "",
        stat.state || "",
        stat.priority || "",
        stat.nominated || "",
        stat.selected || ""
      ]);
    }

    let statsTable = new SimpleTable(
      [getString("local_candidate"), getString("remote_candidate"), getString("ice_state"),
       getString("priority"), getString("nominated"), getString("selected")],
      tbody);

    let div = document.createElement("div");
    let heading = document.createElement("h4");

    heading.textContent = getString("ice_stats_heading");
    div.appendChild(heading);
    div.appendChild(statsTable.render());

    return div;
  },

  generateICEStats: function() {
    // Create an index based on candidate ID for each element in the
    // iceCandidateStats array.
    let candidates = new Map();

    for (let candidate of this._report.iceCandidateStats) {
      candidates.set(candidate.id, candidate);
    }

    // A component may have a remote or local candidate address or both.
    // Combine those with both; these will be the peer candidates.
    let matched = {};
    let stats = [];
    let stat;

    for (let pair of this._report.iceCandidatePairStats) {
      let local = candidates.get(pair.localCandidateId);
      let remote = candidates.get(pair.remoteCandidateId);

      if (local) {
        stat = {
          localcandidate: this.candidateToString(local),
          state: pair.state,
          priority: pair.priority,
          nominated: pair.nominated,
          selected: pair.selected
        };
        matched[local.id] = true;

        if (remote) {
          stat.remotecandidate = this.candidateToString(remote);
          matched[remote.id] = true;
        }
        stats.push(stat);
      }
    }

    for (let c of candidates.values()) {
      if (matched[c.id])
        continue;

      stat = {};
      stat[c.type] = this.candidateToString(c);
      stats.push(stat);
    }

    return stats.sort((a, b) => (b.priority || 0) - (a.priority || 0));
  },

  candidateToString: function(c) {
    if (!c) {
      return "*";
    }

    var type = c.candidateType;

    if (c.type == "localcandidate" && c.candidateType == "relayed") {
      type = `${c.candidateType}-${c.mozLocalTransport}`;
    }

    return `${c.ipAddress}:${c.portNumber}/${c.transport}(${type})`;
  }
};

function SimpleTable(heading, data) {
  this._heading = heading || [];
  this._data = data;
}

SimpleTable.prototype = {
  renderRow: function(list) {
    let row = document.createElement("tr");

    for (let elem of list) {
      let cell = document.createElement("td");
      cell.textContent = elem;
      row.appendChild(cell);
    }

    return row;
  },

  render: function() {
    let table = document.createElement("table");

    if (this._heading) {
      table.appendChild(this.renderRow(this._heading));
    }

    for (let row of this._data) {
      table.appendChild(this.renderRow(row));
    }

    return table;
  }
};

function FoldEffect(targetElem, options = {}) {
  if (targetElem) {
    this._showMsg = "\u25BC " + (options.showMsg || getString("fold_show_msg"));
    this._showHint = options.showHint || getString("fold_show_hint");
    this._hideMsg = "\u25B2 " + (options.hideMsg || getString("fold_hide_msg"));
    this._hideHint = options.hideHint || getString("fold_hide_hint");
    this._target = targetElem;
  }
}

FoldEffect.prototype = {
  render: function() {
    this._target.classList.add("fold-target");

    let ctrl = document.createElement("div");
    this._trigger = ctrl;
    ctrl.className = "fold-trigger";
    ctrl.addEventListener("click", this.onClick.bind(this));
    this.close();

    FoldEffect._sections.push(this);
    return ctrl;
  },

  onClick: function() {
    if (this._target.classList.contains("fold-closed")) {
      this.open();
    } else {
      this.close();
    }
    return true;
  },

  open: function() {
    this._target.classList.remove("fold-closed");
    this._trigger.setAttribute("title", this._hideHint);
    this._trigger.textContent = this._hideMsg;
  },

  close: function() {
    this._target.classList.add("fold-closed");
    this._trigger.setAttribute("title", this._showHint);
    this._trigger.textContent = this._showMsg;
  }
};

FoldEffect._sections = [];

FoldEffect.expandAll = function() {
  for (let section of this._sections) {
    section.open();
  }
};

FoldEffect.collapseAll = function() {
  for (let section of this._sections) {
    section.close();
  }
};
