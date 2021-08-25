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

const WGI = WebrtcGlobalInformation;

const LOGFILE_NAME_DEFAULT = "aboutWebrtc.html";
const WEBRTC_TRACE_ALL = 65535;

async function getStats() {
  const { reports } = await new Promise(r => WGI.getAllStats(r));
  return [...reports].sort((a, b) => b.timestamp - a.timestamp);
}

const getLog = () => new Promise(r => WGI.getLogging("", r));

const renderElement = (name, options, l10n_id, l10n_args) => {
  let elem = Object.assign(document.createElement(name), options);
  if (l10n_id) {
    document.l10n.setAttributes(elem, l10n_id, l10n_args);
  }
  return elem;
};

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
  messageArgs = null;
  messageHeader = null;

  render() {
    this.ctrl = renderElement(
      "button",
      { onclick: () => this.onClick() },
      this.label
    );
    this.msg = renderElement("p");
    this.update();
    return [this.ctrl, this.msg];
  }

  update() {
    document.l10n.setAttributes(this.ctrl, this.label);
    this.msg.textContent = "";
    if (this.message) {
      this.msg.append(
        renderElement(
          "span",
          {
            className: "info-label",
          },
          this.messageHeader
        ),
        renderElement(
          "span",
          {
            className: "info-body",
          },
          this.message,
          this.messageArgs
        )
      );
    }
  }
}

class SavePage extends Control {
  constructor() {
    super();
    this.messageHeader = "about-webrtc-save-page-label";
    this.label = "about-webrtc-save-page-label";
  }

  async onClick() {
    FoldEffect.expandAll();
    let [dialogTitle] = await document.l10n.formatValues([
      { id: "about-webrtc-save-page-dialog-title" },
    ]);
    FilePicker.init(window, dialogTitle, FilePicker.modeSave);
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
    this.message = "about-webrtc-save-page-msg";
    this.messageArgs = { path: FilePicker.file.path };
    this.update();
  }
}

class DebugMode extends Control {
  constructor() {
    super();
    this.messageHeader = "about-webrtc-debug-mode-msg-label";

    if (WGI.debugLevel > 0) {
      this.setState(true);
    } else {
      this.label = "about-webrtc-debug-mode-off-state-label";
    }
  }

