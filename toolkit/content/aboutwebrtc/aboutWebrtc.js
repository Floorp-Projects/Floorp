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
XPCOMUtils.defineLazyGetter(this, "strings", () =>
  Services.strings.createBundle("chrome://global/locale/aboutWebrtc.properties")
);

const string = strings.GetStringFromName;
const format = strings.formatStringFromName;
const WGI = WebrtcGlobalInformation;

const LOGFILE_NAME_DEFAULT = "aboutWebrtc.html";
const WEBRTC_TRACE_ALL = 65535;

async function getStats() {
  const { reports } = await new Promise(r => WGI.getAllStats(r));
  return [...reports].sort((a, b) => b.timestamp - a.timestamp);
}

const getLog = () => new Promise(r => WGI.getLogging("", r));

const renderElement = (name, options) =>
  Object.assign(document.createElement(name), options);

const renderText = (name, textContent, options) =>
  renderElement(name, Object.assign({ textContent }, options));

const renderElements = (name, options, list) => {
  const element = renderElement(name, options);
  element.append(...list);
  return element;
};

// Button control classes

class Control {
  label = null;
  message = null;
  messageHeader = null;

  render() {
    this.ctrl = renderElement("button", { onclick: () => this.onClick() });
    this.msg = renderElement("p");
    this.update();
    return [this.ctrl, this.msg];
  }

  update() {
    this.ctrl.textContent = this.label;
    this.msg.textContent = "";
    if (this.message) {
      this.msg.append(
        renderText("span", `${this.messageHeader}: `, {
          className: "info-label",
        }),
        this.message
      );
    }
  }
}

class SavePage extends Control {
  constructor() {
    super();
    this.messageHeader = string("save_page_label");
    this.label = string("save_page_label");
  }

  async onClick() {
    FoldEffect.expandAll();
    FilePicker.init(
      window,
      string("save_page_dialog_title"),
      FilePicker.modeSave
    );
    FilePicker.defaultString = LOGFILE_NAME_DEFAULT;
    const rv = await new Promise(r => FilePicker.open(r));
    if (rv != FilePicker.returnOK && rv != FilePicker.returnReplace) {
      return;
    }
    const fout = FileUtils.openAtomicFileOutputStream(
      FilePicker.file,
      FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE
    );
    const content = document.querySelector("#content");
    const noPrintList = [...content.querySelectorAll(".no-print")];
    for (const node of noPrintList) {
      node.style.setProperty("display", "none");
    }
    try {
      fout.write(content.outerHTML, content.outerHTML.length);
    } finally {
      FileUtils.closeAtomicFileOutputStream(fout);
      for (const node of noPrintList) {
        node.style.removeProperty("display");
      }
    }
    this.message = format("save_page_msg", [FilePicker.file.path]);
    this.update();
  }
}

class DebugMode extends Control {
  constructor() {
    super();
    this.messageHeader = string("debug_mode_msg_label");

    if (WGI.debugLevel > 0) {
      this.setState(true);
    } else {
      this.label = string("debug_mode_off_state_label");
    }
  }

  setState(state) {
    const stateString = state ? "on" : "off";
    this.label = string(`debug_mode_${stateString}_state_label`);
    try {
      const file = Services.prefs.getCharPref("media.webrtc.debug.log_file");
      this.message = format(`debug_mode_${stateString}_state_msg`, [file]);
    } catch (e) {
      this.message = null;
    }
    return state;
  }

  onClick() {
    this.setState((WGI.debugLevel = WGI.debugLevel ? 0 : WEBRTC_TRACE_ALL));
    this.update();
  }
}

class AecLogging extends Control {
  constructor() {
    super();
    this.messageHeader = string("aec_logging_msg_label");

    if (WGI.aecDebug) {
      this.setState(true);
    } else {
      this.label = string("aec_logging_off_state_label");
      this.message = null;
    }
  }

  setState(state) {
    this.label = string(`aec_logging_${state ? "on" : "off"}_state_label`);
    try {
      if (!state) {
        const file = WGI.aecDebugLogDir;
        this.message = format("aec_logging_off_state_msg", [file]);
      } else {
        this.message = string("aec_logging_on_state_msg");
      }
    } catch (e) {
      this.message = null;
    }
  }

  onClick() {
    this.setState((WGI.aecDebug = !WGI.aecDebug));
    this.update();
  }
}

