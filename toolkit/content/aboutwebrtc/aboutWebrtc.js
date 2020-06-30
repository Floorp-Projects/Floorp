/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "FilePicker",
  "@mozilla.org/filepicker;1",
  "nsIFilePicker"
);
XPCOMUtils.defineLazyGetter(this, "strings", () => {
  return Services.strings.createBundle(
    "chrome://global/locale/aboutWebrtc.properties"
  );
});

const getString = strings.GetStringFromName;
const formatString = strings.formatStringFromName;

const LOGFILE_NAME_DEFAULT = "aboutWebrtc.html";
const WEBRTC_TRACE_ALL = 65535;

const getStats = () => new Promise(r => WebrtcGlobalInformation.getAllStats(r));
const getLog = () =>
  new Promise(r => WebrtcGlobalInformation.getLogging("", r));

// Setup

(async () => {
  // Begin initial data queries as page loads.
  const reportsRetrieved = getStats();
  const logRetrieved = getLog();
  await new Promise(r => (window.onload = r));

  document.title = getString("document_title");
  const controls = document.querySelector("#controls");
  if (controls) {
    const render = className => renderElement("div", null, { className });
    const set = render("controls");
    const control = render("control");
    const message = render("message");
    set.appendChild(control);
    set.appendChild(message);

    const add = controlObj => {
      const [ctrl, msg] = controlObj.render();
      control.appendChild(ctrl);
      message.appendChild(msg);
    };

    add(new SavePage());
    add(new DebugMode());
    add(new AecLogging());
    controls.appendChild(set);
  }

  const contentElem = document.querySelector("#content");
  if (!contentElem) {
    return;
  }

  AboutWebRTC.init(
    async () => {
      WebrtcGlobalInformation.clearAllStats();
      try {
        const { reports } = await getStats();
        AboutWebRTC.refresh({ reports });
      } catch (reportError) {
        AboutWebRTC.refresh({ reportError });
      }
    },
    async () => {
      WebrtcGlobalInformation.clearLogging();
      try {
        AboutWebRTC.refresh({ log: await getLog() });
      } catch (logError) {
        AboutWebRTC.refresh({ logError });
      }
    }
  );

  try {
    const { reports } = await reportsRetrieved;
    AboutWebRTC.render(contentElem, { reports, log: await logRetrieved });
  } catch (error) {
    AboutWebRTC.render(contentElem, { error });
  }
})();

// Button control classes

class Control {
  _label = null;
  _message = null;
  _messageHeader = null;

  render() {
    const controlElem = document.createElement("button");
    const messageElem = document.createElement("p");

    this.ctrl = controlElem;
    controlElem.onclick = () => this.onClick();
    this.msg = messageElem;
    this.update();

    return [controlElem, messageElem];
  }

  set label(val) {
    return (this._labelVal = val || "\xA0");
  }

  get label() {
    return this._labelVal;
  }

  set message(val) {
    return (this._messageVal = val);
  }

  get message() {
    return this._messageVal;
  }

  update() {
    this.ctrl.textContent = this._label;

    this.msg.textContent = "";
    if (this._message) {
      this.msg.appendChild(
        Object.assign(document.createElement("span"), {
          className: "info-label",
          textContent: `${this._messageHeader}: `,
        })
      );
      this.msg.appendChild(document.createTextNode(this._message));
    }
  }

  onClick() {
    return true;
  }
}

class SavePage extends Control {
  constructor() {
    super();
    this._messageHeader = getString("save_page_label");
    this._label = getString("save_page_label");
  }

  async onClick() {
    const content = document.querySelector("#content");
    if (!content) {
      return;
    }

    FoldEffect.expandAll();
    const title = getString("save_page_dialog_title");
    FilePicker.init(window, title, FilePicker.modeSave);
    FilePicker.defaultString = LOGFILE_NAME_DEFAULT;
    const rv = await new Promise(r => FilePicker.open(r));
    if (rv != FilePicker.returnOK && rv != FilePicker.returnReplace) {
      return;
    }
    const fout = FileUtils.openAtomicFileOutputStream(
      FilePicker.file,
      FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE
    );

    const noPrintList = [...content.querySelectorAll(".no-print")];
    for (const node of noPrintList) {
      node.style.setProperty("display", "none");
    }

    fout.write(content.outerHTML, content.outerHTML.length);
    FileUtils.closeAtomicFileOutputStream(fout);

    for (const node of noPrintList) {
      node.style.removeProperty("display");
    }
    this._message = formatString("save_page_msg", [FilePicker.file.path]);
    this.update();
  }
}

