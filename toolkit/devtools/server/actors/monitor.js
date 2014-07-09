/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {Ci,Cu} = require("chrome");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const Services = require("Services");
let {setTimeout} = require("sdk/timers");

function MonitorActor(aConnection) {
  this.conn = aConnection;
}

MonitorActor.prototype = {
  actorPrefix: "monitor",

  // Updates

  _toSend: [],
  _timeout: null,
  _started: false,
  _scheduleUpdate: function() {
    if (this._started && !this._timeout) {
      this._timeout = setTimeout(() => {
        if (this._toSend.length > 0) {
          this.conn.sendActorEvent(this.actorID, "update", this._toSend);
          this._toSend = [];
        }
        this._timeout = null;
      }, 200);
    }
  },


  // Methods available from the front

  start: function() {
    if (!this._started) {
      this._started = true;
      Services.obs.addObserver(this, "devtools-monitor-update", false);
    }
    return {};
  },

  stop: function() {
    if (this._started) {
      Services.obs.removeObserver(this, "devtools-monitor-update");
      this._started = false;
    }
    return {};
  },

  // nsIObserver

  observe: function (subject, topic, data) {
    if (topic == "devtools-monitor-update") {
      try {
        data = JSON.parse(data);
      } catch(e) {
        console.error("Observer notification data is not a valid JSON-string:",
                      data, e.message);
        return;
      }
      if (!Array.isArray(data)) {
        this._toSend.push(data);
      } else {
        this._toSend = this._toSend.concat(data);
      }
      this._scheduleUpdate();
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
}

MonitorActor.prototype.requestTypes = {
  "start": MonitorActor.prototype.start,
  "stop": MonitorActor.prototype.stop,
};

exports.MonitorActor = MonitorActor;

exports.register = function(handle) {
  handle.addGlobalActor(MonitorActor, "monitorActor");
  handle.addTabActor(MonitorActor, "monitorActor");
};

exports.unregister = function(handle) {
  handle.removeGlobalActor(MonitorActor, "monitorActor");
  handle.removeTabActor(MonitorActor, "monitorActor");
};