(async () => {
  // Setup. Retrieve reports & log while page loads.
  const haveReports = getStats();
  const haveLog = getLog();
  await new Promise(r => (window.onload = r));

  document.title = string("document_title");
  {
    const ctrl = renderElement("div", { className: "control" });
    const msg = renderElement("div", { className: "message" });
    const add = ([control, message]) => {
      ctrl.appendChild(control);
      msg.appendChild(message);
    };
    add(new SavePage().render());
    add(new DebugMode().render());
    add(new AecLogging().render());

    const ctrls = document.querySelector("#controls");
    ctrls.append(renderElements("div", { className: "controls" }, [ctrl, msg]));
  }

  // Render pcs and log
  let reports = await haveReports;
  let log = await haveLog;

  let peerConnections = renderElement("div");
  let connectionLog = renderElement("div");
  let userPrefs = renderElement("div");

  const content = document.querySelector("#content");
  content.append(peerConnections, connectionLog, userPrefs);

  function refresh() {
    const pcDiv = renderElements("div", { className: "stats" }, [
      renderElements("span", { className: "section-heading" }, [
        renderText("h3", string("stats_heading")),
        renderText("button", string("stats_clear"), {
          className: "no-print",
          onclick: async () => {
            WGI.clearAllStats();
            reports = await getStats();
            refresh();
          },
        }),
      ]),
      ...reports.map(renderPeerConnection),
    ]);
    const logDiv = renderElements("div", { className: "log" }, [
      renderElement("span", { className: "section-heading" }, [
        renderText("h3", string("log_heading")),
        renderElement("button", {
          textContent: string("log_clear"),
          className: "no-print",
          onclick: async () => {
            WGI.clearLogging();
            log = await getLog();
            refresh();
          },
        }),
      ]),
    ]);
    if (log.length) {
      const div = renderFoldableSection(logDiv, {
        showMsg: string("log_show_msg"),
        hideMsg: string("log_hide_msg"),
      });
      div.append(...log.map(line => renderText("p", line)));
      logDiv.append(div);
    }
    // Replace previous info
    peerConnections.replaceWith(pcDiv);
    connectionLog.replaceWith(logDiv);
    userPrefs.replaceWith((userPrefs = renderUserPrefs()));

    peerConnections = pcDiv;
    connectionLog = logDiv;
  }
  refresh();

  window.setInterval(
    async history => {
      userPrefs.replaceWith((userPrefs = renderUserPrefs()));
      const reports = await getStats();
      reports.forEach(report => {
        const replace = (id, renderFunc) => {
          const elem = document.getElementById(`${id}: ${report.pcid}`);
          if (elem) {
            elem.replaceWith(renderFunc(report, history));
          }
        };
        replace("ice-stats", renderICEStats);
        replace("rtp-stats", renderRTPStats);
        replace("frame-stats", renderFrameRateStats);
      });
    },
    500,
    {}
  );
})();

function renderPeerConnection(report) {
  const { pcid, closed, timestamp, configuration } = report;

  const pcDiv = renderElement("div", { className: "peer-connection" });
  {
    const id = pcid.match(/id=(\S+)/)[1];
    const url = pcid.match(/url=([^)]+)/)[1];
    const closedStr = closed ? `(${string("connection_closed")})` : "";
    const now = new Date(timestamp).toString();

    pcDiv.append(renderText("h3", `[ ${id} ] ${url} ${closedStr} ${now}`));
  }
  {
    const section = renderFoldableSection(pcDiv);
    section.append(
      renderElements("div", {}, [
        renderText("span", `${string("peer_connection_id_label")}: `, {
          className: "info-label",
        }),
        renderText("span", pcid, { className: "info-body" }),
      ]),
      renderConfiguration(configuration),
      renderICEStats(report),
      renderSDPStats(report),
      renderFrameRateStats(report),
      renderRTPStats(report)
    );
    pcDiv.append(section);
  }
  return pcDiv;
}

