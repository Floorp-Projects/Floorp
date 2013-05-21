/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var connCount = 0;

/**
 * Creates a ProfilerActor. ProfilerActor provides remote access to the
 * built-in profiler module.
 */
function ProfilerActor(aConnection)
{
  this._profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
  this._started = false;
  this._observedEvents = [];
  connCount += 1;
}

ProfilerActor.prototype = {
  actorPrefix: "profiler",

  disconnect: function() {
    for (var event of this._observedEvents) {
      Services.obs.removeObserver(this, event);
    }

    // We stop the profiler only after the last client is
    // disconnected. Otherwise there's a problem where
    // we stop the profiler as soon as you close the devtools
    // panel in one tab even though there might be other
    // profiler instances running in other tabs.

    connCount -= 1;
    if (connCount <= 0 && this._profiler && this._started) {
      this._profiler.StopProfiler();
    }

    this._profiler = null;
  },

  onStartProfiler: function(aRequest) {
    this._profiler.StartProfiler(aRequest.entries, aRequest.interval,
                           aRequest.features, aRequest.features.length);
    this._started = true;
    return { "msg": "profiler started" }
  },
  onStopProfiler: function(aRequest) {
    this._profiler.StopProfiler();
    this._started = false;
    return { "msg": "profiler stopped" }
  },
  onGetProfileStr: function(aRequest) {
    var profileStr = this._profiler.GetProfile();
    return { "profileStr": profileStr }
  },
  onGetProfile: function(aRequest) {
    var profile = this._profiler.getProfileData();
    return { "profile": profile }
  },
  onIsActive: function(aRequest) {
    var isActive = this._profiler.IsActive();
    return { "isActive": isActive }
  },
  onGetResponsivenessTimes: function(aRequest) {
    var times = this._profiler.GetResponsivenessTimes({});
    return { "responsivenessTimes": times }
  },
  onGetFeatures: function(aRequest) {
    var features = this._profiler.GetFeatures([]);
    return { "features": features }
  },
  onGetSharedLibraryInformation: function(aRequest) {
    var sharedLibraries = this._profiler.getSharedLibraryInformation();
    return { "sharedLibraryInformation": sharedLibraries }
  },
  onRegisterEventNotifications: function(aRequest) {
    let registered = [];
    for (var event of aRequest.events) {
      if (this._observedEvents.indexOf(event) != -1)
        continue;
      Services.obs.addObserver(this, event, false);
      this._observedEvents.push(event);
      registered.push(event);
    }
    return { registered: registered }
  },
  onUnregisterEventNotifications: function(aRequest) {
    let unregistered = [];
    for (var event of aRequest.events) {
      let idx = this._observedEvents.indexOf(event);
      if (idx == -1)
        continue;
      Services.obs.removeObserver(this, event);
      this._observedEvents.splice(idx, 1);
      unregistered.push(event);
    }
    return { unregistered: unregistered }
  },
  observe: makeInfallible(function(aSubject, aTopic, aData) {
    /*
     * this.conn.send can only transmit acyclic values. However, it is
     * idiomatic for wrapped JS objects like aSubject (and possibly aData?)
     * to have a 'wrappedJSObject' property pointing to themselves.
     *
     * this.conn.send also assumes that it can retain the object it is
     * passed to be handled on later event ticks; and that it's okay to
     * freeze it. Since we don't really know what aSubject and aData are,
     * we need to pass this.conn.send a copy of them, not the originals.
     *
     * We break the cycle and make the copy by JSON.stringifying those
     * values with a replacer that omits properties known to introduce
     * cycles, and then JSON.parsing the result. This spends processor
     * time, but it's simple.
     */
    function cycleBreaker(key, value) {
      if (key === 'wrappedJSObject') {
        return undefined;
      }
      return value;
    }

    /*
     * If these values are objects with a non-null 'wrappedJSObject'
     * property, use its value. Otherwise, use the value unchanged.
     */
    aSubject = (aSubject && aSubject.wrappedJSObject) || aSubject;
    aData    = (aData    && aData.wrappedJSObject)    || aData;

    this.conn.send({ from: this.actorID,
                     type: "eventNotification",
                     event: aTopic,
                     subject: JSON.parse(JSON.stringify(aSubject, cycleBreaker)),
                     data:    JSON.parse(JSON.stringify(aData,    cycleBreaker)) });
  }, "ProfilerActor.prototype.observe"),
};

/**
 * The request types this actor can handle.
 */
ProfilerActor.prototype.requestTypes = {
  "startProfiler": ProfilerActor.prototype.onStartProfiler,
  "stopProfiler": ProfilerActor.prototype.onStopProfiler,
  "getProfileStr": ProfilerActor.prototype.onGetProfileStr,
  "getProfile": ProfilerActor.prototype.onGetProfile,
  "isActive": ProfilerActor.prototype.onIsActive,
  "getResponsivenessTimes": ProfilerActor.prototype.onGetResponsivenessTimes,
  "getFeatures": ProfilerActor.prototype.onGetFeatures,
  "getSharedLibraryInformation": ProfilerActor.prototype.onGetSharedLibraryInformation,
  "registerEventNotifications": ProfilerActor.prototype.onRegisterEventNotifications,
  "unregisterEventNotifications": ProfilerActor.prototype.onUnregisterEventNotifications
};

DebuggerServer.addGlobalActor(ProfilerActor, "profilerActor");
