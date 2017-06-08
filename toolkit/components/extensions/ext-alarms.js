"use strict";

// The ext-* files are imported into the same scopes.
/* import-globals-from ext-toolkit.js */

// WeakMap[Extension -> Map[name -> Alarm]]
let alarmsMap = new WeakMap();

// WeakMap[Extension -> Set[callback]]
let alarmCallbacksMap = new WeakMap();

// Manages an alarm created by the extension (alarms API).
function Alarm(extension, name, alarmInfo) {
  this.extension = extension;
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
  timer.init(this, delay, Ci.nsITimer.TYPE_ONE_SHOT);
  this.timer = timer;
}

Alarm.prototype = {
  clear() {
    this.timer.cancel();
    alarmsMap.get(this.extension).delete(this.name);
    this.canceled = true;
  },

  observe(subject, topic, data) {
    if (this.canceled) {
      return;
    }

    for (let callback of alarmCallbacksMap.get(this.extension)) {
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
  onShutdown() {
    let {extension} = this;

    if (alarmsMap.has(extension)) {
      for (let alarm of alarmsMap.get(extension).values()) {
        alarm.clear();
      }
      alarmsMap.delete(extension);
      alarmCallbacksMap.delete(extension);
    }
  }

  getAPI(context) {
    let {extension} = context;

    alarmsMap.set(extension, new Map());
    alarmCallbacksMap.set(extension, new Set());

    return {
      alarms: {
        create: function(name, alarmInfo) {
          name = name || "";
          let alarms = alarmsMap.get(extension);
          if (alarms.has(name)) {
            alarms.get(name).clear();
          }
          let alarm = new Alarm(extension, name, alarmInfo);
          alarms.set(alarm.name, alarm);
        },

        get: function(name) {
          name = name || "";
          let alarms = alarmsMap.get(extension);
          if (alarms.has(name)) {
            return Promise.resolve(alarms.get(name).data);
          }
          return Promise.resolve();
        },

        getAll: function() {
          let result = Array.from(alarmsMap.get(extension).values(), alarm => alarm.data);
          return Promise.resolve(result);
        },

        clear: function(name) {
          name = name || "";
          let alarms = alarmsMap.get(extension);
          if (alarms.has(name)) {
            alarms.get(name).clear();
            return Promise.resolve(true);
          }
          return Promise.resolve(false);
        },

        clearAll: function() {
          let cleared = false;
          for (let alarm of alarmsMap.get(extension).values()) {
            alarm.clear();
            cleared = true;
          }
          return Promise.resolve(cleared);
        },

        onAlarm: new SingletonEventManager(context, "alarms.onAlarm", fire => {
          let callback = alarm => {
            fire.sync(alarm.data);
          };

          alarmCallbacksMap.get(extension).add(callback);
          return () => {
            alarmCallbacksMap.get(extension).delete(callback);
          };
        }).api(),
      },
    };
  }
};