function renderSDPStats({ offerer, localSdp, remoteSdp, sdpHistory }) {
  const trimNewlines = sdp => sdp.replaceAll("\r\n", "\n");

  const statsDiv = renderElements("div", {}, [
    renderText("h4", string("sdp_heading")),
    renderText(
      "h5",
      `${string("local_sdp_heading")} (${string(offerer ? "offer" : "answer")})`
    ),
    renderText("pre", trimNewlines(localSdp)),
    renderText(
      "h5",
      `${string("remote_sdp_heading")} (${string(
        offerer ? "answer" : "offer"
      )})`
    ),
    renderText("pre", trimNewlines(remoteSdp)),
    renderText("h4", string("sdp_history_heading")),
  ]);

  // All SDP in sequential order. Add onclick handler to scroll the associated
  // SDP into view below.
  for (const { isLocal, timestamp } of sdpHistory) {
    const histDiv = renderElement("div", {});
    const text = renderText(
      "h5",
      format("sdp_set_at_timestamp", [
        string(`${isLocal ? "local" : "remote"}_sdp_heading`),
        timestamp,
      ]),
      { className: "sdp-history-link" }
    );
    text.onclick = () => {
      const elem = document.getElementById("sdp-history: " + timestamp);
      if (elem) {
        elem.scrollIntoView();
      }
    };
    histDiv.append(text);
    statsDiv.append(histDiv);
  }

  // Render the SDP into separate columns for local and remote.
  const section = renderElement("div", { className: "sdp-history" });
  const localDiv = renderElements("div", {}, [
    renderText("h4", `${string("local_sdp_heading")}`),
  ]);
  const remoteDiv = renderElements("div", {}, [
    renderText("h4", `${string("remote_sdp_heading")}`),
  ]);

  let first = NaN;
  for (const { isLocal, timestamp, sdp, errors } of sdpHistory) {
    if (isNaN(first)) {
      first = timestamp;
    }
    const histDiv = isLocal ? localDiv : remoteDiv;
    histDiv.append(
      renderText(
        "h5",
        format("sdp_set_timestamp", [timestamp, timestamp - first]),
        { id: "sdp-history: " + timestamp }
      )
    );
    if (errors.length) {
      histDiv.append(renderElement("h5", string("sdp_parsing_errors_heading")));
    }
    for (const { lineNumber, error } of errors) {
      histDiv.append(renderElement("br"), `${lineNumber}: ${error}`);
    }
    histDiv.append(renderText("pre", trimNewlines(sdp)));
  }
  section.append(localDiv, remoteDiv);
  statsDiv.append(section);
  return statsDiv;
}

function renderFrameRateStats(report) {
  const statsDiv = renderElement("div", { id: "frame-stats: " + report.pcid });
  report.videoFrameHistories.forEach(history => {
    const stats = history.entries.map(stat => {
      stat.elapsed = stat.lastFrameTimestamp - stat.firstFrameTimestamp;
      if (stat.elapsed < 1) {
        stat.elapsed = 0;
      }
      stat.elapsed = (stat.elapsed / 1_000).toFixed(3);
      if (stat.elapsed && stat.consecutiveFrames) {
        stat.avgFramerate = (stat.consecutiveFrames / stat.elapsed).toFixed(2);
      } else {
        stat.avgFramerate = string("n_a");
      }
      return stat;
    });

    const table = renderSimpleTable(
      "",
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
      ].map(columnName => string(columnName)),
      stats.map(stat =>
        [
          stat.width,
          stat.height,
          stat.consecutiveFrames,
          stat.elapsed,
          stat.avgFramerate,
          stat.rotationAngle,
          stat.firstFrameTimestamp,
          stat.lastFrameTimestamp,
          stat.localSsrc,
          stat.remoteSsrc || "?",
        ].map(entry => (Object.is(entry, undefined) ? "<<undefined>>" : entry))
      )
    );

    statsDiv.append(
      renderText(
        "h4",
        `${string("frame_stats_heading")} - MediaStreamTrack Id: ${
          history.trackIdentifier
        }`
      ),
      table
    );
  });

  return statsDiv;
}

