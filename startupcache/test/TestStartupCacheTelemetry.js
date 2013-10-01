const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function shouldHaveChanged(a, b)
{
  if (a.length != b.length) {
    throw Error("TEST-UNEXPECTED-FAIL: telemetry count array size changed");
  }

  for (let i = 0; i < a.length; ++i) {
    if (a[i] == b[i]) {
      continue;
    }
    return; // something was different, that's all that matters
  }
  throw Error("TEST-UNEXPECTED-FAIL: telemetry data didn't change");
}

function TestStartupCacheTelemetry() { }

TestStartupCacheTelemetry.prototype = {
  classID: Components.ID("{73cbeffd-d6c7-42f0-aaf3-f176430dcfc8}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  saveInitial: function() {
    let t = Services.telemetry;
    this._age = t.getHistogramById("STARTUP_CACHE_AGE_HOURS").snapshot.counts;
    this._invalid = t.getHistogramById("STARTUP_CACHE_INVALID").snapshot.counts;
  },

  checkFinal: function() {
    let t = Services.telemetry;
    let newAge = t.getHistogramById("STARTUP_CACHE_AGE_HOURS").snapshot.counts;
    shouldHaveChanged(this._age, newAge);

    let newInvalid = t.getHistogramById("STARTUP_CACHE_INVALID").snapshot.counts;
    shouldHaveChanged(this._invalid, newInvalid);
  },

  observe: function(subject, topic, data) {
    switch (topic) {
    case "save-initial":
      this.saveInitial();
      break;

    case "check-final":
      this.checkFinal();
      break;

    default:
      throw Error("BADDOG, NO MILKBONE FOR YOU");
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([TestStartupCacheTelemetry]);
