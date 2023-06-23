/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CHECK_RTC_STATS_COLLECTION = [
  "inboundRtpStreamStats",
  "outboundRtpStreamStats",
  "remoteInboundRtpStreamStats",
  "remoteOutboundRtpStreamStats",
];

const DEFAULT_PROPS = {
  avgPoints: 10,
  histSecs: 15,
  toRate: false,
  noAvg: false,
  fixedPointDecimals: 2,
  toHuman: false,
};

const REMOTE_RTP_PROPS = "avgPoints=2;histSecs=90";
const GRAPH_KEYS = [
  "inbound-rtp.framesPerSecond;noAvg",
  "inbound-rtp.packetsReceived;toRate",
  "inbound-rtp.packetsLost;toRate",
  "inbound-rtp.jitter;fixedPointDecimals=4",
  `remote-inbound-rtp.roundTripTime;${REMOTE_RTP_PROPS}`,
  `remote-inbound-rtp.packetsReceived;toRate;${REMOTE_RTP_PROPS}`,
  "outbound-rtp.packetsSent;toRate",
  "outbound-rtp.framesSent;toRate",
  "outbound-rtp.frameHeight;noAvg",
  "outbound-rtp.frameWidth;noAvg",
  "outbound-rtp.nackCount",
  "outbound-rtp.pliCount",
  "outbound-rtp.firCount",
  `remote-outbound-rtp.bytesSent;toHuman;toRate;${REMOTE_RTP_PROPS}`,
  `remote-outbound-rtp.packetsSent;toRate;${REMOTE_RTP_PROPS}`,
]
  .map(k => k.split(".", 2))
  .reduce((mapOfArr, [k, rest]) => {
    mapOfArr[k] ??= [];
    const [subKey, ...conf] = rest.split(";");
    let config = conf.reduce((c, v) => {
      let [configName, ...configVal] = v.split("=", 2);
      c[configName] = !configVal.length ? true : configVal[0];
      return c;
    }, {});
    mapOfArr[k].push({ subKey, config });
    return mapOfArr;
  }, {});

// Sliding window iterator of size n (where: n >= 1) over the array.
// Only returns full windows.
// Returns [] if n > array.length.
// eachN(['a','b','c','d','e'], 3) will yield the following values:
//   ['a','b','c'], ['b','c','d'], and ['c','d','e']
const eachN = (array, n) => {
  return {
    // Index state
    index: 0,
    // Iteration function
    next() {
      let slice = array.slice(this.index, this.index + n);
      this.index++;
      // Done is true _AFTER_ the last value has returned.
      // When done is true, value is ignored.
      return { value: slice, done: slice.length < n };
    },
    [Symbol.iterator]() {
      return this;
    },
  };
};

const msToSec = ms => 1000 * ms;

//
// A subset of the graph data
//
class GraphDataSet {
  constructor(dataPoints) {
    this.dataPoints = dataPoints;
  }

  // The latest
  latest = () => (this.dataPoints ? this.dataPoints.slice(-1)[0] : undefined);

  earliest = () => (this.dataPoints ? this.dataPoints[0] : undefined);

  // The returns the earliest time to graph
  startTime = () => (this.earliest() || { time: 0 }).time;

  // Returns the latest time to graph
  stopTime = () => (this.latest() || { time: 0 }).time;

  // Elapsed time within the display window
  elapsed = () =>
    this.dataPoints ? this.latest().time - this.earliest().time : 0;

  // Return a new data set that has been filtered
  filter = fn => new GraphDataSet([...this.dataPoints].filter(fn));

  // The range of values in the set or or undefined if the set is empty
  dataRange = () =>
    this.dataPoints.reduce(
      ({ min, max }, { value }) => ({
        min: Math.min(min, value),
        max: Math.max(max, value),
      }),
      this.dataPoints.length
        ? { min: this.dataPoints[0].value, max: this.dataPoints[0].value }
        : undefined
    );

  // Get the rates between points. By definition the rates will have
  // one fewer data points.
  toRateDataSet = () =>
    new GraphDataSet(
      [...eachN(this.dataPoints, 2)].map(([a, b]) => ({
        // Time mid point
        time: (b.time + a.time) / 2,
        value: msToSec(b.value - a.value) / (b.time - a.time),
      }))
    );

  average = samples =>
    samples.reduce(
      ({ time, value }, { time: t, value: v }) => ({
        time: time + t / samples.length,
        value: value + v / samples.length,
      }),
      { time: 0, value: 0 }
    );

  toRollingAverageDataSet = sampleSize =>
    new GraphDataSet([...eachN(this.dataPoints, sampleSize)].map(this.average));
}

class GraphData {
  constructor(id, key, subKey, config) {
    this.id = id;
    this.key = key;
    this.subKey = subKey;
    this.data = [];
    this.config = Object.assign({}, DEFAULT_PROPS, config);
  }

  setValueForTime(dataPoint) {
    this.data = this.data.filter(({ time: t }) => t != dataPoint.time);
    this.data.push(dataPoint);
  }

  getValuesSince = time => this.data.filter(dp => dp.time > time);

  getDataSetSince = time =>
    new GraphDataSet(this.data.filter(dp => dp.time > time));

  getConfig = () => this.config;

  // Cull old data, but keep twice the window size for average computation
  cullData = timeNow =>
    (this.data = this.data.filter(
      ({ time }) => time + msToSec(this.config.histSecs * 2) > timeNow
    ));
}

class GraphDb {
  constructor(report) {
    this.graphDatas = new Map();
    this.insertReportData(report);
  }

  mkStoreKey = ({ id, key, subKey }) => `${key}.${id}.${subKey}`;

  insertDataPoint(id, key, subKey, config, time, value) {
    let storeKey = this.mkStoreKey({ id, key, subKey });
    let data =
      this.graphDatas.get(storeKey) || new GraphData(id, key, subKey, config);
    data.setValueForTime({ time, value });
    data.cullData(time);
    this.graphDatas.set(storeKey, data);
  }

  insertReportData(report) {
    if (report.timestamp == this.lastReportTimestamp) {
      return;
    }
    this.lastReportTimestamp = report.timestamp;
    CHECK_RTC_STATS_COLLECTION.forEach(listName => {
      (report[listName] || []).forEach(stats => {
        (GRAPH_KEYS[stats.type] || []).forEach(({ subKey, config }) => {
          if (stats[subKey] !== undefined) {
            this.insertDataPoint(
              stats.id,
              stats.type,
              subKey,
              config,
              stats.timestamp,
              stats[subKey]
            );
          }
        });
      });
    });
  }

  getGraphDataById = id =>
    [...this.graphDatas.values()].filter(gd => gd.id == id);
}

export { GraphDb };