function renderRTPStats(report, history) {
  const rtpStats = [
    ...(report.inboundRtpStreamStats || []),
    ...(report.outboundRtpStreamStats || []),
  ];
  const remoteRtpStats = [
    ...(report.remoteInboundRtpStreamStats || []),
    ...(report.remoteOutboundRtpStreamStats || []),
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

  // Render stats set
  return renderElements("div", { id: "rtp-stats: " + report.pcid }, [
    renderText("h4", string("rtp_stats_heading")),
    ...stats.map(stat => {
      const { id, remoteId, remoteRtpStats } = stat;
      const div = renderElements("div", {}, [
        renderText("h5", id),
        renderCoderStats(stat),
        renderTransportStats(stat, true, history),
      ]);
      if (remoteId && remoteRtpStats) {
        div.append(renderTransportStats(remoteRtpStats, false));
      }
      return div;
    }),
  ]);
}

function renderCoderStats({
  bitrateMean,
  bitrateStdDev,
  framerateMean,
  framerateStdDev,
  droppedFrames,
  discardedPackets,
  packetsReceived,
}) {
  let s = "";

  if (bitrateMean) {
    s += ` ${string("avg_bitrate_label")}: ${(bitrateMean / 1000000).toFixed(
      2
    )} Mbps`;
    if (bitrateStdDev) {
      s += ` (${(bitrateStdDev / 1000000).toFixed(2)} SD)`;
    }
  }
  if (framerateMean) {
    s += ` ${string("avg_framerate_label")}: ${framerateMean.toFixed(2)} fps`;
    if (framerateStdDev) {
      s += ` (${framerateStdDev.toFixed(2)} SD)`;
    }
  }
  if (droppedFrames) {
    s += ` ${string("dropped_frames_label")}: ${droppedFrames}`;
  }
  if (discardedPackets) {
    s += ` ${string("discarded_packets_label")}: ${discardedPackets}`;
  }
  if (s.length) {
    s = ` ${string(`${packetsReceived ? "de" : "en"}coder_label`)}:${s}`;
  }
  return renderText("p", s);
}

function renderTransportStats(
  {
    id,
    timestamp,
    type,
    ssrc,
    packetsReceived,
    bytesReceived,
    packetsLost,
    jitter,
    roundTripTime,
    packetsSent,
    bytesSent,
  },
  local,
  history
) {
  const typeLabel = local ? string("typeLocal") : string("typeRemote");

  if (history) {
    if (history[id] === undefined) {
      history[id] = {};
    }
  }

  const estimateKbps = (timestamp, lastTimestamp, bytes, lastBytes) => {
    if (!timestamp || !lastTimestamp || !bytes || !lastBytes) {
      return string("n_a");
    }
    const elapsedTime = timestamp - lastTimestamp;
    if (elapsedTime <= 0) {
      return string("n_a");
    }
    return ((bytes - lastBytes) / elapsedTime).toFixed(1);
  };

  const time = new Date(timestamp).toTimeString();
  let s = `${typeLabel}: ${time} ${type} SSRC: ${ssrc}`;

  const packets = string("packets");
  if (packetsReceived) {
    s += ` ${string("received_label")}: ${packetsReceived} ${packets}`;

    if (bytesReceived) {
      s += ` (${(bytesReceived / 1024).toFixed(2)} Kb`;
      if (local && history) {
        s += ` , ~${estimateKbps(
          timestamp,
          history[id].lastTimestamp,
          bytesReceived,
          history[id].lastBytesReceived
        )} Kbps`;
      }
      s += ")";
    }

    s += ` ${string("lost_label")}: ${packetsLost} ${string(
      "jitter_label"
    )}: ${jitter}`;

    if (roundTripTime) {
      s += ` RTT: ${roundTripTime * 1000} ms`;
    }
  } else if (packetsSent) {
    s += ` ${string("sent_label")}: ${packetsSent} ${packets}`;
    if (bytesSent) {
      s += ` (${(bytesSent / 1024).toFixed(2)} Kb`;
      if (local && history) {
        s += `, ~${estimateKbps(
          timestamp,
          history[id].lastTimestamp,
          bytesSent,
          history[id].lastBytesSent
        )} Kbps`;
      }
      s += ")";
    }
  }

  // Update history
  if (history) {
    history[id].lastBytesReceived = bytesReceived;
    history[id].lastBytesSent = bytesSent;
    history[id].lastTimestamp = timestamp;
  }

  return renderText("p", s);
}

function renderRawIceTable(caption, candidates) {
  const table = renderSimpleTable(
    "",
    [string(caption)],
    [...new Set(candidates.sort())].filter(i => i).map(i => [i])
  );
  table.className = "raw-candidate";
  return table;
}

function renderConfiguration(c) {
  const provided = string("configuration_element_provided");
  const notProvided = string("configuration_element_not_provided");

  // Create the text for a configuration field
  const cfg = (obj, key) => [
    renderElement("br"),
    `${key}: `,
    key in obj ? obj[key] : renderText("i", notProvided),
  ];

  // Create the text for a fooProvided configuration field
  const pro = (obj, key) => [
    renderElement("br"),
    `${key}(`,
    renderText("i", provided),
    `/`,
    renderText("i", notProvided),
    `): `,
    renderText("i", obj[`${key}Provided`] ? provided : notProvided),
  ];

  return renderElements("div", { classList: "peer-connection-config" }, [
    "RTCConfiguration",
    ...cfg(c, "bundlePolicy"),
    ...cfg(c, "iceTransportPolicy"),
    ...pro(c, "peerIdentity"),
    ...cfg(c, "sdpSemantics"),
    renderElement("br"),
    "iceServers: ",
    ...(!c.iceServers
      ? [renderText("i", notProvided)]
      : c.iceServers.map(i =>
          renderElements("div", {}, [
            `urls: ${JSON.stringify(i.urls)}`,
            ...pro(i, "credential"),
            ...pro(i, "userName"),
          ])
        )),
  ]);
}

function renderICEStats(report) {
  const iceDiv = renderElements("div", { id: "ice-stats: " + report.pcid }, [
    renderText("h4", string("ice_stats_heading")),
  ]);

  // Render ICECandidate table
  {
    const caption = renderElement("caption", { className: "no-print" });

    // This takes the caption message with the replacement token, breaks
    // it around the token, and builds the spans for each portion of the
    // caption.  This is to allow localization to put the color name for
    // the highlight wherever it is appropriate in the translated string
    // while avoiding innerHTML warnings from eslint.
    const [start, end] = string("trickle_caption_msg2").split(/%(?:1\$)?S/);

    // only append span if non-whitespace chars present
    if (/\S/.test(start)) {
      caption.append(renderText("span", start));
    }
    caption.append(
      renderText("span", string("trickle_highlight_color_name2"), {
        className: "ice-trickled",
      })
    );
    // only append span if non-whitespace chars present
    if (/\S/.test(end)) {
      caption.append(renderText("span", end));
    }

    // Generate ICE stats
    const stats = [];
    {
      // Create an index based on candidate ID for each element in the
      // iceCandidateStats array.
      const candidates = {};
      for (const candidate of report.iceCandidateStats) {
        candidates[candidate.id] = candidate;
      }

      // a method to see if a given candidate id is in the array of tickled
      // candidates.
      const isTrickled = candidateId =>
        report.trickledIceCandidateStats.some(({ id }) => id == candidateId);

      // A component may have a remote or local candidate address or both.
      // Combine those with both; these will be the peer candidates.
      const matched = {};

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
      } of report.iceCandidatePairStats) {
        const local = candidates[localCandidateId];
        if (local) {
          const stat = {
            ["local-candidate"]: candidateToString(local),
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

          const remote = candidates[remoteCandidateId];
          if (remote) {
            stat["remote-candidate"] = candidateToString(remote);
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
      stats.sort((a, b) => {
        if (a.componentId != b.componentId) {
          return a.componentId - b.componentId;
        }
        return b.bytesSent
          ? b.bytesSent - (a.bytesSent || 0)
          : (b.priority || 0) - (a.priority || 0);
      });
    }
    // Render ICE stats
    // don't use |stat.x || ""| here because it hides 0 values
    const statsTable = renderSimpleTable(
      caption,
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
      ].map(columnName => string(columnName)),
      stats.map(stat =>
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
      )
    );

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
        cells[1].className = "ice-succeeded";
      }
      if (selected) {
        cells[2].className = "ice-succeeded";
      }
      if (localTrickled) {
        cells[3].className = "ice-trickled";
      }
      if (remoteTrickled) {
        cells[4].className = "ice-trickled";
      }
    }

    // if the current row's component id changes, mark the bottom of the
    // previous row with a thin, black border to differentiate the
    // component id grouping.
    let previousRow;
    for (const row of statsTable.rows) {
      if (previousRow) {
        if (previousRow.cells[5].innerHTML != row.cells[5].innerHTML) {
          previousRow.className = "bottom-border";
        }
      }
      previousRow = row;
    }
    iceDiv.append(statsTable);
  }
  // add just a bit of vertical space between the restart/rollback
  // counts and the ICE candidate pair table above.
  iceDiv.append(
    renderElement("br"),
    renderIceMetric("ice_restart_count_label", report.iceRestarts),
    renderIceMetric("ice_rollback_count_label", report.iceRollbacks)
  );

  // Render raw ICECandidate section
  {
    const section = renderElements("div", {}, [
      renderText("h4", string("raw_candidates_heading")),
    ]);
    const foldSection = renderFoldableSection(section, {
      showMsg: string("raw_cand_show_msg"),
      hideMsg: string("raw_cand_hide_msg"),
    });

    // render raw candidates
    foldSection.append(
      renderElements("div", {}, [
        renderRawIceTable("raw_local_candidate", report.rawLocalCandidates),
        renderRawIceTable("raw_remote_candidate", report.rawRemoteCandidates),
      ])
    );
    section.append(foldSection);
    iceDiv.append(section);
  }
  return iceDiv;
}