class DebugMode extends Control {
  constructor() {
    super();
    this._messageHeader = getString("debug_mode_msg_label");

    if (WebrtcGlobalInformation.debugLevel > 0) {
      this.setState(true);
    } else {
      this._label = getString("debug_mode_off_state_label");
      this._message = null;
    }
  }

  setState(state) {
    this._label = getString(`debug_mode_${state ? "on" : "off"}_state_label`);
    try {
      let file = Services.prefs.getCharPref("media.webrtc.debug.log_file");
      this._message = formatString("debug_mode_on_state_msg", [file]);
    } catch (e) {
      this._message = null;
    }
  }

  onClick() {
    if (WebrtcGlobalInformation.debugLevel > 0) {
      WebrtcGlobalInformation.debugLevel = 0;
      this.setState(false);
    } else {
      WebrtcGlobalInformation.debugLevel = WEBRTC_TRACE_ALL;
      this.setState(true);
    }
    this.update();
  }
}

class AecLogging extends Control {
  constructor() {
    super();
    this._messageHeader = getString("aec_logging_msg_label");

    if (WebrtcGlobalInformation.aecDebug) {
      this.setState(true);
    } else {
      this._label = getString("aec_logging_off_state_label");
      this._message = null;
    }
  }

  setState(state) {
    this._label = getString(`aec_logging_${state ? "on" : "off"}_state_label`);
    try {
      if (!state) {
        const file = WebrtcGlobalInformation.aecDebugLogDir;
        this._message = formatString("aec_logging_off_state_msg", [file]);
      } else {
        this._message = getString("aec_logging_on_state_msg");
      }
    } catch (e) {
      this._message = null;
    }
  }

  onClick() {
    this.setState(
      (WebrtcGlobalInformation.aecDebug = !WebrtcGlobalInformation.aecDebug)
    );
    this.update();
  }
}

// Singleton app logic

const AboutWebRTC = {
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
        renderElement("p", `${data.error.name}: ${data.error.message}`)
      );
      return;
    }

    this._peerConnections = this.renderPeerConnections();
    this._connectionLog = this.renderConnectionLog();
    this._content.appendChild(this._peerConnections);
    this._content.appendChild(this._connectionLog);
  },

  _setData({ reports = [], log = this._log }) {
    this._reports = [...reports].sort((a, b) => b.timestamp - a.timestamp);
    this._log = log;
  },

  refresh(data) {
    this._setData(data);
    const pc = this._peerConnections;
    this._peerConnections = this.renderPeerConnections();
    const log = this._connectionLog;
    this._connectionLog = this.renderConnectionLog();
    this._content.replaceChild(this._peerConnections, pc);
    this._content.replaceChild(this._connectionLog, log);
  },

  renderPeerConnections() {
    const connections = renderElement("div", null, { className: "stats" });
    const heading = renderElement("span", null, {
      className: "section-heading",
    });
    heading.appendChild(renderElement("h3", getString("stats_heading")));

    heading.appendChild(
      renderElement("button", getString("stats_clear"), {
        className: "no-print",
        onclick: this._onClearStats,
      })
    );
    connections.appendChild(heading);

    connections.append(
      ...this._reports.map(r => new PeerConnection(r).render())
    );
    return connections;
  },

  renderConnectionLog() {
    let content = renderElement("div", null, { className: "log" });

    let heading = renderElement("span", null, { className: "section-heading" });
    heading.appendChild(renderElement("h3", getString("log_heading")));
    heading.appendChild(
      renderElement("button", getString("log_clear"), {
        className: "no-print",
        onclick: this._onClearLog,
      })
    );
    content.appendChild(heading);

    if (!this._log || !this._log.length) {
      return content;
    }

    let div = new FoldableSection(content, {
      showMsg: getString("log_show_msg"),
      hideMsg: getString("log_hide_msg"),
    }).render();

    for (const line of this._log) {
      div.appendChild(renderElement("p", line));
    }

    content.appendChild(div);
    return content;
  },
};

