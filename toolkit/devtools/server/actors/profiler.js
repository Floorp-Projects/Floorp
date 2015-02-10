/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Cc, Ci, Cu, Cr} = require("chrome");
const Services = require("Services");
const DevToolsUtils = require("devtools/toolkit/DevToolsUtils.js");

let DEFAULT_PROFILER_ENTRIES = 1000000;
let DEFAULT_PROFILER_INTERVAL = 1;
let DEFAULT_PROFILER_FEATURES = ["js"];
let DEFAULT_PROFILER_THREADFILTERS = ["GeckoMain"];

/**
 * The nsIProfiler is target agnostic and interacts with the whole platform.
 * Therefore, special care needs to be given to make sure different actor
 * consumers (i.e. "toolboxes") don't interfere with each other.
 */
let gProfilerConsumers = 0;
let gProfilingStartTime = -1;
Services.obs.addObserver(() => gProfilingStartTime = Date.now(), "profiler-started", false);
Services.obs.addObserver(() => gProfilingStartTime = -1, "profiler-stopped", false);

loader.lazyGetter(this, "nsIProfilerModule", () => {
  return Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
});

/**
 * The profiler actor provides remote access to the built-in nsIProfiler module.
 */
function ProfilerActor() {
  gProfilerConsumers++;
  this._observedEvents = new Set();
}

ProfilerActor.prototype = {
  actorPrefix: "profiler",
  disconnect: function() {
    for (let event of this._observedEvents) {
      Services.obs.removeObserver(this, event);
    }
    this._observedEvents = null;
    this.onStopProfiler();

    gProfilerConsumers--;
    checkProfilerConsumers();
  },

  /**
   * Returns an array of feature strings, describing the profiler features
   * that are available on this platform. Can be called while the profiler
   * is stopped.
   */
  onGetFeatures: function() {
    return { features: nsIProfilerModule.GetFeatures([]) };
  },

  /**
   * Starts the nsIProfiler module. Doing so will discard any samples
   * that might have been accumulated so far.
   *
   * @param number entries [optional]
   * @param number interval [optional]
   * @param array:string features [optional]
   * @param array:string threadFilters [description]
   */
  onStartProfiler: function(request = {}) {
    nsIProfilerModule.StartProfiler(
      (request.entries || DEFAULT_PROFILER_ENTRIES),
      (request.interval || DEFAULT_PROFILER_INTERVAL),
      (request.features || DEFAULT_PROFILER_FEATURES),
      (request.features || DEFAULT_PROFILER_FEATURES).length,
      (request.threadFilters || DEFAULT_PROFILER_THREADFILTERS),
      (request.threadFilters || DEFAULT_PROFILER_THREADFILTERS).length);

    return { started: true };
  },

  /**
   * Stops the nsIProfiler module, if no other client is using it.
   */
  onStopProfiler: function() {
    // Actually stop the profiler only if the last client has stopped profiling.
    // Since this is a root actor, and the profiler module interacts with the
    // whole platform, we need to avoid a case in which the profiler is stopped
    // when there might be other clients still profiling.
    if (gProfilerConsumers == 1) {
      nsIProfilerModule.StopProfiler();
    }
    return { started: false };
  },

  /**
   * Verifies whether or not the nsIProfiler module has started.
   * If already active, the current time is also returned.
   */
  onIsActive: function() {
    let isActive = nsIProfilerModule.IsActive();
    let elapsedTime = isActive ? getElapsedTime() : undefined;
    return { isActive: isActive, currentTime: elapsedTime };
  },

  /**
   * Returns a stringified JSON object that describes the shared libraries
   * which are currently loaded into our process. Can be called while the
   * profiler is stopped.
   */
  onGetSharedLibraryInformation: function() {
    return { sharedLibraryInformation: nsIProfilerModule.getSharedLibraryInformation() };
  },

  /**
   * Returns all the samples accumulated since the profiler was started,
   * along with the current time. The data has the following format:
   * {
   *   libs: string,
   *   meta: {
   *     interval: number,
   *     platform: string,
   *     ...
   *   },
   *   threads: [{
   *     samples: [{
   *       frames: [{
   *         line: number,
   *         location: string,
   *         category: number
   *       } ... ],
   *       name: string
   *       responsiveness: number
   *       time: number
   *     } ... ]
   *   } ... ]
   * }
   */
  onGetProfile: function() {
    let profile = nsIProfilerModule.getProfileData();
    return { profile: profile, currentTime: getElapsedTime() };
  },

  /**
   * Registers for certain event notifications.
   * Currently supported events:
   *   - "console-api-profiler"
   *   - "profiler-started"
   *   - "profiler-stopped"
   */
  onRegisterEventNotifications: function(request) {
    let response = [];
    for (let event of request.events) {
      if (this._observedEvents.has(event)) {
        continue;
      }
      Services.obs.addObserver(this, event, false);
      this._observedEvents.add(event);
      response.push(event);
    }
    return { registered: response };
  },

  /**
   * Unregisters from certain event notifications.
   * Currently supported events:
   *   - "console-api-profiler"
   *   - "profiler-started"
   *   - "profiler-stopped"
   */
  onUnregisterEventNotifications: function(request) {
    let response = [];
    for (let event of request.events) {
      if (!this._observedEvents.has(event)) {
        continue;
      }
      Services.obs.removeObserver(this, event);
      this._observedEvents.delete(event);
      response.push(event);
    }
    return { unregistered: response };
  },

  /**
   * Callback for all observed notifications.
   * @param object subject
   * @param string topic
   * @param object data
   */
  observe: DevToolsUtils.makeInfallible(function(subject, topic, data) {
    // Create JSON objects suitable for transportation across the RDP,
    // by breaking cycles and making a copy of the `subject` and `data` via
    // JSON.stringifying those values with a replacer that omits properties
    // known to introduce cycles, and then JSON.parsing the result.
    // This spends some CPU cycles, but it's simple.
    subject = (subject && !Cu.isXrayWrapper(subject) && subject.wrappedJSObject) || subject;
    subject = JSON.parse(JSON.stringify(subject, cycleBreaker));
    data = (data && !Cu.isXrayWrapper(data) && data.wrappedJSObject) || data;
    data = JSON.parse(JSON.stringify(data, cycleBreaker));

    // Sends actor, type and other additional information over the remote
    // debugging protocol to any profiler clients.
    let reply = details => {
      this.conn.send({
        from: this.actorID,
        type: "eventNotification",
        subject: subject,
        topic: topic,
        data: data,
        details: details
      });
    };

    switch (topic) {
      case "console-api-profiler":
        return void reply(this._handleConsoleEvent(subject, data));
      case "profiler-started":
      case "profiler-stopped":
      default:
        return void reply();
    }
  }, "ProfilerActor.prototype.observe"),

  /**
   * Handles `console.profile` and `console.profileEnd` invocations and
   * creates an appropriate response sent over the protocol.
   * @param object subject
   * @param object data
   * @return object
   */
  _handleConsoleEvent: function(subject, data) {
    // An optional label may be specified when calling `console.profile`.
    // If that's the case, stringify it and send it over with the response.
    let args = subject.arguments;
    let profileLabel = args.length > 0 ? args[0] + "" : undefined;

    // If the event was generated from `console.profile` or `console.profileEnd`
    // we need to start the profiler right away and then just notify the client.
    // Otherwise, we'll lose precious samples.

    if (subject.action == "profile") {
      let { isActive, currentTime } = this.onIsActive();

      // Start the profiler only if it wasn't already active. Otherwise, any
      // samples that might have been accumulated so far will be discarded.
      if (!isActive) {
        this.onStartProfiler();
        return {
          profileLabel: profileLabel,
          currentTime: 0
        };
      }
      return {
        profileLabel: profileLabel,
        currentTime: currentTime
      };
    }

    if (subject.action == "profileEnd") {
      let details = this.onGetProfile();
      details.profileLabel = profileLabel;
      return details;
    }
  }
};