function renderIceMetric(label, value) {
  return renderElement("div", {}, [
    renderText("span", `${string(label)}: `, { className: "info-label" }),
    renderText("span", value, { className: "info-body" }),
  ]);
}

function candidateToString({
  type,
  address,
  port,
  protocol,
  candidateType,
  relayProtocol,
  proxied,
} = {}) {
  if (!type) {
    return "*";
  }
  if (relayProtocol) {
    candidateType = `${candidateType}-${relayProtocol}`;
  }
  proxied = type == "local-candidate" ? ` [${proxied}]` : "";
  return `${address}:${port}/${protocol}(${candidateType})${proxied}`;
}

function renderUserPrefs() {
  const getPref = key => {
    switch (Services.prefs.getPrefType(key)) {
      case Services.prefs.PREF_BOOL:
        return Services.prefs.getBoolPref(key);
      case Services.prefs.PREF_INT:
        return Services.prefs.getIntPref(key);
      case Services.prefs.PREF_STRING:
        return Services.prefs.getStringPref(key);
    }
    return "";
  };
  const prefs = [
    "media.peerconnection",
    "media.navigator",
    "media.getusermedia",
  ];
  const renderPref = p => renderText("p", `${p}: ${getPref(p)}`);
  const display = prefs
    .flatMap(Services.prefs.getChildList)
    .filter(Services.prefs.prefHasUserValue)
    .map(renderPref);
  return renderElements(
    "div",
    {
      id: "prefs",
      className: "prefs",
      style: display.length ? "" : "visibility:hidden",
    },
    [
      renderElements("span", { className: "section-heading" }, [
        renderText("h3", string("custom_webrtc_configuration_heading")),
      ]),
      ...display,
    ]
  );
}