class PeerConnection {
  constructor(report) {
    this._report = report;
  }

  render() {
    const pc = renderElement("div", null, { className: "peer-connection" });
    pc.appendChild(this.renderHeading());

    const div = new FoldableSection(pc).render();

    div.appendChild(this.renderDesc());
    div.appendChild(this.renderConfiguration());

    div.appendChild(new ICEStats(this._report).render());
    div.appendChild(new SDPStats(this._report).render());
    for (const frameStats of this._report.videoFrameHistories) {
      div.appendChild(new FrameRateStats(frameStats).render());
    }
    div.appendChild(new RTPStats(this._report).render());
    pc.appendChild(div);
    return pc;
  }

  renderHeading() {
    const pcInfo = this.getPCInfo(this._report);
    const heading = document.createElement("h3");
    const now = new Date(this._report.timestamp).toString();
    heading.textContent = `[ ${pcInfo.id} ] ${pcInfo.url} ${
      pcInfo.closed ? `(${getString("connection_closed")})` : ""
    } ${now}`;
    return heading;
  }

  renderDesc() {
    const info = document.createElement("div");

    info.appendChild(
      renderElement("span", `${getString("peer_connection_id_label")}: `),
      {
        className: "info-label",
      }
    );

    info.appendChild(
      renderElement("span", this._report.pcid, {
        className: "info-body",
      })
    );

    return info;
  }

  renderConfiguration() {
    const provided = () => {
      const italics = document.createElement("i");
      italics.textContent = getString("configuration_element_provided");
      return italics;
    };
    const notProvided = () => {
      const italics = document.createElement("i");
      italics.textContent = getString("configuration_element_not_provided");
      return italics;
    };
    const br = () => document.createElement("br");

    const div = document.createElement("div");
    div.classList = "peer-connection-config";
    // Create the text for a configuration field
    const cfg = (obj, key, elem) => {
      elem.append(br(), `${key}: `, key in obj ? obj[key] : notProvided());
    };
    // Create the text for a fooProvided configuration field
    const pro = (obj, key, elem) => {
      elem.append(
        br(),
        `${key}(`,
        provided(),
        "/",
        notProvided(),
        "): ",
        `${key}Provided` in obj ? provided() : notProvided()
      );
    };

    const c = this._report.configuration;
    div.append("RTCConfiguration");
    cfg(c, "bundlePolicy", div);
    cfg(c, "iceTransportPolicy", div);
    pro(c, "peerIdentity", div);
    cfg(c, "sdpSemantics", div);
    div.append(br(), "iceServers: ");
    if (!c.iceServers) {
      div.append(notProvided());
    }
    for (const i of c.iceServers) {
      const inner = document.createElement("div");
      div.append(inner);
      inner.append(`urls: ${JSON.stringify(i.urls)}`);
      pro(i, "credential", inner);
      pro(i, "userName", inner);
    }
    return div;
  },

  getPCInfo(report) {
    return {
      id: report.pcid.match(/id=(\S+)/)[1],
      url: report.pcid.match(/url=([^)]+)/)[1],
      closed: report.closed,
    };
  }
}

function renderElement(elemName, elemText, options = {}) {
  const elem = document.createElement(elemName);
  // check for null instead of using elemText || "" so we don't hide
  // elements with 0 values
  if (elemText != null) {
    elem.textContent = elemText;
  }
  Object.assign(elem, options);
  return elem;
}

class SDPStats {
  constructor(report) {
    this._report = report;
  }