  setState(state) {
    this.label = state
      ? "about-webrtc-debug-mode-on-state-label"
      : "about-webrtc-debug-mode-off-state-label";
    try {
      const file = Services.prefs.getCharPref("media.webrtc.debug.log_file");
      this.message = state
        ? "about-webrtc-debug-mode-on-state-msg"
        : "about-webrtc-debug-mode-off-state-msg";
      this.messageArgs = { path: file };
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
    this.messageHeader = "about-webrtc-aec-logging-msg-label";

    if (WGI.aecDebug) {
      this.setState(true);
    } else {
      this.label = "about-webrtc-aec-logging-off-state-label";
      this.message = null;
    }
  }

  setState(state) {
    this.label = state
      ? "about-webrtc-aec-logging-on-state-label"
      : "about-webrtc-aec-logging-off-state-label";
    try {
      if (!state) {
        const file = WGI.aecDebugLogDir;
        this.message = "about-webrtc-aec-logging-off-state-msg";
        this.messageArgs = { path: file };
      } else {
        this.message = "about-webrtc-aec-logging-on-state-msg";
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

class ShowTab extends Control {
  constructor(browserId) {
    super();
    this.label = "about-webrtc-show-tab-label";
    this.message = null;
    this.browserId = browserId;
  }

  onClick() {
    const gBrowser =
      window.ownerGlobal.browsingContext.topChromeWindow.gBrowser;
    for (const tab of gBrowser.visibleTabs) {
      if (tab.linkedBrowser && tab.linkedBrowser.browserId == this.browserId) {
        gBrowser.selectedTab = tab;
        return;
      }
    }
    this.ctrl.disabled = true;
  }
}

(async () => {
  // Setup. Retrieve reports & log while page loads.
  const haveReports = getStats();
  const haveLog = getLog();
  await new Promise(r => (window.onload = r));
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

  reports.sort((a, b) => a.browserId - b.browserId);

  let peerConnections = renderElement("div");
  let connectionLog = renderElement("div");
  let userPrefs = renderElement("div");

  const content = document.querySelector("#content");
  content.append(peerConnections, connectionLog, userPrefs);

  // This does not handle the auto-refresh, only the manual refreshes needed
  // for certain user actions, and the initial population of the data
  function refresh() {
    const pcDiv = renderElements("div", { className: "stats" }, [
      renderElements("span", { className: "section-heading" }, [
        renderElement("h3", {}, "about-webrtc-stats-heading"),
        renderElement(
          "button",
          {
            className: "no-print",
            onclick: async () => {
              WGI.clearAllStats();
              reports = await getStats();
              refresh();
            },
          },
          "about-webrtc-stats-clear"
        ),
      ]),
      ...reports.map(renderPeerConnection),
    ]);
    const logDiv = renderElements("div", { className: "log" }, [
      renderElements("span", { className: "section-heading" }, [
        renderElement("h3", {}, "about-webrtc-log-heading"),
        renderElement(
          "button",
          {
            className: "no-print",
            onclick: async () => {
              WGI.clearLogging();
              log = await getLog();
              refresh();
            },
          },
          "about-webrtc-log-clear"
        ),
      ]),
    ]);
    if (log.length) {
      const div = renderFoldableSection(logDiv, {
        showMsg: "about-webrtc-log-show-msg",
        hideMsg: "about-webrtc-log-hide-msg",
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

  async function translate(element) {
    const frag = document.createDocumentFragment();
    frag.append(element);
    await document.l10n.translateFragment(frag);
    return frag;
  }

  window.setInterval(
    async history => {
      const reports = await getStats();

      const translateSection = async (report, id, renderFunc) => {
        const element = document.getElementById(`${id}: ${report.pcid}`);
        const result =
          element && (await translate(renderFunc(report, history)));
        return { element, translated: result };
      };

      const sections = (
        await Promise.all(
          reports.flatMap(report => [
            translateSection(report, "ice-stats", renderICEStats),
            translateSection(report, "rtp-stats", renderRTPStats),
            translateSection(report, "bandwidth-stats", renderBandwidthStats),
            translateSection(report, "frame-stats", renderFrameRateStats),
          ])
        )
      ).filter(({ element }) => element);

      document.l10n.pauseObserving();
      for (const { element, translated } of sections) {
        element.replaceWith(translated);
      }
      document.l10n.resumeObserving();
    },
    500,
    {}
  );
})();

function renderPeerConnection(report) {
  const { pcid, browserId, closed, timestamp, configuration } = report;

  const pcDiv = renderElement("div", { className: "peer-connection" });
  {
    const id = pcid.match(/id=(\S+)/)[1];
    const url = pcid.match(/url=([^)]+)/)[1];
    const now = new Date(timestamp);

    pcDiv.append(
      closed
        ? renderElement("h3", {}, "about-webrtc-connection-closed", {
            "browser-id": browserId,
            id,
            url,
            now,
          })
        : renderElement("h3", {}, "about-webrtc-connection-open", {
            "browser-id": browserId,
            id,
            url,
            now,
          })
    );
    pcDiv.append(new ShowTab(browserId).render()[0]);
  }
  {
    const section = renderFoldableSection(pcDiv);
    section.append(
      renderElements("div", {}, [
        renderElement(
          "span",
          {
            className: "info-label",
          },
          "about-webrtc-peerconnection-id-label"
        ),
        renderText("span", pcid, { className: "info-body" }),
      ]),
      renderConfiguration(configuration),
      renderICEStats(report),
      renderSDPStats(report),
      renderBandwidthStats(report),
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
    renderElement("h4", {}, "about-webrtc-sdp-heading"),
    renderElement(
      "h5",
      {},
      offerer
        ? "about-webrtc-local-sdp-heading-offer"
        : "about-webrtc-local-sdp-heading-answer"
    ),
    renderText("pre", trimNewlines(localSdp)),
    renderElement(
      "h5",
      {},
      offerer
        ? "about-webrtc-remote-sdp-heading-answer"
        : "about-webrtc-remote-sdp-heading-offer"
    ),
    renderText("pre", trimNewlines(remoteSdp)),
    renderElement("h4", {}, "about-webrtc-sdp-history-heading"),
  ]);

  // All SDP in sequential order. Add onclick handler to scroll the associated
  // SDP into view below.
  for (const { isLocal, timestamp } of sdpHistory) {
    const histDiv = renderElement("div", {});
    const text = renderElement(
      "h5",
      { className: "sdp-history-link" },
      isLocal
        ? "about-webrtc-sdp-set-at-timestamp-local"
        : "about-webrtc-sdp-set-at-timestamp-remote",
      { timestamp }
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
    renderElement("h4", {}, "about-webrtc-local-sdp-heading"),
  ]);
  const remoteDiv = renderElements("div", {}, [
    renderElement("h4", {}, "about-webrtc-remote-sdp-heading"),
  ]);

  let first = NaN;
  for (const { isLocal, timestamp, sdp, errors } of sdpHistory) {
    if (isNaN(first)) {
      first = timestamp;
    }
    const histDiv = isLocal ? localDiv : remoteDiv;
    histDiv.append(
      renderElement(
        "h5",
        { id: "sdp-history: " + timestamp },
        "about-webrtc-sdp-set-timestamp",
        { timestamp, "relative-timestamp": timestamp - first }
      )
    );
    if (errors.length) {
      histDiv.append(
        renderElement("h5", {}, "about-webrtc-sdp-parsing-errors-heading")
      );
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

function renderBandwidthStats(report) {
  const statsDiv = renderElement("div", {
    id: "bandwidth-stats: " + report.pcid,
  });
  const table = renderSimpleTable(
    "",
    [
      "about-webrtc-track-identifier",
      "about-webrtc-send-bandwidth-bytes-sec",
      "about-webrtc-receive-bandwidth-bytes-sec",
      "about-webrtc-max-padding-bytes-sec",
      "about-webrtc-pacer-delay-ms",
      "about-webrtc-round-trip-time-ms",
    ],
    report.bandwidthEstimations.map(stat => [
      stat.trackIdentifier,
      stat.sendBandwidthBps,
      stat.receiveBandwidthBps,
      stat.maxPaddingBps,
      stat.pacerDelayMs,
      stat.rttMs,
    ])
  );
  statsDiv.append(
    renderElement("h4", {}, "about-webrtc-bandwidth-stats-heading"),
    table
  );
  return statsDiv;
}

function renderFrameRateStats(report) {
  const statsDiv = renderElement("div", { id: "frame-stats: " + report.pcid });
  report.videoFrameHistories.forEach(history => {
    const stats = history.entries.map(stat => {
      stat.elapsed = stat.lastFrameTimestamp - stat.firstFrameTimestamp;
      if (stat.elapsed < 1) {
        stat.elapsed = "0.00";
      }
      stat.elapsed = (stat.elapsed / 1_000).toFixed(3);
      if (stat.elapsed && stat.consecutiveFrames) {
        stat.avgFramerate = (stat.consecutiveFrames / stat.elapsed).toFixed(2);
      } else {
        stat.avgFramerate = "0.00";
      }
      return stat;
    });

    const table = renderSimpleTable(
      "",
      [
        "about-webrtc-width-px",
        "about-webrtc-height-px",
        "about-webrtc-consecutive-frames",
        "about-webrtc-time-elapsed",
        "about-webrtc-estimated-framerate",
        "about-webrtc-rotation-degrees",
        "about-webrtc-first-frame-timestamp",
        "about-webrtc-last-frame-timestamp",
        "about-webrtc-local-receive-ssrc",
        "about-webrtc-remote-send-ssrc",
      ],
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
      renderElement("h4", {}, "about-webrtc-frame-stats-heading", {
        "track-identifier": history.trackIdentifier,
      }),
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
    renderElement("h4", {}, "about-webrtc-rtp-stats-heading"),
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
  let elements = [];

  if (bitrateMean) {
    elements.push(
      renderElement(
        "span",
        { className: "stat-label" },
        "about-webrtc-avg-bitrate-label"
      )
    );
    elements.push(
      renderText("span", ` ${(bitrateMean / 1000000).toFixed(2)}`, {})
    );

    if (bitrateStdDev) {
      elements.push(
        renderText("span", ` (${(bitrateStdDev / 1000000).toFixed(2)} SD)`, {})
      );
    }
  }
  if (bitrateMean) {
    elements.push(
      renderElement(
        "span",
        { className: "stat-label" },
        "about-webrtc-avg-framerate-label"
      )
    );
    elements.push(
      renderText("span", ` ${(framerateMean / 1000000).toFixed(2)}`, {})
    );

    if (framerateStdDev) {
      elements.push(
        renderText(
          "span",
          ` (${(framerateStdDev / 1000000).toFixed(2)} SD)`,
          {}
        )
      );
    }
  }
  if (droppedFrames) {
    elements.push(
      renderElement(
        "span",
        { className: "stat-label" },
        "about-webrtc-dropped-frames-label"
      )
    );
    elements.push(renderText("span", ` ${droppedFrames}`, {}));
  }
  if (discardedPackets) {
    elements.push(
      renderElement(
        "span",
        { className: "stat-label" },
        "about-webrtc-discarded-packets-label"
      )
    );
    elements.push(renderText("span", ` ${discardedPackets}`, {}));
  }
  if (elements.length) {
    if (packetsReceived) {
      elements.unshift(
        renderElement("span", {}, "about-webrtc-decoder-label"),
        renderText("span", ": ")
      );
    } else {
      elements.unshift(
        renderElement("span", {}, "about-webrtc-encoder-label"),
        renderText("span", ": ")
      );
    }
  }
  return renderElements("div", {}, elements);
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
  if (history) {
    if (history[id] === undefined) {
      history[id] = {};
    }
  }

  const estimateKbps = (timestamp, lastTimestamp, bytes, lastBytes) => {
    if (!timestamp || !lastTimestamp || !bytes || !lastBytes) {
      return "0.0";
    }
    const elapsedTime = timestamp - lastTimestamp;
    if (elapsedTime <= 0) {
      return "0.0";
    }
    return ((bytes - lastBytes) / elapsedTime).toFixed(1);
  };

  let elements = [];

  if (local) {
    elements.push(
      renderElement("span", {}, "about-webrtc-type-local"),
      renderText("span", ": ")
    );
  } else {
    elements.push(
      renderElement("span", {}, "about-webrtc-type-remote"),
      renderText("span", ": ")
    );
  }

  const time = new Date(timestamp).toTimeString();
  elements.push(renderText("span", `${time} ${type} SSRC: ${ssrc}`));

  if (packetsReceived) {
    elements.push(
      renderElement(
        "span",
        { className: "stat-label" },
        "about-webrtc-received-label",
        {
          packets: packetsReceived,
        }
      )
    );

    if (bytesReceived) {
      let s = ` (${(bytesReceived / 1024).toFixed(2)} Kb`;
      if (local && history) {
        s += ` , ${estimateKbps(
          timestamp,
          history[id].lastTimestamp,
          bytesReceived,
          history[id].lastBytesReceived
        )} Kbps`;
      }
      s += ")";
      elements.push(renderText("span", s));
    }

    elements.push(
      renderElement(
        "span",
        { className: "stat-label" },
        "about-webrtc-lost-label",
        {
          packets: packetsLost,
        }
      )
    );
    elements.push(
      renderElement(
        "span",
        { className: "stat-label" },
        "about-webrtc-jitter-label",
        {
          jitter,
        }
      )
    );

    if (roundTripTime) {
      elements.push(renderText("span", ` RTT: ${roundTripTime * 1000} ms`));
    }
  } else if (packetsSent) {
    elements.push(
      renderElement(
        "span",
        { className: "stat-label" },
        "about-webrtc-sent-label",
        {
          packets: packetsSent,
        }
      )
    );
    if (bytesSent) {
      let s = ` (${(bytesSent / 1024).toFixed(2)} Kb`;
      if (local && history) {
        s += `, ${estimateKbps(
          timestamp,
          history[id].lastTimestamp,
          bytesSent,
          history[id].lastBytesSent
        )} Kbps`;
      }
      s += ")";
      elements.push(renderText("span", s));
    }
  }

  // Update history
  if (history) {
    history[id].lastBytesReceived = bytesReceived;
    history[id].lastBytesSent = bytesSent;
    history[id].lastTimestamp = timestamp;
  }

  return renderElements("div", {}, elements);
}

function renderRawIceTable(caption, candidates) {
  const table = renderSimpleTable(
    "",
    [caption],
    [...new Set(candidates.sort())].filter(i => i).map(i => [i])
  );
  table.className = "raw-candidate";
  return table;
}

function renderConfiguration(c) {
  const provided = "about-webrtc-configuration-element-provided";
  const notProvided = "about-webrtc-configuration-element-not-provided";

  // Create the text for a configuration field
  const cfg = (obj, key) => [
    renderElement("br"),
    `${key}: `,
    key in obj ? obj[key] : renderElement("i", {}, notProvided),
  ];

  // Create the text for a fooProvided configuration field
  const pro = (obj, key) => [
    renderElement("br"),
    `${key}(`,
    renderElement("i", {}, provided),
    `/`,
    renderElement("i", {}, notProvided),
    `): `,
    renderElement("i", {}, obj[`${key}Provided`] ? provided : notProvided),
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
      ? [renderElement("i", {}, notProvided)]
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
    renderElement("h4", {}, "about-webrtc-ice-stats-heading"),
  ]);

  // Render ICECandidate table
  {
    const caption = renderElement(
      "caption",
      { className: "no-print" },
      "about-webrtc-trickle-caption-msg"
    );

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
        "about-webrtc-ice-state",
        "about-webrtc-nominated",
        "about-webrtc-selected",
        "about-webrtc-local-candidate",
        "about-webrtc-remote-candidate",
        "about-webrtc-ice-component-id",
        "about-webrtc-priority",
        "about-webrtc-ice-pair-bytes-sent",
        "about-webrtc-ice-pair-bytes-received",
      ],
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
  // restart/rollback counts.
  iceDiv.append(
    renderIceMetric("about-webrtc-ice-restart-count-label", report.iceRestarts),
    renderIceMetric(
      "about-webrtc-ice-rollback-count-label",
      report.iceRollbacks
    )
  );

  // Render raw ICECandidate section
  {
    const section = renderElements("div", {}, [
      renderElement("h4", {}, "about-webrtc-raw-candidates-heading"),
    ]);
    const foldSection = renderFoldableSection(section, {
      showMsg: "about-webrtc-raw-cand-show-msg",
      hideMsg: "about-webrtc-raw-cand-hide-msg",
    });

    // render raw candidates
    foldSection.append(
      renderElements("div", {}, [
        renderRawIceTable(
          "about-webrtc-raw-local-candidate",
          report.rawLocalCandidates
        ),
        renderRawIceTable(
          "about-webrtc-raw-remote-candidate",
          report.rawRemoteCandidates
        ),
      ])
    );
    section.append(foldSection);
    iceDiv.append(section);
  }
  return iceDiv;
}

function renderIceMetric(label, value) {
  return renderElements("div", {}, [
    renderElement("span", { className: "info-label" }, label),
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
        renderElement(
          "h3",
          {},
          "about-webrtc-custom-webrtc-configuration-heading"
        ),
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
  const heads = headings.map(text => renderElement("th", {}, text));
  const renderCell = text => renderText("td", text);

  return renderElements("table", {}, [
    caption,
    renderElements("tr", {}, heads),
    ...data.map(line => renderElements("tr", {}, line.map(renderCell))),
  ]);
}

class FoldEffect {
  constructor(
    target,
    {
      showMsg = "about-webrtc-fold-show-msg",
      hideMsg = "about-webrtc-fold-hide-msg",
    } = {}
  ) {
    Object.assign(this, { target, showMsg, hideMsg });
  }

  render() {
    this.target.classList.add("fold-target");
    this.trigger = renderElement("div", { className: "fold-trigger" });
    this.trigger.classList.add(this.showMsg, this.hideMsg);
    this.collapse();
    this.trigger.onclick = () => {
      if (this.target.classList.contains("fold-closed")) {
        this.expand();
      } else {
        this.collapse();
      }
    };
    return this.trigger;
  }

  expand() {
    this.target.classList.remove("fold-closed");
    document.l10n.setAttributes(this.trigger, this.hideMsg);
  }

  collapse() {
    this.target.classList.add("fold-closed");
    document.l10n.setAttributes(this.trigger, this.showMsg);
  }

  static expandAll() {
    for (const target of document.getElementsByClassName("fold-closed")) {
      target.classList.remove("fold-closed");
    }
    for (const trigger of document.getElementsByClassName("fold-trigger")) {
      const hideMsg = trigger.classList[2];
      document.l10n.setAttributes(trigger, hideMsg);
    }
  }

  static collapseAll() {
    for (const target of document.getElementsByClassName("fold-target")) {
      target.classList.add("fold-closed");
    }
    for (const trigger of document.getElementsByClassName("fold-trigger")) {
      const showMsg = trigger.classList[1];
      document.l10n.setAttributes(trigger, showMsg);
    }
  }
}
