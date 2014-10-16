/** @jsx React.DOM */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Tabs = React.createClass({displayName: 'Tabs',
  getDefaultProps: function() {
    return {selectedIndex: 0};
  },

  getInitialState: function() {
    return {selectedIndex: this.props.selectedIndex};
  },

  selectTab: function(index) {
    return function(event) {
      event.preventDefault();
      this.setState({selectedIndex: index});
    }.bind(this);
  },

  _findSelectedTabContent: function() {
    // Using map() to filter children…
    // https://github.com/facebook/react/issues/1644#issuecomment-45138113
    return React.Children.map(this.props.children, function(tab, i) {
      return i === this.state.selectedIndex ? tab : null;
    }.bind(this))
  },

  render: function() {
    var cx = React.addons.classSet;
    return (
      React.DOM.div({className: "tabs"}, 
        React.DOM.ul(null, 
          React.Children.map(this.props.children, function(tab, i) {
            return (
              React.DOM.li({className: cx({active: i === this.state.selectedIndex})}, 
                React.DOM.a({href: "#", key: i, onClick: this.selectTab(i)}, 
                  tab.props.title
                )
              )
            );
          }.bind(this))
        ), 
        this._findSelectedTabContent()
      )
    );
  }
});

var Tab = React.createClass({displayName: 'Tab',
  render: function() {
    return React.DOM.section(null, this.props.children);
  }
});

var AboutWebRTC = React.createClass({displayName: 'AboutWebRTC',
  getInitialState: function() {
    return {logs: null, reports: this.props.reports};
  },

  displayLogs: function() {
    WebrtcGlobalInformation.getLogging('', function(logs) {
      this.setState({logs: logs, reports: this.state.reports});
    }.bind(this))
  },

  render: function() {
    return (
      React.DOM.div(null, 
        React.DOM.div({id: "stats"}, 
          PeerConnections({reports: this.state.reports})
        ), 
        React.DOM.div({id: "buttons"}, 
          LogsButton({handler: this.displayLogs}), 
          DebugButton(null), 
          AECButton(null)
        ), 
        React.DOM.div({id: "logs"}, 
          this.state.logs ? Logs({logs: this.state.logs}) : null
        )
      )
    );
  }
});

var PeerConnections = React.createClass({displayName: 'PeerConnections',
  getInitialState: function() {
    // Sort the reports to have the more recent at the top
    var reports = this.props.reports.slice().sort(function(r1, r2) {
      return r2.timestamp - r1.timestamp;
    });

    return {reports: reports};
  },

  render: function() {
    return (
      React.DOM.div({className: "peer-connections"}, 
        
          this.state.reports.map(function(report, i) {
            return PeerConnection({key: i, report: report});
          })
        
      )
    );
  }
});

var PeerConnection = React.createClass({displayName: 'PeerConnection',
  getPCInfo: function(report) {
    return {
      id: report.pcid.match(/id=(\S+)/)[1],
      url: report.pcid.match(/http[^)]+/)[0],
      closed: report.closed
    };
  },

  getIceCandidatePairs: function(report) {
    var candidates =
      report.iceCandidateStats.reduce(function(candidates, candidate) {
        candidates[candidate.id] = candidate;

        return candidates;
      }, {});

    var pairs = report.iceCandidatePairStats.map(function(pair) {
      var localCandidate = candidates[pair.localCandidateId];
      var remoteCandidate = candidates[pair.remoteCandidateId];

      return {
        localCandidate: candidateToString(localCandidate),
        remoteCandidate: candidateToString(remoteCandidate),
        state: pair.state,
        priority: pair.mozPriority,
        nominated: pair.nominated,
        selected: pair.selected
      };
    });

    var pairedCandidates = pairs.reduce(function(paired, pair) {
      paired.add(pair.localCandidate.id);
      paired.add(pair.remoteCandidate.id);

      return paired
    }, new Set());

    var unifiedPairs =
      report.iceCandidateStats.reduce(function(pairs, candidate) {
        if (pairedCandidates.has(candidate)) {
            return pairs;
        }

        var isLocal = candidate.type === "localcandidate";

        pairs.push({
          localCandidate:  isLocal ? candidateToString(candidate) : null,
          remoteCandidate: isLocal ? null : candidateToString(candidate),
          state: null,
          priority: null,
          nominated: null,
          selected: null
        });

        return pairs;
      }, pairs);

    return unifiedPairs;
  },

  getInitialState: function() {
    return {
      unfolded: false
    };
  },

  onFold: function() {
    this.setState({unfolded: !this.state.unfolded});
  },

  body: function(report, pairs) {
    return (
      React.DOM.div(null, 
        React.DOM.p({className: "pcid"}, "PeerConnection ID: ", report.pcid), 
        Tabs(null, 
          Tab({title: "Ice Stats"}, 
            IceStats({pairs: pairs})
          ), 
          Tab({title: "SDP"}, 
            SDP({type: "local", sdp: report.localSdp}), 
            SDP({type: "remote", sdp: report.remoteSdp})
          ), 
          Tab({title: "RTP Stats"}, 
            RTPStats({report: report})
          )
        )
      )
    );
  },

  render: function() {
    var report = this.props.report;
    var pcInfo = this.getPCInfo(report);
    var pairs  = this.getIceCandidatePairs(report);

    return (
      React.DOM.div({className: "peer-connection"}, 
        React.DOM.h3({onClick: this.onFold}, 
          "[", pcInfo.id, "] ", pcInfo.url, " ", pcInfo.closed ? "(closed)" : null, 
          new Date(report.timestamp).toTimeString()
        ), 
        this.state.unfolded ? this.body(report, pairs) : undefined
      )
    );
  }
});