  render() {
    const div = document.createElement("div");
    div.appendChild(renderElement("h4", getString("sdp_heading")));

    const offerLabel = `(${getString("offer")})`;
    const answerLabel = `(${getString("answer")})`;
    const localSdpHeading = `${getString("local_sdp_heading")} ${
      this._report.offerer ? offerLabel : answerLabel
    }`;
    const remoteSdpHeading = `${getString("remote_sdp_heading")} ${
      this._report.offerer ? answerLabel : offerLabel
    }`;

    div.appendChild(renderElement("h5", localSdpHeading));
    div.appendChild(renderElement("pre", this._report.localSdp));

    div.appendChild(renderElement("h5", remoteSdpHeading));
    div.appendChild(renderElement("pre", this._report.remoteSdp));

    div.appendChild(renderElement("h4", getString("sdp_history_heading")));
    for (const history of this._report.sdpHistory) {
      const historyElem = renderElement("div", null, {
        className: "sdp-history",
      });
      const sdpSide = history.isLocal
        ? getString("local_sdp_heading")
        : getString("remote_sdp_heading");
      historyElem.appendChild(
        renderElement(
          "h5",
          formatString("sdp_set_at_timestamp", [sdpSide, history.timestamp])
        )
      );
      const historyDiv = new FoldableSection(historyElem).render();
      if (history.errors.length) {
        historyDiv.append(
          renderElement("h5", getString("sdp_parsing_errors_heading"))
        );
      }
      for (const { lineNumber, error } of history.errors) {
        historyDiv.append(renderElement("br"), `${lineNumber}: ${error}`);
      }
      historyDiv.append(renderElement("pre", history.sdp));
      historyElem.appendChild(historyDiv);
      div.appendChild(historyElem);
    }
    return div;
  }
}

class FrameRateStats {
  constructor(frameHistory) {
    this.remoteSsrc = frameHistory.remoteSsrc;
    this.trackIdentifier = frameHistory.trackIdentifier;
    this.stats = frameHistory.entries.map(stat => {
      stat.elapsed = stat.lastFrameTimestamp - stat.firstFrameTimestamp;
      if (stat.elapsed < 1) {
        stat.elapsed = 0;
      }
      stat.elapsed = stat.elapsed / 1_000;
      if (stat.elapsed && stat.consecutiveFrames) {
        stat.avgFramerate = stat.consecutiveFrames / stat.elapsed;
      } else {
        stat.avgFramerate = getString("n_a");
      }
      return stat;
    });
  }

  render() {
    const div = document.createElement("div");
    div.appendChild(
      renderElement(
        "h4",
        `${getString("frame_stats_heading")} - MediaStreamTrack Id: ${
          this.trackIdentifier
        }`
      )
    );
    div.appendChild(this.renderFrameStatSet());
    return div;
  }

  renderFrameStatSet() {
    const caption = "";
    const tbody = this.stats.map(stat =>
      [
        stat.width,
        stat.height,
        stat.consecutiveFrames,
        stat.elapsed,
        stat.avgFramerate,
        stat.rotationAngle,
        stat.lastFrameTimestamp,
        stat.firstFrameTimestamp,
        stat.localSsrc,
        stat.remoteSsrc || "?",
      ].map(entry => (Object.is(entry, undefined) ? "<<undefined>>" : entry))
    );
    return new SimpleTable(
      [
        "width_px",
        "height_px",
        "consecutive_frames",
        "time_elapsed",
        "estimated_framerate",
        "rotation_degrees",
        "first_frame_timestamp",
        "last_frame_timestamp",
        "local_receive_ssrc",
        "remote_send_ssrc",
      ].map(columnName => getString(columnName)),
      tbody,
      caption
    ).render();
  }
}

class RTPStats {
  constructor(report) {
    this._report = report;
  }

  render() {
    const div = document.createElement("div");
    div.appendChild(renderElement("h4", getString("rtp_stats_heading")));

    const rtpStats = [
      ...(this._report.inboundRtpStreamStats || []),
      ...(this._report.outboundRtpStreamStats || []),
    ];
    const remoteRtpStats = [
      ...(this._report.remoteInboundRtpStreamStats || []),
      ...(this._report.remoteOutboundRtpStreamStats || []),
    ];

    // Generate an id-to-streamStat index for each remote streamStat. This will
    // be used next to link the remote to its local side.
    const remoteRtpStatsMap = {};
    for (const stat of remoteRtpStats) {
      remoteRtpStatsMap[stat.id] = stat;
    }

    // If a streamStat has a remoteId attribute, create a remoteRtpStats
    // attribute that references the remote streamStat entry directly.
    // That is, the index generated above is merged into the returned list.
    for (const stat of rtpStats.filter(s => "remoteId" in s)) {
      stat.remoteRtpStats = remoteRtpStatsMap[stat.remoteId];
    }
    const stats = [...rtpStats, ...remoteRtpStats];
    div.append(...stats.map(stat => this.renderRTPStatSet(stat)));
    return div;
  }

