/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

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
    .then(([stats, log]) => contentInit({reports: stats.reports, log}))
    .catch(error => contentInit({error}));
}

function onClearLog() {
  WebrtcGlobalInformation.clearLogging();
  getLog()
    .then(log => AboutWebRTC.refresh({log}))
    .catch(error => AboutWebRTC.refresh({logError: error}));
}

function onClearStats() {
  WebrtcGlobalInformation.clearAllStats();
  getStats()
    .then(stats => AboutWebRTC.refresh({reports: stats.reports}))
    .catch(error => AboutWebRTC.refresh({reportError: error}));
}

var ControlSet = {
  render() {
    let controls = renderElement("div", null, {className: "controls"});
    this.controlSection = renderElement("div", null, {className: "control"});
    this.messageSection = renderElement("div", null, {className: "message"});

    controls.appendChild(this.controlSection);
    controls.appendChild(this.messageSection);

    return controls;
  },

  add(controlObj) {
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
  render() {
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

  update() {
    this.ctrl.textContent = this._label;

    this.msg.textContent = "";
    if (this._message) {
      this.msg.appendChild(Object.assign(document.createElement("span"), {
        className: "info-label",
        textContent: `${this._messageHeader}: `,
      }));
      this.msg.appendChild(document.createTextNode(this._message));
    }
  },

  onClick(event) {
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
  FilePicker.open(rv => {
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
  });
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

AecLogging.prototype.offState = function() {
  this._label = getString("aec_logging_off_state_label");
  try {
    let file = WebrtcGlobalInformation.aecDebugLogDir;
    this._message = formatString("aec_logging_off_state_msg", [file], 1);
  } catch (e) {
    this._message = null;
  }
};

AecLogging.prototype.onState = function() {
  this._label = getString("aec_logging_on_state_label");
  try {
    this._message = getString("aec_logging_on_state_msg");
  } catch (e) {
    this._message = null;
  }
};

AecLogging.prototype.onClick = function() {
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

  init(onClearStats, onClearLog) {
    this._onClearStats = onClearStats;
    this._onClearLog = onClearLog;
  },

  render(parent, data) {
    this._content = parent;
    this._setData(data);

    if (data.error) {
      parent.appendChild(renderElement("h3", getString("cannot_retrieve_log")));
      parent.appendChild(
          renderElement("p", `${data.error.name}: ${data.error.message}`));
      return;
    }

    this._peerConnections = this.renderPeerConnections();
    this._connectionLog = this.renderConnectionLog();
    this._content.appendChild(this._peerConnections);
    this._content.appendChild(this._connectionLog);
  },

  _setData(data) {
    if (data.reports) {
      this._reports = data.reports;
    }

    if (data.log) {
      this._log = data.log;
    }
  },

  refresh(data) {
    this._setData(data);
    let pc = this._peerConnections;
    this._peerConnections = this.renderPeerConnections();
    let log = this._connectionLog;
    this._connectionLog = this.renderConnectionLog();
    this._content.replaceChild(this._peerConnections, pc);
    this._content.replaceChild(this._connectionLog, log);
  },

  renderPeerConnections() {
    let connections = renderElement("div", null, {className: "stats"});

    let heading = renderElement("span", null, {className: "section-heading"});
    heading.appendChild(renderElement("h3", getString("stats_heading")));

    heading.appendChild(renderElement("button", getString("stats_clear"), {
      className: "no-print",
      onclick: this._onClearStats
    }));
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

  renderConnectionLog() {
    let content = renderElement("div", null, {className: "log"});

    let heading = renderElement("span", null, {className: "section-heading"});
    heading.appendChild(renderElement("h3", getString("log_heading")));
    heading.appendChild(renderElement("button", getString("log_clear"), {
      className: "no-print",
      onclick: this._onClearLog
    }));
    content.appendChild(heading);

    if (!this._log || !this._log.length) {
      return content;
    }

    let div = new FoldableSection(content, {
      showMsg: getString("log_show_msg"),
      hideMsg: getString("log_hide_msg")
    }).render();

    for (let line of this._log) {
      div.appendChild(renderElement("p", line));
    }

    content.appendChild(div);
    return content;
  }
};

function PeerConnection(report) {
  this._report = report;
}

PeerConnection.prototype = {
  render() {
    let pc = renderElement("div", null, {className: "peer-connection"});
    pc.appendChild(this.renderHeading());

    let div = new FoldableSection(pc).render();

    div.appendChild(this.renderDesc());
    div.appendChild(new ICEStats(this._report).render());
    div.appendChild(new SDPStats(this._report).render());
    div.appendChild(new RTPStats(this._report).render());

    pc.appendChild(div);
    return pc;
  },

  renderHeading() {
    let pcInfo = this.getPCInfo(this._report);
    let heading = document.createElement("h3");
    let now = new Date(this._report.timestamp).toTimeString();
    heading.textContent =
      `[ ${pcInfo.id} ] ${pcInfo.url} ${pcInfo.closed ? `(${getString("connection_closed")})` : ""} ${now}`;
    return heading;
  },

  renderDesc() {
    let info = document.createElement("div");

    info.appendChild(
        renderElement("span", `${getString("peer_connection_id_label")}: `), {
          className: "info-label"
        });

    info.appendChild(renderElement("span", this._report.pcid, {
      className: "info-body"
    }));

    return info;
  },

  getPCInfo(report) {
    return {
      id: report.pcid.match(/id=(\S+)/)[1],
      url: report.pcid.match(/url=([^)]+)/)[1],
      closed: report.closed
    };
  }
};

function renderElement(elemName, elemText, options = {}) {
  let elem = document.createElement(elemName);
  elem.textContent = elemText || "";
  Object.assign(elem, options);
  return elem;
}

function SDPStats(report) {
  this._report = report;
}

SDPStats.prototype = {
  render() {
    let div = document.createElement("div");
    div.appendChild(renderElement("h4", getString("sdp_heading")));

    let offerLabel = `(${getString("offer")})`;
    let answerLabel = `(${getString("answer")})`;
    let localSdpHeading =
      `${getString("local_sdp_heading")} ${this._report.offerer ? offerLabel : answerLabel}`;
    let remoteSdpHeading =
      `${getString("remote_sdp_heading")} ${this._report.offerer ? answerLabel : offerLabel}`;

    div.appendChild(renderElement("h5", localSdpHeading));
    div.appendChild(renderElement("pre", this._report.localSdp));

    div.appendChild(renderElement("h5", remoteSdpHeading));
    div.appendChild(renderElement("pre", this._report.remoteSdp));

    return div;
  }
};

function RTPStats(report) {
  this._report = report;
  this._stats = [];
}

RTPStats.prototype = {
  render() {
    let div = document.createElement("div");
    div.appendChild(renderElement("h4", getString("rtp_stats_heading")));

    this.generateRTPStats();

    for (let statSet of this._stats) {
      div.appendChild(this.renderRTPStatSet(statSet));
    }

    return div;
  },

  generateRTPStats() {
    let remoteRtpStats = {};
    let rtpStats = [].concat((this._report.inboundRTPStreamStats || []),
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

  renderAvStats(stats) {
    let statsString = "";

    if (stats.mozAvSyncDelay) {
      statsString += `${getString("av_sync_label")}: ${stats.mozAvSyncDelay} ms `;
    }
    if (stats.mozJitterBufferDelay) {
      statsString += `${getString("jitter_buffer_delay_label")}: ${stats.mozJitterBufferDelay} ms`;
    }

    return renderElement("p", statsString);
  },

  renderCoderStats(stats) {
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

    return renderElement("p", statsString);
  },

  renderTransportStats(stats, typeLabel) {
    let time  = new Date(stats.timestamp).toTimeString();
    let statsString = `${typeLabel}: ${time} ${stats.type} SSRC: ${stats.ssrc}`;

    if (stats.packetsReceived) {
      statsString += ` ${getString("received_label")}: ${stats.packetsReceived} ${getString("packets")}`;

      if (stats.bytesReceived) {
        statsString += ` (${(stats.bytesReceived / 1024).toFixed(2)} Kb)`;
      }

      statsString += ` ${getString("lost_label")}: ${stats.packetsLost} ${getString("jitter_label")}: ${stats.jitter}`;

      if (stats.roundTripTime) {
        statsString += ` RTT: ${stats.roundTripTime} ms`;
      }
    } else if (stats.packetsSent) {
      statsString += ` ${getString("sent_label")}: ${stats.packetsSent} ${getString("packets")}`;
      if (stats.bytesSent) {
        statsString += ` (${(stats.bytesSent / 1024).toFixed(2)} Kb)`;
      }
    }

    return renderElement("p", statsString);
  },

  renderRTPStatSet(stats) {
    let div = document.createElement("div");
    div.appendChild(renderElement("h5", stats.id));

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
  render() {
    let div = document.createElement("div");
    div.appendChild(renderElement("h4", getString("ice_stats_heading")));

    div.appendChild(this.renderICECandidateTable());
    // add just a bit of vertical space between the restart/rollback
    // counts and the ICE candidate pair table above.
    div.appendChild(document.createElement("br"));
    div.appendChild(this.renderIceMetric("ice_restart_count_label",
                                         this._report.iceRestarts));
    div.appendChild(this.renderIceMetric("ice_rollback_count_label",
                                         this._report.iceRollbacks));

    div.appendChild(this.renderRawICECandidateSection());

    return div;
  },

  renderICECandidateTable() {
    let caption = renderElement("caption", null, {className: "no-print"});
    caption.appendChild(
        renderElement("span", `${getString("trickle_caption_msg")} `));
    caption.appendChild(
        renderElement("span", getString("trickle_highlight_color_name"), {
          className: "trickled"
        }));

    let stats = this.generateICEStats();
    // don't use |stat.x || ""| here because it hides 0 values
    let tbody = stats.map(stat => [
      stat["local-candidate"],
      stat["remote-candidate"],
      stat.state,
      stat.priority,
      stat.nominated,
      stat.selected,
      stat.bytesSent,
      stat.bytesReceived
    ].map(entry => Object.is(entry, undefined) ? "" : entry));

    let statsTable = new SimpleTable(
      ["local_candidate", "remote_candidate", "ice_state",
       "priority", "nominated", "selected",
       "ice_pair_bytes_sent", "ice_pair_bytes_received"
      ].map(columnName => getString(columnName)),
      tbody, caption).render();

    // after rendering the table, we need to change the class name for each
    // candidate pair's local or remote candidate if it was trickled.
    stats.forEach((stat, index) => {
      // look at statsTable row index + 1 to skip column headers
      let rowIndex = index + 1;
      if (stat["remote-trickled"]) {
        statsTable.rows[rowIndex].cells[1].className = "trickled";
      }
      if (stat["local-trickled"]) {
        statsTable.rows[rowIndex].cells[0].className = "trickled";
      }
    });

    return statsTable;
  },

  renderRawICECandidates() {
    let div = document.createElement("div");

    let tbody = [];
    let rows = this.generateRawICECandidates();
    for (let row of rows) {
      tbody.push([row.local, row.remote]);
    }

    let statsTable = new SimpleTable(
      [getString("raw_local_candidate"), getString("raw_remote_candidate")],
      tbody).render();

    // we want different formatting on the raw stats table (namely, left-align)
    statsTable.className = "raw-candidate";
    div.appendChild(statsTable);

    return div;
  },

  renderRawICECandidateSection() {
    let section = document.createElement("div");
    section.appendChild(
        renderElement("h4", getString("raw_candidates_heading")));

    let div = new FoldableSection(section, {
      showMsg: getString("raw_cand_show_msg"),
      hideMsg: getString("raw_cand_hide_msg")
    }).render();

    div.appendChild(this.renderRawICECandidates());

    section.appendChild(div);

    return section;
  },

  generateRawICECandidates() {
    let rows = [];
    let row;

    let rawLocals = this._report.rawLocalCandidates.sort();
    // add to a Set (to remove duplicates) because some of these come from
    // candidates in use and some come from the raw trickled candidates
    // received that may have been dropped because no stream was found or
    // they were for a component id that was too high.
    let rawRemotes = [...new Set(this._report.rawRemoteCandidates)].sort();
    let rowCount = Math.max(rawLocals.length, rawRemotes.length);
    for (var i = 0; i < rowCount; i++) {
      let rawLocal = rawLocals[i];
      let rawRemote = rawRemotes[i];
      row = {
        local: rawLocal || "",
        remote: rawRemote || ""
      };
      rows.push(row);
    }
    return rows;
  },

  renderIceMetric(labelName, value) {
    let info = document.createElement("div");

    info.appendChild(
        renderElement("span", `${getString(labelName)}: `, {
          className: "info-label"
        }));
    info.appendChild(
        renderElement("span", value, {className: "info-body"}));

    return info;
  },

  generateICEStats() {
    // Create an index based on candidate ID for each element in the
    // iceCandidateStats array.
    let candidates = new Map();

    for (let candidate of this._report.iceCandidateStats) {
      candidates.set(candidate.id, candidate);
    }

    // a method to see if a given candidate id is in the array of tickled
    // candidates.
    let isTrickled = id => [...this._report.trickledIceCandidateStats].some(
      candidate => candidate.id == id);

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
          ["local-candidate"]: this.candidateToString(local),
          state: pair.state,
          priority: pair.priority,
          nominated: pair.nominated,
          selected: pair.selected,
          bytesSent: pair.bytesSent,
          bytesReceived: pair.bytesReceived
        };
        matched[local.id] = true;
        if (isTrickled(local.id)) {
            stat["local-trickled"] = true;
        }

        if (remote) {
          stat["remote-candidate"] = this.candidateToString(remote);
          matched[remote.id] = true;
          if (isTrickled(remote.id)) {
            stat["remote-trickled"] = true;
          }
        }
        stats.push(stat);
      }
    }

    return stats.sort((a, b) => (b.bytesSent ?
                                 (b.bytesSent || 0) - (a.bytesSent || 0) :
                                 (b.priority || 0) - (a.priority || 0)
                                ));
  },

  candidateToString(c) {
    if (!c) {
      return "*";
    }

    var type = c.candidateType;

    if (c.type == "local-candidate" && c.candidateType == "relayed") {
      type = `${c.candidateType}-${c.mozLocalTransport}`;
    }

    return `${c.ipAddress}:${c.portNumber}/${c.transport}(${type})`;
  }
};

function FoldableSection(parentElement, options = {}) {
  this._foldableElement = document.createElement("div");
  if (parentElement) {
    let sectionCtrl = renderElement("div", null, {
      className: "section-ctrl no-print"
    });
    let foldEffect = new FoldEffect(this._foldableElement, options);
    sectionCtrl.appendChild(foldEffect.render());
    parentElement.appendChild(sectionCtrl);
  }
}

FoldableSection.prototype = {
  render() {
    return this._foldableElement;
  }
};

function SimpleTable(heading, data, caption) {
  this._heading = heading || [];
  this._data = data;
  this._caption = caption;
}

SimpleTable.prototype = {
  renderRow(list, header) {
    let row = document.createElement("tr");
    let elemType = (header ? "th" : "td");

    for (let elem of list) {
      row.appendChild(renderElement(elemType, elem));
    }

    return row;
  },

  render() {
    let table = document.createElement("table");

    if (this._caption) {
      table.appendChild(this._caption);
    }

    if (this._heading) {
      table.appendChild(this.renderRow(this._heading, true));
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
  render() {
    this._target.classList.add("fold-target");

    let ctrl = renderElement("div", null, {className: "fold-trigger"});
    this._trigger = ctrl;
    ctrl.addEventListener("click", this.onClick.bind(this));
    this.close();

    FoldEffect._sections.push(this);
    return ctrl;
  },

  onClick() {
    if (this._target.classList.contains("fold-closed")) {
      this.open();
    } else {
      this.close();
    }
    return true;
  },

  open() {
    this._target.classList.remove("fold-closed");
    this._trigger.setAttribute("title", this._hideHint);
    this._trigger.textContent = this._hideMsg;
  },

  close() {
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