var IceStats = React.createClass({displayName: 'IceStats',
  sortHeadings: {
    "Local candidate":  "localCandidate",
    "Remote candidate": "remoteCandidate",
    "Ice State":        "state",
    "Priority":         "priority",
    "Nominated":        "nominated",
    "Selected":         "selected"
  },

  sort: function(key) {
    var sorting = this.state.sorting;
    var pairs = this.state.pairs.slice().sort(function(pair1, pair2) {
      var value1 = pair1[key] ? pair1[key].toString() : "";
      var value2 = pair2[key] ? pair2[key].toString() : "";

      // Reverse sorting
      if (key === sorting) {
        return value2.localeCompare(value1);
      }

      return value1.localeCompare(value2);
    }.bind(this));

    sorting = (key === sorting) ? null : key;
    this.setState({pairs: pairs, sorting: sorting});
  },

  generateSortHeadings: function() {
    return Object.keys(this.sortHeadings).map(function(heading, i) {
      var sortKey = this.sortHeadings[heading];
      return (
        React.DOM.th(null, 
          React.DOM.a({href: "#", onClick: this.sort.bind(this, sortKey)}, 
            heading
          )
        )
      );
    }.bind(this));
  },

  getInitialState: function() {
    return {pairs: this.props.pairs, sorting: null};
  },

  render: function() {
    return (
      React.DOM.table(null, 
        React.DOM.tbody(null, 
          React.DOM.tr(null, this.generateSortHeadings()), 
          this.state.pairs.map(function(pair, i) {
            return IceCandidatePair({key: i, pair: pair});
          })
        )
      )
    );
  }
});

var IceCandidatePair = React.createClass({displayName: 'IceCandidatePair',
  render: function() {
    var pair = this.props.pair;
    return (
      React.DOM.tr(null, 
        React.DOM.td(null, pair.localCandidate), 
        React.DOM.td(null, pair.remoteCandidate), 
        React.DOM.td(null, pair.state), 
        React.DOM.td(null, pair.priority), 
        React.DOM.td(null, pair.nominated ? '✔' : null), 
        React.DOM.td(null, pair.selected ? '✔' : null)
      )
    );
  }
});

var SDP = React.createClass({displayName: 'SDP',
  render: function() {
    var type = labelize(this.props.type);

    return (
      React.DOM.div(null, 
        React.DOM.h4(null, type, " SDP"), 
        React.DOM.pre(null, 
          this.props.sdp
        )
      )
    );
  }
});