  renderCoderStats(stats) {
    let statsString = "";
    let label;

    if (stats.bitrateMean) {
      statsString += ` ${getString("avg_bitrate_label")}: ${(
        stats.bitrateMean / 1000000
      ).toFixed(2)} Mbps`;
      if (stats.bitrateStdDev) {
        statsString += ` (${(stats.bitrateStdDev / 1000000).toFixed(2)} SD)`;
      }
    }

    if (stats.framerateMean) {
      statsString += ` ${getString(
        "avg_framerate_label"
      )}: ${stats.framerateMean.toFixed(2)} fps`;
      if (stats.framerateStdDev) {
        statsString += ` (${stats.framerateStdDev.toFixed(2)} SD)`;
      }
    }

    if (stats.droppedFrames) {
      statsString += ` ${getString("dropped_frames_label")}: ${
        stats.droppedFrames
      }`;
    }
    if (stats.discardedPackets) {
      statsString += ` ${getString("discarded_packets_label")}: ${
        stats.discardedPackets
      }`;
    }

    if (statsString.length) {
      label = stats.packetsReceived
        ? ` ${getString("decoder_label")}:`
        : ` ${getString("encoder_label")}:`;
      statsString = label + statsString;
    }
    return renderElement("p", statsString);
  }

  renderTransportStats(stats, typeLabel) {
    const time = new Date(stats.timestamp).toTimeString();
    let statsString = `${typeLabel}: ${time} ${stats.type} SSRC: ${stats.ssrc}`;

    if (stats.packetsReceived) {
      statsString += ` ${getString("received_label")}: ${
        stats.packetsReceived
      } ${getString("packets")}`;

      if (stats.bytesReceived) {
        statsString += ` (${(stats.bytesReceived / 1024).toFixed(2)} Kb)`;
      }

      statsString += ` ${getString("lost_label")}: ${
        stats.packetsLost
      } ${getString("jitter_label")}: ${stats.jitter}`;

      if (stats.roundTripTime) {
        statsString += ` RTT: ${stats.roundTripTime * 1000} ms`;
      }
    } else if (stats.packetsSent) {
      statsString += ` ${getString("sent_label")}: ${
        stats.packetsSent
      } ${getString("packets")}`;
      if (stats.bytesSent) {
        statsString += ` (${(stats.bytesSent / 1024).toFixed(2)} Kb)`;
      }
    }
    return renderElement("p", statsString);
  }

  renderRTPStatSet(stats) {
    const div = document.createElement("div");
    div.appendChild(renderElement("h5", stats.id));

    div.appendChild(this.renderCoderStats(stats));
    div.appendChild(this.renderTransportStats(stats, getString("typeLocal")));

    if (stats.remoteId && stats.remoteRtpStats) {
      div.appendChild(
        this.renderTransportStats(stats.remoteRtpStats, getString("typeRemote"))
      );
    }
    return div;
  }
}

class ICEStats {
  constructor(report) {
    this._report = report;
  }

  render() {
    const div = document.createElement("div");
    div.appendChild(renderElement("h4", getString("ice_stats_heading")));

    div.appendChild(this.renderICECandidateTable());
    // add just a bit of vertical space between the restart/rollback
    // counts and the ICE candidate pair table above.
    div.appendChild(document.createElement("br"));
    div.appendChild(
      this.renderIceMetric("ice_restart_count_label", this._report.iceRestarts)
    );
    div.appendChild(
      this.renderIceMetric(
        "ice_rollback_count_label",
        this._report.iceRollbacks
      )
    );
    div.appendChild(this.renderRawICECandidateSection());
    return div;
  }

