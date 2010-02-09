/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Update timer Manager.
#
# The Initial Developer of the Original Code is Ben Goodger.
# Portions created by the Initial Developer are Copyright (C) 2004
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#  Ben Goodger <ben@mozilla.org> (Original Author)
#  Robert Strong <robert.bugzilla@gmail.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

const Cc = Components.classes;
const Ci = Components.interfaces;

const PREF_APP_UPDATE_LASTUPDATETIME_FMT  = "app.update.lastUpdateTime.%ID%";
const PREF_APP_UPDATE_TIMER               = "app.update.timer";
const PREF_APP_UPDATE_LOG                 = "app.update.log";

const CATEGORY_UPDATE_TIMER               = "update-timer";

XPCOMUtils.defineLazyServiceGetter(this, "gPref",
                                   "@mozilla.org/preferences-service;1",
                                   "nsIPrefBranch2");

XPCOMUtils.defineLazyServiceGetter(this, "gConsole",
                                   "@mozilla.org/consoleservice;1",
                                   "nsIConsoleService");

XPCOMUtils.defineLazyGetter(this, "gLogEnabled", function tm_gLogEnabled() {
  return getPref("getBoolPref", PREF_APP_UPDATE_LOG, false);
});

function getObserverService() {
  return Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
}

/**
#  Gets a preference value, handling the case where there is no default.
#  @param   func
#           The name of the preference function to call, on nsIPrefBranch
#  @param   preference
#           The name of the preference
#  @param   defaultValue
#           The default value to return in the event the preference has
#           no setting
#  @returns The value of the preference, or undefined if there was no
#           user or default value.
 */
function getPref(func, preference, defaultValue) {
  try {
    return gPref[func](preference);
  }
  catch (e) {
  }
  return defaultValue;
}

/**
#  Logs a string to the error console.
#  @param   string
#           The string to write to the error console.
 */
function LOG(string) {
  if (gLogEnabled) {
    dump("*** UTM:SVC " + string + "\n");
    gConsole.logStringMessage("UTM:SVC " + string);
  }
}

/**
#  A manager for timers. Manages timers that fire over long periods of time
#  (e.g. days, weeks, months).
#  @constructor
 */
