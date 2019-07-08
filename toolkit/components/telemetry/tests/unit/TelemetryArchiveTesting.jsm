const { TelemetryArchive } = ChromeUtils.import(
  "resource://gre/modules/TelemetryArchive.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["TelemetryArchiveTesting"];

function checkForProperties(ping, expected) {
  for (let [props, val] of expected) {
    let test = ping;
    for (let prop of props) {
      test = test[prop];
      if (test === undefined) {
        return false;
      }
    }
    if (test !== val) {
      return false;
    }
  }
  return true;
}

/**
 * A helper object that allows test code to check whether a telemetry ping
 * was properly saved. To use, first initialize to collect the starting pings
 * and then check for new ping data.
 */
function Checker() {}
Checker.prototype = {
  promiseInit() {
    this._pingMap = new Map();
    return TelemetryArchive.promiseArchivedPingList().then(plist => {
      for (let ping of plist) {
        this._pingMap.set(ping.id, ping);
      }
    });
  },

  /**
   * Find and return a new ping with certain properties.
   *
   * @param expected: an array of [['prop'...], 'value'] to check
   * For example:
   * [
   *   [['environment', 'build', 'applicationId'], '20150101010101'],
   *   [['version'], 1],
   *   [['metadata', 'OOMAllocationSize'], 123456789],
   * ]
   * @returns a matching ping if found, or null
   */
  async promiseFindPing(type, expected) {
    let candidates = [];
    let plist = await TelemetryArchive.promiseArchivedPingList();
    for (let ping of plist) {
      if (this._pingMap.has(ping.id)) {
        continue;
      }
      if (ping.type == type) {
        candidates.push(ping);
      }
    }

    for (let candidate of candidates) {
      let ping = await TelemetryArchive.promiseArchivedPingById(candidate.id);
      if (checkForProperties(ping, expected)) {
        return ping;
      }
    }
    return null;
  },
};

const TelemetryArchiveTesting = {
  setup() {
    Services.prefs.setCharPref("toolkit.telemetry.log.level", "Trace");
    Services.prefs.setBoolPref("toolkit.telemetry.archive.enabled", true);
  },

  Checker,
};