  renderICECandidateTable() {
    const caption = renderElement("caption", null, { className: "no-print" });

    // This takes the caption message with the replacement token, breaks
    // it around the token, and builds the spans for each portion of the
    // caption.  This is to allow localization to put the color name for
    // the highlight wherever it is appropriate in the translated string
    // while avoiding innerHTML warnings from eslint.
    const captionTemplate = getString("trickle_caption_msg2");
    const [start, end] = captionTemplate.split(/%(?:1\$)?S/);

    // only append span if non-whitespace chars present
    if (/\S/.test(start)) {
      caption.appendChild(renderElement("span", `${start}`));
    }
    caption.appendChild(
      renderElement("span", getString("trickle_highlight_color_name2"), {
        className: "ice-trickled",
      })
    );
    // only append span if non-whitespace chars present
    if (/\S/.test(end)) {
      caption.appendChild(renderElement("span", `${end}`));
    }

    const stats = this.generateICEStats();
    // don't use |stat.x || ""| here because it hides 0 values
    const tbody = stats.map(stat =>
      [
        stat.state,
        stat.nominated,
        stat.selected,
        stat["local-candidate"],
        stat["remote-candidate"],
        stat.componentId,
        stat.priority,
        stat.bytesSent,
        stat.bytesReceived,
      ].map(entry => (Object.is(entry, undefined) ? "" : entry))
    );

    const statsTable = new SimpleTable(
      [
        "ice_state",
        "nominated",
        "selected",
        "local_candidate",
        "remote_candidate",
        "ice_component_id",
        "priority",
        "ice_pair_bytes_sent",
        "ice_pair_bytes_received",
      ].map(columnName => getString(columnName)),
      tbody,
      caption
    ).render();

    // after rendering the table, we need to change the class name for each
    // candidate pair's local or remote candidate if it was trickled.
    let index = 0;
    for (const {
      state,
      nominated,
      selected,
      "local-trickled": localTrickled,
      "remote-trickled": remoteTrickled,
    } of stats) {
      // look at statsTable row index + 1 to skip column headers
      const { cells } = statsTable.rows[++index];
      cells[0].className = `ice-${state}`;
      if (nominated) {
        cells[1].className = "ice-success";
      }
      if (selected) {
        cells[2].className = "ice-success";
      }
      if (localTrickled) {
        cells[3].className = "ice-trickled";
      }
      if (remoteTrickled) {
        cells[4].className = "ice-trickled";
      }
    }

    // if the next row's component id changes, mark the bottom of the
    // current row with a thin, black border to differentiate the
    // component id grouping.
    const rowCount = statsTable.rows.length - 1;
    for (let i = 0; i < rowCount; i++) {
      if (
        statsTable.rows[i].cells[5].innerHTML !==
        statsTable.rows[i + 1].cells[5].innerHTML
      ) {
        statsTable.rows[i].className = "bottom-border";
      }
    }
    return statsTable;
  }

  renderRawICECandidates() {
    const div = document.createElement("div");
    const candidates = direction =>
      [
        ...new Set(
          direction == "local"
            ? this._report.rawLocalCandidates.sort()
            : this._report.rawRemoteCandidates.sort()
        ),
      ]
        .filter(i => `${i}` != "")
        .map(i => [i]);

    for (const direction of ["local", "remote"]) {
      const statsTable = new SimpleTable(
        [getString(`raw_${direction}_candidate`)],
        candidates(direction)
      ).render();
      statsTable.className = "raw-candidate";
      div.appendChild(statsTable);
    }
    return div;
  }

  renderRawICECandidateSection() {
    const section = document.createElement("div");
    section.appendChild(
      renderElement("h4", getString("raw_candidates_heading"))
    );

    const div = new FoldableSection(section, {
      showMsg: getString("raw_cand_show_msg"),
      hideMsg: getString("raw_cand_hide_msg"),
    }).render();

    div.appendChild(this.renderRawICECandidates());
    section.appendChild(div);
    return section;
  }

  renderIceMetric(labelName, value) {
    const info = document.createElement("div");

    info.appendChild(
      renderElement("span", `${getString(labelName)}: `, {
        className: "info-label",
      })
    );
    info.appendChild(renderElement("span", value, { className: "info-body" }));
    return info;
  }