function TimerManager() {
  getObserverService().addObserver(this, "xpcom-shutdown", false);
}
TimerManager.prototype = {
  /**
   * The Checker Timer
   */
  _timer: null,

  /**
#    The Checker Timer interval as specified by the app.update.timer pref. If
#    the app.update.timer pref doesn't exist this will default to 600000.
   */
   _timerInterval: null,

  /**
   * The set of registered timers.
   */
  _timers: { },

  /**
#    The amount to fudge the lastUpdateTime where fudge is a random increment of
#    the update check interval (e.g. some random slice of 10 minutes). When the
#    time comes to notify a timer or a timer is first registered the timer is
#    offset by this amount to lessen the number of timers firing at the same
#    time. this._timerInterval is in milliseconds, whereas the lastUpdateTime is
#    in seconds so this._timerInterval is divided by 1000.
   */
  get _fudge() {
    return Math.round(Math.random() * this._timerInterval / 1000);
  },

  /**
   * See nsIObserver.idl
   */
  observe: function TM_observe(aSubject, aTopic, aData) {
    // Prevent setting the timer interval to a value of less than 60 seconds.
    var minInterval = 60000;
    switch (aTopic) {
    case "utm-test-init":
      // Enforce a minimum timer interval of 500 ms for tests and fall through
      // to profile-after-change to initialize the timer.
      minInterval = 500;
    case "profile-after-change":
      // Cancel the timer if it has already been initialized. This is primarily
      // for tests.
      if (this._timer) {
        this._timer.cancel();
        this._timer = null;
      }
      this._timerInterval = Math.max(getPref("getIntPref", PREF_APP_UPDATE_TIMER, 600000),
                                     minInterval);
      this._timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      this._timer.initWithCallback(this, this._timerInterval,
                                   Ci.nsITimer.TYPE_REPEATING_SLACK);
      break;
    case "xpcom-shutdown":
      let os = getObserverService();
      os.removeObserver(this, "xpcom-shutdown");

      // Release everything we hold onto.
      if (this._timer) {
        this._timer.cancel();
        this._timer = null;
      }
      for (var timerID in this._timers)
        delete this._timers[timerID];
      this._timers = null;
      break;
    }
  },

  /**
#    Called when the checking timer fires.
#    @param   timer
#             The checking timer that fired.
   */
  notify: function TM_notify(timer) {
    var prefLastUpdate;
    var lastUpdateTime;
    var now = Math.round(Date.now() / 1000);
    var catMan = Cc["@mozilla.org/categorymanager;1"].
                 getService(Ci.nsICategoryManager);
    var entries = catMan.enumerateCategory(CATEGORY_UPDATE_TIMER);
    while (entries.hasMoreElements()) {
      let entry = entries.getNext().QueryInterface(Ci.nsISupportsCString).data;
      let value = catMan.getCategoryEntry(CATEGORY_UPDATE_TIMER, entry);
      let [cid, method, timerID, prefInterval, defaultInterval] = value.split(",");
      defaultInterval = parseInt(defaultInterval);
      // cid and method are validated below when calling notify.
      if (!timerID || !defaultInterval || isNaN(defaultInterval)) {
        LOG("TimerManager:notify - update-timer category registered" +
            (cid ? " for " + cid : "") + " without required parameters - " +
             "skipping");
        continue;
      }

      let interval = getPref("getIntPref", prefInterval, defaultInterval);
      prefLastUpdate = PREF_APP_UPDATE_LASTUPDATETIME_FMT.replace(/%ID%/,
                                                                  timerID);
      if (gPref.prefHasUserValue(prefLastUpdate)) {
        lastUpdateTime = gPref.getIntPref(prefLastUpdate);
      }
      else {
        lastUpdateTime = now + this._fudge;
        gPref.setIntPref(prefLastUpdate, lastUpdateTime);
        continue;
      }

      if ((now - lastUpdateTime) > interval) {
        try {
          Components.classes[cid][method](Ci.nsITimerCallback).notify(timer);
          LOG("TimerManager:notify - notified " + cid);
        }
        catch (e) {
          LOG("TimerManager:notify - error notifying component id: " +
              cid + " ,error: " + e);
        }
        lastUpdateTime = now + this._fudge;
        gPref.setIntPref(prefLastUpdate, lastUpdateTime);
      }
    }

    for (var timerID in this._timers) {
      var timerData = this._timers[timerID];

      if ((now - timerData.lastUpdateTime) > timerData.interval) {
        if (timerData.callback instanceof Ci.nsITimerCallback) {
          try {
            timerData.callback.notify(timer);
            LOG("TimerManager:notify - notified timerID: " + timerID);
          }
          catch (e) {
            LOG("TimerManager:notify - error notifying timerID: " + timerID +
                ", error: " + e);
          }
        }
        else {
          LOG("TimerManager:notify - timerID: " + timerID + " doesn't " +
              "implement nsITimerCallback - skipping");
        }
        lastUpdateTime = now + this._fudge;
        timerData.lastUpdateTime = lastUpdateTime;
        prefLastUpdate = PREF_APP_UPDATE_LASTUPDATETIME_FMT.replace(/%ID%/, timerID);
        gPref.setIntPref(prefLastUpdate, lastUpdateTime);
      }
    }
  },

  /**
   * See nsIUpdateTimerManager.idl
   */
  registerTimer: function TM_registerTimer(id, callback, interval) {
    LOG("TimerManager:registerTimer - id: " + id);
    var prefLastUpdate = PREF_APP_UPDATE_LASTUPDATETIME_FMT.replace(/%ID%/, id);
    var lastUpdateTime;
    if (gPref.prefHasUserValue(prefLastUpdate)) {
      lastUpdateTime = gPref.getIntPref(prefLastUpdate);
    } else {
      lastUpdateTime = Math.round(Date.now() / 1000) + this._fudge;
      gPref.setIntPref(prefLastUpdate, lastUpdateTime);
    }
    this._timers[id] = { callback       : callback,
                         interval       : interval,
                         lastUpdateTime : lastUpdateTime };
  },

  classDescription: "Timer Manager",
  contractID: "@mozilla.org/updates/timer-manager;1",
  classID: Components.ID("{B322A5C0-A419-484E-96BA-D7182163899F}"),
  _xpcom_categories: [{ category: "profile-after-change" }],
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIUpdateTimerManager,
                                         Ci.nsITimerCallback,
                                         Ci.nsIObserver])
};

function NSGetModule(compMgr, fileSpec)
  XPCOMUtils.generateModule([TimerManager]);
