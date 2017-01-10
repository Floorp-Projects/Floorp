/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains code for maintaining the set of declined engines,
 * in conjunction with EngineManager.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["DeclinedEngines"];

var {utils: Cu} = Components;

Cu.import("resource://services-sync/constants.js");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/observers.js");
Cu.import("resource://gre/modules/Preferences.jsm");



this.DeclinedEngines = function(service) {
  this._log = Log.repository.getLogger("Sync.Declined");
  this._log.level = Log.Level[new Preferences(PREFS_BRANCH).get("log.logger.declined")];

  this.service = service;
}
this.DeclinedEngines.prototype = {
  updateDeclined(meta, engineManager = this.service.engineManager) {
    let enabled = new Set(engineManager.getEnabled().map(e => e.name));
    let known = new Set(engineManager.getAll().map(e => e.name));
    let remoteDeclined = new Set(meta.payload.declined || []);
    let localDeclined = new Set(engineManager.getDeclined());

    this._log.debug("Handling remote declined: " + JSON.stringify([...remoteDeclined]));
    this._log.debug("Handling local declined: " + JSON.stringify([...localDeclined]));

    // Any engines that are locally enabled should be removed from the remote
    // declined list.
    //
    // Any engines that are locally declined should be added to the remote
    // declined list.
    let newDeclined = CommonUtils.union(localDeclined, CommonUtils.difference(remoteDeclined, enabled));

    // If our declined set has changed, put it into the meta object and mark
    // it as changed.
    let declinedChanged = !CommonUtils.setEqual(newDeclined, remoteDeclined);
    this._log.debug("Declined changed? " + declinedChanged);
    if (declinedChanged) {
      meta.changed = true;
      meta.payload.declined = [...newDeclined];
    }

    // Update the engine manager regardless.
    engineManager.setDeclined(newDeclined);

    // Any engines that are locally known, locally disabled, and not remotely
    // or locally declined, are candidates for enablement.
    let undecided = CommonUtils.difference(CommonUtils.difference(known, enabled), newDeclined);
    if (undecided.size) {
      let subject = {
        declined: newDeclined,
        enabled,
        known,
        undecided,
      };
      CommonUtils.nextTick(() => {
        Observers.notify("weave:engines:notdeclined", subject);
      });
    }

    return declinedChanged;
  },
};
