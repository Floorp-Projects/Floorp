/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-sync/main.js");

XPCOMUtils.defineLazyModuleGetter(this, "BookmarkRepairRequestor",
  "resource://services-sync/bookmark_repair.js");

this.EXPORTED_SYMBOLS = ["getRepairRequestor", "getAllRepairRequestors",
                         "CollectionRepairRequestor",
                         "getRepairResponder",
                         "CollectionRepairResponder"];

// The individual requestors/responders, lazily loaded.
const REQUESTORS = {
  bookmarks: ["bookmark_repair.js", "BookmarkRepairRequestor"],
};

const RESPONDERS = {
  bookmarks: ["bookmark_repair.js", "BookmarkRepairResponder"],
};

// Should we maybe enforce the requestors being a singleton?
function _getRepairConstructor(which, collection) {
  if (!(collection in which)) {
    return null;
  }
  let [modname, symbolname] = which[collection];
  let ns = {};
  Cu.import("resource://services-sync/" + modname, ns);
  return ns[symbolname];
}

function getRepairRequestor(collection) {
  let ctor = _getRepairConstructor(REQUESTORS, collection);
  if (!ctor) {
    return null;
  }
  return new ctor();
}

function getAllRepairRequestors() {
  let result = {};
  for (let collection of Object.keys(REQUESTORS)) {
    let ctor = _getRepairConstructor(REQUESTORS, collection);
    result[collection] = new ctor();
  }
  return result;
}

function getRepairResponder(collection) {
  let ctor = _getRepairConstructor(RESPONDERS, collection);
  if (!ctor) {
    return null;
  }
  return new ctor();
}

// The abstract classes.
class CollectionRepairRequestor {
  constructor(service = null) {
    // allow service to be mocked in tests.
    this.service = service || Weave.Service;
  }

  /* Try to resolve some issues with the server without involving other clients.
     Returns true if we repaired some items.

     @param   validationInfo       {Object}
              The validation info as returned by the collection's validator.

  */
  tryServerOnlyRepairs(validationInfo) {
    return false;
  }

  /* See if the repairer is willing and able to begin a repair process given
     the specified validation information.
     Returns true if a repair was started and false otherwise.

     @param   validationInfo       {Object}
              The validation info as returned by the collection's validator.

     @param   flowID               {String}
              A guid that uniquely identifies this repair process for this
              collection, and which should be sent to any requestors and
              reported in telemetry.

  */
  async startRepairs(validationInfo, flowID) {
    throw new Error("not implemented");
  }

  /* Work out what state our current repair request is in, and whether it can
     proceed to a new state.
     Returns true if we could continue the repair - even if the state didn't
     actually move. Returns false if we aren't actually repairing.

     @param   responseInfo       {Object}
              An optional response to a previous repair request, as returned
              by a remote repair responder.

  */
  async continueRepairs(responseInfo = null) {
    throw new Error("not implemented");
  }
}

class CollectionRepairResponder {
  constructor(service = null) {
    // allow service to be mocked in tests.
    this.service = service || Weave.Service;
  }

  /* Take some action in response to a repair request. Returns a promise that
     resolves once the repair process has started, or rejects if there
     was an error starting the repair.

     Note that when the promise resolves the repair is not yet complete - at
     some point in the future the repair will auto-complete, at which time
     |rawCommand| will be removed from the list of client commands for this
     client.

     @param   request       {Object}
              The repair request as sent by another client.

     @param   rawCommand    {Object}
              The command object as stored in the clients engine, and which
              will be automatically removed once a repair completes.
  */
  async repair(request, rawCommand) {
    throw new Error("not implemented");
  }
}
