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
  this._conn = aConnection;
  this._profiler = Cc["@mozilla.org/tools/profiler;1"].getService(Ci.nsIProfiler);
  this._started = false;

  Cu.import("resource://gre/modules/Services.jsm");
  Services.obs.addObserver(this, "profiler-started", false);
  Services.obs.addObserver(this, "profiler-stopped", false);
}

ProfilerActor.prototype = {
  actorPrefix: "profiler",

  disconnect: function() {
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
  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "profiler-started") {
      this.conn.send({ from: this.actorID, type: "profilerStateChanged", isActive: true });
    } else if (aTopic == "profiler-stopped") {
      this.conn.send({ from: this.actorID, type: "profilerStateChanged", isActive: false });
    }
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
  "getSharedLibraryInformation": ProfilerActor.prototype.onGetSharedLibraryInformation
};

DebuggerServer.addGlobalActor(ProfilerActor, "profilerActor");