  generateICEStats() {
    // Create an index based on candidate ID for each element in the
    // iceCandidateStats array.
    const candidates = new Map();

    for (const candidate of this._report.iceCandidateStats) {
      candidates.set(candidate.id, candidate);
    }

    // a method to see if a given candidate id is in the array of tickled
    // candidates.
    const isTrickled = candidateId =>
      [...this._report.trickledIceCandidateStats].some(
        ({ id }) => id == candidateId
      );

    // A component may have a remote or local candidate address or both.
    // Combine those with both; these will be the peer candidates.
    const matched = {};
    const stats = [];
    let stat;

    for (const {
      localCandidateId,
      remoteCandidateId,
      componentId,
      state,
      priority,
      nominated,
      selected,
      bytesSent,
      bytesReceived,
    } of this._report.iceCandidatePairStats) {
      const local = candidates.get(localCandidateId);
      if (local) {
        stat = {
          ["local-candidate"]: this.candidateToString(local),
          componentId,
          state,
          priority,
          nominated,
          selected,
          bytesSent,
          bytesReceived,
        };
        matched[local.id] = true;
        if (isTrickled(local.id)) {
          stat["local-trickled"] = true;
        }

        const remote = candidates.get(remoteCandidateId);
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

    // sort (group by) componentId first, then bytesSent if available, else by
    // priority
    return stats.sort((a, b) => {
      if (a.componentId != b.componentId) {
        return a.componentId - b.componentId;
      }
      return b.bytesSent
        ? (b.bytesSent || 0) - (a.bytesSent || 0)
        : (b.priority || 0) - (a.priority || 0);
    });
  }

  candidateToString(c) {
    if (!c) {
      return "*";
    }
    const type =
      c.type == "local-candidate" && c.candidateType == "relayed"
        ? `${c.candidateType}-${c.relayProtocol}`
        : c.candidateType;
    const proxied = c.type == "local-candidate" ? ` [${c.proxied}]` : "";

    return `${c.address}:${c.port}/${c.protocol}(${type})${proxied}`;
  }
}

class FoldableSection {
  constructor(parentElement, options = {}) {
    this._foldableElement = document.createElement("div");
    if (parentElement) {
      const sectionCtrl = renderElement("div", null, {
        className: "section-ctrl no-print",
      });
      const foldEffect = new FoldEffect(this._foldableElement, options);
      sectionCtrl.appendChild(foldEffect.render());
      parentElement.appendChild(sectionCtrl);
    }
  }

  render() {
    return this._foldableElement;
  }
}

class SimpleTable {
  constructor(heading = [], data, caption) {
    this._heading = heading;
    this._data = data;
    this._caption = caption;
  }

  renderRow(list, elemType) {
    const row = document.createElement("tr");
    row.append(...list.map(elem => renderElement(elemType, elem)));
    return row;
  }

  render() {
    const table = document.createElement("table");

    if (this._caption) {
      table.appendChild(this._caption);
    }
    if (this._heading) {
      table.appendChild(this.renderRow(this._heading, "th"));
    }
    table.append(...this._data.map(row => this.renderRow(row, "td")));
    return table;
  }
}

class FoldEffect {
  static _sections = [];

  constructor(targetElem, { showMsg, showHint, hideMsg, hideHint } = {}) {
    if (targetElem) {
      this._showMsg = "\u25BC " + (showMsg || getString("fold_show_msg"));
      this._showHint = showHint || getString("fold_show_hint");
      this._hideMsg = "\u25B2 " + (hideMsg || getString("fold_hide_msg"));
      this._hideHint = hideHint || getString("fold_hide_hint");
      this._target = targetElem;
    }
  }

  render() {
    this._target.classList.add("fold-target");

    const ctrl = renderElement("div", null, { className: "fold-trigger" });
    this._trigger = ctrl;
    ctrl.addEventListener("click", () => this.onClick());
    this.close();

    FoldEffect._sections.push(this);
    return ctrl;
  }

  onClick() {
    if (this._target.classList.contains("fold-closed")) {
      this.open();
    } else {
      this.close();
    }
  }

  open() {
    this._target.classList.remove("fold-closed");
    this._trigger.setAttribute("title", this._hideHint);
    this._trigger.textContent = this._hideMsg;
  }

  close() {
    this._target.classList.add("fold-closed");
    this._trigger.setAttribute("title", this._showHint);
    this._trigger.textContent = this._showMsg;
  }

  static expandAll() {
    for (const section of FoldEffect._sections) {
      section.open();
    }
  }

  static collapseAll() {
    for (const section of FoldEffect._sections) {
      section.close();
    }
  }
}
