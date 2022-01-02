/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-toolkit.js */

// Manages an alarm created by the extension (alarms API).
function Alarm(api, name, alarmInfo) {
  this.api = api;
  this.name = name;
  this.when = alarmInfo.when;
  this.delayInMinutes = alarmInfo.delayInMinutes;
  this.periodInMinutes = alarmInfo.periodInMinutes;
  this.canceled = false;

  let delay, scheduledTime;
  if (this.when) {
    scheduledTime = this.when;
    delay = this.when - Date.now();
  } else {
    if (!this.delayInMinutes) {
      this.delayInMinutes = this.periodInMinutes;
    }
    delay = this.delayInMinutes * 60 * 1000;
    scheduledTime = Date.now() + delay;
  }

  this.scheduledTime = scheduledTime;

  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  delay = delay > 0 ? delay : 0;
  timer.init(this, delay, Ci.nsITimer.TYPE_ONE_SHOT);
  this.timer = timer;
}

Alarm.prototype = {
  clear() {
    this.timer.cancel();
    this.api.alarms.delete(this.name);
    this.canceled = true;
  },

  observe(subject, topic, data) {
    if (this.canceled) {
      return;
    }

    for (let callback of this.api.callbacks) {
      callback(this);
    }

    if (!this.periodInMinutes) {
      this.clear();
      return;
    }

    let delay = this.periodInMinutes * 60 * 1000;
    this.scheduledTime = Date.now() + delay;
    this.timer.init(this, delay, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  get data() {
    return {
      name: this.name,
      scheduledTime: this.scheduledTime,
      periodInMinutes: this.periodInMinutes,
    };
  },
};

this.alarms = class extends ExtensionAPI {
  constructor(extension) {
    super(extension);

    this.alarms = new Map();
    this.callbacks = new Set();
  }

  onShutdown() {
    for (let alarm of this.alarms.values()) {
      alarm.clear();
    }
  }

  getAPI(context) {
    const self = this;

    return {
      alarms: {
        create: function(name, alarmInfo) {
          name = name || "";
          if (self.alarms.has(name)) {
            self.alarms.get(name).clear();
          }
          let alarm = new Alarm(self, name, alarmInfo);
          self.alarms.set(alarm.name, alarm);
        },

        get: function(name) {
          name = name || "";
          if (self.alarms.has(name)) {
            return Promise.resolve(self.alarms.get(name).data);
          }
          return Promise.resolve();
        },

        getAll: function() {
          let result = Array.from(self.alarms.values(), alarm => alarm.data);
          return Promise.resolve(result);
        },

        clear: function(name) {
          name = name || "";
          if (self.alarms.has(name)) {
            self.alarms.get(name).clear();
            return Promise.resolve(true);
          }
          return Promise.resolve(false);
        },

        clearAll: function() {
          let cleared = false;
          for (let alarm of self.alarms.values()) {
            alarm.clear();
            cleared = true;
          }
          return Promise.resolve(cleared);
        },

        onAlarm: new EventManager({
          context,
          name: "alarms.onAlarm",
          register: fire => {
            let callback = alarm => {
              fire.sync(alarm.data);
            };

            self.callbacks.add(callback);
            return () => {
              self.callbacks.delete(callback);
            };
          },
        }).api(),
      },
    };
  }
};