exports.ProfilerActor = ProfilerActor;

/**
 * JSON.stringify callback used in ProfilerActor.prototype.observe.
 */
function cycleBreaker(key, value) {
  if (key == "wrappedJSObject") {
    return undefined;
  }
  return value;
}

/**
 * Gets the time elapsed since the profiler was last started.
 * @return number
 */
function getElapsedTime() {
  // Assign `gProfilingStartTime` now if no client of this actor has actually
  // started it yet, but the built-in profiler module is somehow already active
  // (it could happen if the MOZ_PROFILER_STARTUP environment variable is set,
  // or the Gecko Profiler add-on is installed and isn't using this actor).
  // Otherwise, the returned value is bogus and messes up the samples filtering.
  if (gProfilingStartTime == -1) {
    let profile = nsIProfilerModule.getProfileData();
    let lastSampleTime = findOldestSampleTime(profile);
    gProfilingStartTime = Date.now() - lastSampleTime;
  }
  return Date.now() - gProfilingStartTime;
}

/**
 * Finds the oldest sample time in the provided profile.
 * @param object profile
 * @return number
 */
function findOldestSampleTime(profile) {
  let firstThreadSamples = profile.threads[0].samples;

  for (let i = firstThreadSamples.length - 1; i >= 0; i--) {
    if ("time" in firstThreadSamples[i]) {
      return firstThreadSamples[i].time;
    }
  }
}

/**
 * Asserts the value sanity of `gProfilerConsumers`.
 */
function checkProfilerConsumers() {
  if (gProfilerConsumers < 0) {
    let msg = "Somehow the number of started profilers is now negative.";
    DevToolsUtils.reportException("ProfilerActor", msg);
  }
}

/**
 * The request types this actor can handle.
 * At the moment there are two known users of the Profiler actor:
 * the devtools and the Gecko Profiler addon, which uses the debugger
 * protocol to get profiles from Fennec.
 */
ProfilerActor.prototype.requestTypes = {
  "getFeatures": ProfilerActor.prototype.onGetFeatures,
  "startProfiler": ProfilerActor.prototype.onStartProfiler,
  "stopProfiler": ProfilerActor.prototype.onStopProfiler,
  "isActive": ProfilerActor.prototype.onIsActive,
  "getSharedLibraryInformation": ProfilerActor.prototype.onGetSharedLibraryInformation,
  "getProfile": ProfilerActor.prototype.onGetProfile,
  "registerEventNotifications": ProfilerActor.prototype.onRegisterEventNotifications,
  "unregisterEventNotifications": ProfilerActor.prototype.onUnregisterEventNotifications
};
