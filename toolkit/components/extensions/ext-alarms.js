var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  ignoreEvent,
  runSafe,
} = ExtensionUtils;

// WeakMap[Extension -> Set[Alarm]]
var alarmsMap = new WeakMap();

// WeakMap[Extension -> Set[callback]]
var alarmCallbacksMap = new WeakMap();

// Manages an alarm created by the extension (alarms API).
function Alarm(extension, name, alarmInfo)
{
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
    alarmsMap.get(this.extension).delete(this);
    this.canceled = true;
  },

  observe(subject, topic, data) {
    for (let callback in alarmCallbacksMap.get(this.extension)) {
      callback(this);
    }
    if (this.canceled) {
      return;
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

extensions.on("startup", (type, extension) => {
  alarmsMap.set(extension, new Set());
  alarmCallbacksMap.set(extension, new Set());
});

extensions.on("shutdown", (type, extension) => {
  for (let alarm of alarmsMap.get(extension)) {
    alarm.clear();
  }
  alarmsMap.delete(extension);
  alarmCallbacksMap.delete(extension);
});

extensions.registerAPI((extension, context) => {
  return {
    alarms: {
      create: function(...args) {
        let name = "", alarmInfo;
        if (args.length == 1) {
          alarmInfo = args[0];
        } else {
          [name, alarmInfo] = args;
        }

        let alarm = new Alarm(extension, name, alarmInfo);
        alarmsMap.get(extension).add(alarm);
      },

      get: function(args) {
        let name = "", callback;
        if (args.length == 1) {
          callback = args[0];
        } else {
          [name, callback] = args;
        }

        for (let alarm of alarmsMap.get(extension)) {
          if (alarm.name == name) {
            runSafe(context, callback, alarm.data);
            break;
          }
        }
      },

      getAll: function(callback) {
        let alarms = alarmsMap.get(extension);
        result = [ for (alarm of alarms) alarm.data ];
        runSafe(context, callback, result);
      },

      clear: function(...args) {
        let name = "", callback;
        if (args.length == 1) {
          callback = args[0];
        } else {
          [name, callback] = args;
        }

        let alarms = alarmsMap.get(extension);
        let cleared = false;
        for (let alarm of alarms) {
          if (alarm.name == name) {
            alarm.clear();
            cleared = true;
            break;
          }
        }

        if (callback) {
          runSafe(context, callback, cleared);
        }
      },

      clearAll: function(callback) {
        let alarms = alarmsMap.get(extension);
        let cleared = false;
        for (let alarm of alarms) {
          alarm.clear();
          cleared = true;
        }
        if (callback) {
          runSafe(context, callback, cleared);
        }
      },

      onAlarm: new EventManager(context, "alarms.onAlarm", fire => {
        let callback = alarm => {
          fire(alarm.data);
        };

        alarmCallbacksMap.get(extension).add(callback);
        return () => {
          alarmCallbacksMap.get(extension).delete(callback);
        };
      }).api(),
    },
  };
});