function renderFoldableSection(parent, options = {}) {
  const section = renderElement("div");
  if (parent) {
    const ctrl = renderElements("div", { className: "section-ctrl no-print" }, [
      new FoldEffect(section, options).render(),
    ]);
    parent.append(ctrl);
  }
  return section;
}

function renderSimpleTable(caption, headings, data) {
  const heads = headings.map(text => renderText("th", text));
  const renderCell = text => renderText("td", text);

  return renderElements("table", {}, [
    caption,
    renderElements("tr", {}, heads),
    ...data.map(line => renderElements("tr", {}, line.map(renderCell))),
  ]);
}

class FoldEffect {
  static allSections = [];

  constructor(
    target,
    {
      showMsg = string("fold_show_msg"),
      showHint = string("fold_show_hint"),
      hideMsg = string("fold_hide_msg"),
      hideHint = string("fold_hide_hint"),
    } = {}
  ) {
    showMsg = `\u25BC ${showMsg}`;
    hideMsg = `\u25B2 ${hideMsg}`;
    Object.assign(this, { target, showMsg, showHint, hideMsg, hideHint });
  }

  render() {
    this.target.classList.add("fold-target");
    this.trigger = renderElement("div", { className: "fold-trigger" });
    this.collapse();
    this.trigger.onclick = () => {
      if (this.target.classList.contains("fold-closed")) {
        this.expand();
      } else {
        this.collapse();
      }
    };
    FoldEffect.allSections.push(this);
    return this.trigger;
  }

  expand() {
    this.target.classList.remove("fold-closed");
    this.trigger.setAttribute("title", this.hideHint);
    this.trigger.textContent = this.hideMsg;
  }

  collapse() {
    this.target.classList.add("fold-closed");
    this.trigger.setAttribute("title", this.showHint);
    this.trigger.textContent = this.showMsg;
  }

  static expandAll() {
    for (const section of FoldEffect.allSections) {
      section.expand();
    }
  }

  static collapseAll() {
    for (const section of FoldEffect.allSections) {
      section.collapse();
    }
  }
}