var RTPStats = React.createClass({displayName: 'RTPStats',
  getRtpStats: function(report) {
    var remoteRtpStats = {};
    var rtpStats = [];

    rtpStats = rtpStats.concat(report.inboundRTPStreamStats  || []);
    rtpStats = rtpStats.concat(report.outboundRTPStreamStats || []);

    rtpStats.forEach(function(stats) {
      if (stats.isRemote) {
        remoteRtpStats[stats.id] = stats;
      }
    });

    rtpStats.forEach(function(stats) {
      if (stats.remoteId) {
        stats.remoteRtpStats = remoteRtpStats[stats.remoteId];
      }
    });

    return rtpStats;
  },

  dumpAvStats: function(stats) {
    var statsString = "";
    if (stats.mozAvSyncDelay) {
      statsString += `A/V sync: ${stats.mozAvSyncDelay} ms `;
    }
    if (stats.mozJitterBufferDelay) {
      statsString += `Jitter-buffer delay: ${stats.mozJitterBufferDelay} ms`;
    }

    return React.DOM.div(null, statsString);
  },

  dumpCoderStats: function(stats) {
    var statsString = "";
    var label;

    if (stats.bitrateMean) {
      statsString += ` Avg. bitrate: ${(stats.bitrateMean/1000000).toFixed(2)} Mbps`;
      if (stats.bitrateStdDev) {
        statsString += ` (${(stats.bitrateStdDev/1000000).toFixed(2)} SD)`;
      }
    }

    if (stats.framerateMean) {
      statsString += ` Avg. framerate: ${(stats.framerateMean).toFixed(2)} fps`;
      if (stats.framerateStdDev) {
        statsString += ` (${stats.framerateStdDev.toFixed(2)} SD)`;
      }
    }

    if (stats.droppedFrames) {
      statsString += ` Dropped frames: ${stats.droppedFrames}`;
    }
    if (stats.discardedPackets) {
      statsString += ` Discarded packets: ${stats.discardedPackets}`;
    }

    if (statsString) {
      label = (stats.packetsReceived)? " Decoder:" : " Encoder:";
      statsString = label + statsString;
    }

    return React.DOM.div(null, statsString);
  },

  dumpRtpStats: function(stats, type) {
    var label = labelize(type);
    var time  = new Date(stats.timestamp).toTimeString();

    var statsString = `${label}: ${time} ${stats.type} SSRC: ${stats.ssrc}`;

    if (stats.packetsReceived) {
      statsString += ` Received: ${stats.packetsReceived} packets`;

      if (stats.bytesReceived) {
        statsString += ` (${round00(stats.bytesReceived/1024)} Kb)`;
      }

      statsString += ` Lost: ${stats.packetsLost} Jitter: ${stats.jitter}`;

      if (stats.mozRtt) {
        statsString += ` RTT: ${stats.mozRtt} ms`;
      }
    } else if (stats.packetsSent) {
      statsString += ` Sent: ${stats.packetsSent} packets`;
      if (stats.bytesSent) {
        statsString += ` (${round00(stats.bytesSent/1024)} Kb)`;
      }
    }

    return React.DOM.div(null, statsString);
  },

  render: function() {
    var rtpStats = this.getRtpStats(this.props.report);

    return (
      React.DOM.div(null, 
        rtpStats.map(function(stats) {
          var isAvStats = (stats.mozAvSyncDelay || stats.mozJitterBufferDelay);
          var remoteRtpStats = stats.remoteId ? stats.remoteRtpStats : null;

          return [
            React.DOM.h5(null, stats.id),
            isAvStats ? this.dumpAvStats(stats) : null,
            this.dumpCoderStats(stats),
            this.dumpRtpStats(stats, "local"),
            remoteRtpStats ? this.dumpRtpStats(remoteRtpStats, "remote") : null
          ]
        }.bind(this))
      )
    );
  }
});

var LogsButton = React.createClass({displayName: 'LogsButton',
  render: function() {
    return (
      React.DOM.button({onClick: this.props.handler}, "Connection log")
    );
  }
});

var DebugButton = React.createClass({displayName: 'DebugButton',
  getInitialState: function() {
    var on = (WebrtcGlobalInformation.debugLevel > 0);
    return {on: on};
  },

  onClick: function() {
    if (this.state.on)
      WebrtcGlobalInformation.debugLevel = 0;
    else
      WebrtcGlobalInformation.debugLevel = 65535;

    this.setState({on: !this.state.on});
  },

  render: function() {
    return (
      React.DOM.button({onClick: this.onClick}, 
        this.state.on ? "Stop debug mode" : "Start debug mode"
      )
    );
  }
});

var AECButton = React.createClass({displayName: 'AECButton',
  getInitialState: function() {
    return {on: WebrtcGlobalInformation.aecDebug};
  },

  onClick: function() {
    WebrtcGlobalInformation.aecDebug = !this.state.on;
    this.setState({on: WebrtcGlobalInformation.aecDebug});
  },

  render: function() {
    return (
      React.DOM.button({onClick: this.onClick}, 
        this.state.on ? "Stop AEC logging" : "Start AEC logging"
      )
    );
  }
});

var Logs = React.createClass({displayName: 'Logs',
  render: function() {
    return (
      React.DOM.div(null, 
        React.DOM.h3(null, "Logging:"), 
        React.DOM.div(null, 
          this.props.logs.map(function(line, i) {
            return React.DOM.div({key: i}, line);
          })
        )
      )
    );
  }
});

function iceCandidateMapping(iceCandidates) {
  var candidates = {};
  iceCandidates = iceCandidates || [];

  iceCandidates.forEach(function(candidate) {
    candidates[candidate.id] = candidate;
  });

  return candidates;
}

function round00(num) {
  return Math.round(num * 100) / 100;
}

function labelize(label) {
  return `${label.charAt(0).toUpperCase()}${label.slice(1)}`;
}

function candidateToString(c) {
  var type = c.candidateType;

  if (c.type == "localcandidate" && c.candidateType == "relayed") {
    type = `${c.candidateType}-${c.mozLocalTransport}`;
  }

  return `${c.ipAddress}:${c.portNumber}/${c.transport}(${type})`
}

function onLoad() {
  WebrtcGlobalInformation.getAllStats(function(globalReport) {
    var reports = globalReport.reports;
    React.renderComponent(AboutWebRTC({reports: reports}),
                          document.querySelector("#body"));
  });
}
