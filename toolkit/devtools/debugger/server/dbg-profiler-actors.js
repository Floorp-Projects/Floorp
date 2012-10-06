/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Creates a ProfilerActor. ProfilerActor provides remote access to the
 * built-in profiler module.
 */
function ProfilerActor(aConnection)
{
  this._profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
  this._started = false;
  this._observedEvents = [];
}

ProfilerActor.prototype = {
  actorPrefix: "profiler",

  disconnect: function() {
    for (var event of this._observedEvents) {
      Services.obs.removeObserver(this, event);
    }
    if (this._profiler && this._started) {
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
  observe: function(aSubject, aTopic, aData) {
    function unWrapper(obj) {
      if (obj && typeof obj == "object" && ("wrappedJSObject" in obj)) {
        obj = obj.wrappedJSObject;
        if (("wrappedJSObject" in obj) && (obj.wrappedJSObject == obj)) {
          /* If the object defines wrappedJSObject as itself, which is the
           * typical idiom for wrapped JS objects, JSON.stringify won't be
           * able to work because the object is cyclic.
           * But removing the wrappedJSObject property will break aSubject
           * for possible other observers of the same topic, so we need
           * to restore wrappedJSObject afterwards */
          delete obj.wrappedJSObject;
          return { unwrapped: obj,
                   fixup: function() {
                     this.unwrapped.wrappedJSObject = this.unwrapped;
                   }
                 }
        }
      }
      return { unwrapped: obj, fixup: function() { } }
    }
    var subject = unWrapper(aSubject);
    var data = unWrapper(aData);
    this.conn.send({ from: this.actorID,
                     type: "eventNotification",
                     event: aTopic,
                     subject: subject.unwrapped,
                     data: data.unwrapped });
    data.fixup();
    subject.fixup();
  },
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
