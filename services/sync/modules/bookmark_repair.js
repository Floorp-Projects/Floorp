/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cu = Components.utils;

this.EXPORTED_SYMBOLS = ["BookmarkRepairRequestor", "BookmarkRepairResponder"];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/util.js");
Cu.import("resource://services-sync/collection_repair.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/resource.js");
Cu.import("resource://services-sync/doctor.js");
Cu.import("resource://services-sync/telemetry.js");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "PlacesSyncUtils",
                                  "resource://gre/modules/PlacesSyncUtils.jsm");

const log = Log.repository.getLogger("Sync.Engine.Bookmarks.Repair");

const PREF_BRANCH = "services.sync.repairs.bookmarks.";

// How long should we wait after sending a repair request before we give up?
const RESPONSE_INTERVAL_TIMEOUT = 60 * 60 * 24 * 3; // 3 days

// The maximum number of IDs we will request to be repaired. Beyond this
// number we assume that trying to repair may do more harm than good and may
// ask another client to wipe the server and reupload everything. Bug 1341972
// is tracking that work.
const MAX_REQUESTED_IDS = 1000;

class AbortRepairError extends Error {
  constructor(reason) {
    super();
    this.reason = reason;
  }
}

// The states we can be in.
const STATE = Object.freeze({
  NOT_REPAIRING: "",

  // We need to try to find another client to use.
  NEED_NEW_CLIENT: "repair.need-new-client",

  // We've sent the first request to a client.
  SENT_REQUEST: "repair.sent",

  // We've retried a request to a client.
  SENT_SECOND_REQUEST: "repair.sent-again",

  // There were no problems, but we've gone as far as we can.
  FINISHED: "repair.finished",

  // We've found an error that forces us to abort this entire repair cycle.
  ABORTED: "repair.aborted",
});

// The preferences we use to hold our state.
const PREF = Object.freeze({
  // If a repair is in progress, this is the generated GUID for the "flow ID".
  REPAIR_ID: "flowID",

  // The IDs we are currently trying to obtain via the repair process, space sep'd.
  REPAIR_MISSING_IDS: "ids",

  // The ID of the client we're currently trying to get the missing items from.
  REPAIR_CURRENT_CLIENT: "currentClient",

  // The IDs of the clients we've previously tried to get the missing items
  // from, space sep'd.
  REPAIR_PREVIOUS_CLIENTS: "previousClients",

  // The time, in seconds, when we initiated the most recent client request.
  REPAIR_WHEN: "when",

  // Our current state.
  CURRENT_STATE: "state",
});

class BookmarkRepairRequestor extends CollectionRepairRequestor {
  constructor(service = null) {
    super(service);
    this.prefs = new Preferences(PREF_BRANCH);
  }

  /* Check if any other clients connected to our account are current performing
     a repair. A thin wrapper which exists mainly for mocking during tests.
  */
  anyClientsRepairing(flowID) {
    return Doctor.anyClientsRepairing(this.service, "bookmarks", flowID);
  }

  /* Return a set of IDs we should request.
  */
  getProblemIDs(validationInfo) {
    // Set of ids of "known bad records". Many of the validation issues will
    // report duplicates -- if the server is missing a record, it is unlikely
    // to cause only a single problem.
    let ids = new Set();

    // Note that we allow any of the validation problem fields to be missing so
    // that tests have a slightly easier time, hence the `|| []` in each loop.

    // Missing children records when the parent exists but a child doesn't.
    for (let { child } of validationInfo.problems.missingChildren || []) {
      ids.add(child);
    }
    if (ids.size > MAX_REQUESTED_IDS) {
      return ids; // might as well give up here - we aren't going to repair.
    }

    // Orphans are when the child exists but the parent doesn't.
    // This could either be a problem in the child (it's wrong about the node
    // that should be its parent), or the parent could simply be missing.
    for (let { parent, id } of validationInfo.problems.orphans || []) {
      // Request both, to handle both cases
      ids.add(id);
      ids.add(parent);
    }
    if (ids.size > MAX_REQUESTED_IDS) {
      return ids; // might as well give up here - we aren't going to repair.
    }

    // Entries where we have the parent but know for certain that the child was
    // deleted.
    for (let { parent } of validationInfo.problems.deletedChildren || []) {
      ids.add(parent);
    }
    if (ids.size > MAX_REQUESTED_IDS) {
      return ids; // might as well give up here - we aren't going to repair.
    }

    // Entries where the child references a parent that we don't have, but we
    // know why: the parent was deleted.
    for (let { child } of validationInfo.problems.deletedParents || []) {
      ids.add(child);
    }
    if (ids.size > MAX_REQUESTED_IDS) {
      return ids; // might as well give up here - we aren't going to repair.
    }

    // Cases where the parent and child disagree about who the parent is.
    for (let { parent, child } of validationInfo.problems.parentChildMismatches || []) {
      // Request both, since we don't know which is right.
      ids.add(parent);
      ids.add(child);
    }
    if (ids.size > MAX_REQUESTED_IDS) {
      return ids; // might as well give up here - we aren't going to repair.
    }

    // Cases where multiple parents reference a child. We re-request both the
    // child, and all the parents that think they own it. This may be more than
    // we need, but I don't think we can safely make the assumption that the
    // child is right.
    for (let { parents, child } of validationInfo.problems.multipleParents || []) {
      for (let parent of parents) {
        ids.add(parent);
      }
      ids.add(child);
    }

    // XXX - any others we should consider?
    return ids;
  }

  _countServerOnlyFixableProblems(validationInfo) {
    const fixableProblems = ["clientMissing", "serverMissing", "serverDeleted"];
    return fixableProblems.reduce((numProblems, problemLabel) => {
      return numProblems + validationInfo.problems[problemLabel].length;
    }, 0);
  }

  tryServerOnlyRepairs(validationInfo) {
    if (this._countServerOnlyFixableProblems(validationInfo) == 0) {
      return false;
    }
    let engine = this.service.engineManager.get("bookmarks");
    for (let id of validationInfo.problems.serverMissing) {
      engine._modified.setWeak(id, { tombstone: false });
    }
    let toFetch = engine.toFetch.concat(validationInfo.problems.clientMissing,
                                        validationInfo.problems.serverDeleted);
    engine.toFetch = Array.from(new Set(toFetch));
    return true;
  }

  /* See if the repairer is willing and able to begin a repair process given
     the specified validation information.
     Returns true if a repair was started and false otherwise.
  */
  startRepairs(validationInfo, flowID) {
    if (this._currentState != STATE.NOT_REPAIRING) {
      log.info(`Can't start a repair - repair with ID ${this._flowID} is already in progress`);
      return false;
    }

    let ids = this.getProblemIDs(validationInfo);
    if (ids.size > MAX_REQUESTED_IDS) {
      log.info("Not starting a repair as there are over " + MAX_REQUESTED_IDS + " problems");
      let extra = { flowID, reason: `too many problems: ${ids.size}` };
      this.service.recordTelemetryEvent("repair", "aborted", undefined, extra)
      return false;
    }

    if (ids.size == 0) {
      log.info("Not starting a repair as there are no problems");
      return false;
    }

    if (this.anyClientsRepairing()) {
      log.info("Can't start repair, since other clients are already repairing bookmarks");
      let extra = { flowID, reason: "other clients repairing" };
      this.service.recordTelemetryEvent("repair", "aborted", undefined, extra)
      return false;
    }

    log.info(`Starting a repair, looking for ${ids.size} missing item(s)`);
    // setup our prefs to indicate we are on our way.
    this._flowID = flowID;
    this._currentIDs = Array.from(ids);
    this._currentState = STATE.NEED_NEW_CLIENT;
    this.service.recordTelemetryEvent("repair", "started", undefined, { flowID, numIDs: ids.size.toString() });
    return this.continueRepairs();
  }

  /* Work out what state our current repair request is in, and whether it can
     proceed to a new state.
     Returns true if we could continue the repair - even if the state didn't
     actually move. Returns false if we aren't actually repairing.
  */
  continueRepairs(response = null) {
    // Note that "ABORTED" and "FINISHED" should never be current when this
    // function returns - this function resets to NOT_REPAIRING in those cases.
    if (this._currentState == STATE.NOT_REPAIRING) {
      return false;
    }

    let state, newState;
    let abortReason;
    // we loop until the state doesn't change - but enforce a max of 10 times
    // to prevent errors causing infinite loops.
    for (let i = 0; i < 10; i++) {
      state = this._currentState;
      log.info("continueRepairs starting with state", state);
      try {
        newState = this._continueRepairs(state, response);
        log.info("continueRepairs has next state", newState);
      } catch (ex) {
        if (!(ex instanceof AbortRepairError)) {
          throw ex;
        }
        log.info(`Repair has been aborted: ${ex.reason}`);
        newState = STATE.ABORTED;
        abortReason = ex.reason;
      }

      if (newState == STATE.ABORTED) {
        break;
      }

      this._currentState = newState;
      Services.prefs.savePrefFile(null); // flush prefs.
      if (state == newState) {
        break;
      }
    }
    if (state != newState) {
      log.error("continueRepairs spun without getting a new state");
    }
    if (newState == STATE.FINISHED || newState == STATE.ABORTED) {
      let object = newState == STATE.FINISHED ? "finished" : "aborted";
      let extra = {
        flowID: this._flowID,
        numIDs: this._currentIDs.length.toString(),
      };
      if (abortReason) {
        extra.reason = abortReason;
      }
      this.service.recordTelemetryEvent("repair", object, undefined, extra);
      // reset our state and flush our prefs.
      this.prefs.resetBranch();
      Services.prefs.savePrefFile(null); // flush prefs.
    }
    return true;
  }

  _continueRepairs(state, response = null) {
    if (this.anyClientsRepairing(this._flowID)) {
      throw new AbortRepairError("other clients repairing");
    }
    switch (state) {
      case STATE.SENT_REQUEST:
      case STATE.SENT_SECOND_REQUEST:
        let flowID = this._flowID;
        let clientID = this._currentRemoteClient;
        if (!clientID) {
          throw new AbortRepairError(`In state ${state} but have no client IDs listed`);
        }
        if (response) {
          // We got an explicit response - let's see how we went.
          state = this._handleResponse(state, response);
          break;
        }
        // So we've sent a request - and don't yet have a response. See if the
        // client we sent it to has removed it from its list (ie, whether it
        // has synced since we wrote the request.)
        let client = this.service.clientsEngine.remoteClient(clientID);
        if (!client) {
          // hrmph - the client has disappeared.
          log.info(`previously requested client "${clientID}" has vanished - moving to next step`);
          state = STATE.NEED_NEW_CLIENT;
          let extra = {
            deviceID: this.service.identity.hashedDeviceID(clientID),
            flowID,
          }
          this.service.recordTelemetryEvent("repair", "abandon", "missing", extra);
          break;
        }
        if (this._isCommandPending(clientID, flowID)) {
          // So the command we previously sent is still queued for the client
          // (ie, that client is yet to have synced). Let's see if we should
          // give up on that client.
          let lastRequestSent = this.prefs.get(PREF.REPAIR_WHEN);
          let timeLeft = lastRequestSent + RESPONSE_INTERVAL_TIMEOUT - this._now();
          if (timeLeft <= 0) {
            log.info(`previous request to client ${clientID} is pending, but has taken too long`);
            state = STATE.NEED_NEW_CLIENT;
            // XXX - should we remove the command?
            let extra = {
              deviceID: this.service.identity.hashedDeviceID(clientID),
              flowID,
            }
            this.service.recordTelemetryEvent("repair", "abandon", "silent", extra);
            break;
          }
          // Let's continue to wait for that client to respond.
          log.trace(`previous request to client ${clientID} has ${timeLeft} seconds before we give up on it`);
          break;
        }
        // The command isn't pending - if this was the first request, we give
        // it another go (as that client may have cleared the command but is yet
        // to complete the sync)
        // XXX - note that this is no longer true - the responders don't remove
        // their command until they have written a response. This might mean
        // we could drop the entire STATE.SENT_SECOND_REQUEST concept???
        if (state == STATE.SENT_REQUEST) {
          log.info(`previous request to client ${clientID} was removed - trying a second time`);
          state = STATE.SENT_SECOND_REQUEST;
          this._writeRequest(clientID);
        } else {
          // this was the second time around, so give up on this client
          log.info(`previous 2 requests to client ${clientID} were removed - need a new client`);
          state = STATE.NEED_NEW_CLIENT;
        }
        break;

      case STATE.NEED_NEW_CLIENT:
        // We need to find a new client to request.
        let newClientID = this._findNextClient();
        if (!newClientID) {
          state = STATE.FINISHED;
          break;
        }
        this._addToPreviousRemoteClients(this._currentRemoteClient);
        this._currentRemoteClient = newClientID;
        this._writeRequest(newClientID);
        state = STATE.SENT_REQUEST;
        break;

      case STATE.ABORTED:
        break; // our caller will take the abort action.

      case STATE.FINISHED:
        break;

      case STATE.NOT_REPAIRING:
        // No repair is in progress. This is a common case, so only log trace.
        log.trace("continue repairs called but no repair in progress.");
        break;

      default:
        log.error(`continue repairs finds itself in an unknown state ${state}`);
        state = STATE.ABORTED;
        break;

    }
    return state;
  }

  /* Handle being in the SENT_REQUEST or SENT_SECOND_REQUEST state with an
     explicit response.
  */
  _handleResponse(state, response) {
    let clientID = this._currentRemoteClient;
    let flowID = this._flowID;

    if (response.flowID != flowID || response.clientID != clientID ||
        response.request != "upload") {
      log.info("got a response to a different repair request", response);
      // hopefully just a stale request that finally came in (either from
      // an entirely different repair flow, or from a client we've since
      // given up on.) It doesn't mean we need to abort though...
      return state;
    }
    // Pull apart the response and see if it provided everything we asked for.
    let remainingIDs = Array.from(CommonUtils.difference(this._currentIDs, response.ids));
    log.info(`repair response from ${clientID} provided "${response.ids}", remaining now "${remainingIDs}"`);
    this._currentIDs = remainingIDs;
    if (remainingIDs.length) {
      // try a new client for the remaining ones.
      state = STATE.NEED_NEW_CLIENT;
    } else {
      state = STATE.FINISHED;
    }
    // record telemetry about this
    let extra = {
      deviceID: this.service.identity.hashedDeviceID(clientID),
      flowID,
      numIDs: response.ids.length.toString(),
    }
    this.service.recordTelemetryEvent("repair", "response", "upload", extra);
    return state;
  }

  /* Issue a repair request to a specific client.
  */
  _writeRequest(clientID) {
    log.trace("writing repair request to client", clientID);
    let ids = this._currentIDs;
    if (!ids) {
      throw new AbortRepairError("Attempting to write a request, but there are no IDs");
    }
    let flowID = this._flowID;
    // Post a command to that client.
    let request = {
      collection: "bookmarks",
      request: "upload",
      requestor: this.service.clientsEngine.localID,
      ids,
      flowID,
    }
    this.service.clientsEngine.sendCommand("repairRequest", [request], clientID, { flowID });
    this.prefs.set(PREF.REPAIR_WHEN, Math.floor(this._now()));
    // record telemetry about this
    let extra = {
      deviceID: this.service.identity.hashedDeviceID(clientID),
      flowID,
      numIDs: ids.length.toString(),
    }
    this.service.recordTelemetryEvent("repair", "request", "upload", extra);
  }

  _findNextClient() {
    let alreadyDone = this._getPreviousRemoteClients();
    alreadyDone.push(this._currentRemoteClient);
    let remoteClients = this.service.clientsEngine.remoteClients;
    // we want to consider the most-recently synced clients first.
    remoteClients.sort((a, b) => b.serverLastModified - a.serverLastModified);
    for (let client of remoteClients) {
      log.trace("findNextClient considering", client);
      if (alreadyDone.indexOf(client.id) == -1 && this._isSuitableClient(client)) {
        return client.id;
      }
    }
    log.trace("findNextClient found no client");
    return null;
  }

  /* Is the passed client record suitable as a repair responder?
  */
  _isSuitableClient(client) {
    // filter only desktop firefox running > 53 (ie, any 54)
    return (client.type == DEVICE_TYPE_DESKTOP &&
            Services.vc.compare(client.version, 53) > 0);
  }

  /* Is our command still in the "commands" queue for the specific client?
  */
  _isCommandPending(clientID, flowID) {
    // getClientCommands() is poorly named - it's only outgoing commands
    // from us we have yet to write. For our purposes, we want to check
    // them and commands previously written (which is in .commands)
    let commands = [...this.service.clientsEngine.getClientCommands(clientID),
                    ...this.service.clientsEngine.remoteClient(clientID).commands || []];
    for (let command of commands) {
      if (command.command != "repairRequest" || command.args.length != 1) {
        continue;
      }
      let arg = command.args[0];
      if (arg.collection == "bookmarks" && arg.request == "upload" &&
          arg.flowID == flowID) {
        return true;
      }
    }
    return false;
  }

  // accessors for our prefs.
  get _currentState() {
    return this.prefs.get(PREF.CURRENT_STATE, STATE.NOT_REPAIRING);
  }
  set _currentState(newState) {
    this.prefs.set(PREF.CURRENT_STATE, newState);
  }

  get _currentIDs() {
    let ids = this.prefs.get(PREF.REPAIR_MISSING_IDS, "");
    return ids.length ? ids.split(" ") : [];
  }
  set _currentIDs(arrayOfIDs) {
    this.prefs.set(PREF.REPAIR_MISSING_IDS, arrayOfIDs.join(" "));
  }

  get _currentRemoteClient() {
    return this.prefs.get(PREF.REPAIR_CURRENT_CLIENT);
  }
  set _currentRemoteClient(clientID) {
    this.prefs.set(PREF.REPAIR_CURRENT_CLIENT, clientID);
  }

  get _flowID() {
    return this.prefs.get(PREF.REPAIR_ID);
  }
  set _flowID(val) {
    this.prefs.set(PREF.REPAIR_ID, val);
  }

  // use a function for this pref to offer somewhat sane semantics.
  _getPreviousRemoteClients() {
    let alreadyDone = this.prefs.get(PREF.REPAIR_PREVIOUS_CLIENTS, "");
    return alreadyDone.length ? alreadyDone.split(" ") : [];
  }
  _addToPreviousRemoteClients(clientID) {
    let arrayOfClientIDs = this._getPreviousRemoteClients();
    arrayOfClientIDs.push(clientID);
    this.prefs.set(PREF.REPAIR_PREVIOUS_CLIENTS, arrayOfClientIDs.join(" "));
  }

  /* Used for test mocks.
  */
  _now() {
    // We use the server time, which is SECONDS
    return AsyncResource.serverTime;
  }
}

/* An object that responds to repair requests initiated by some other device.
*/
class BookmarkRepairResponder extends CollectionRepairResponder {
  async repair(request, rawCommand) {
    if (request.request != "upload") {
      this._abortRepair(request, rawCommand,
                        `Don't understand request type '${request.request}'`);
      return;
    }

    // Note that we don't try and guard against multiple repairs being in
    // progress as we don't do anything too smart that could cause problems,
    // but just upload items. If we get any smarter we should re-think this
    // (but when we do, note that checking this._currentState isn't enough as
    // this responder is not a singleton)

    this._currentState = {
      request,
      rawCommand,
      ids: [],
    }

    try {
      let engine = this.service.engineManager.get("bookmarks");
      let { toUpload, toDelete } = await this._fetchItemsToUpload(request);

      if (toUpload.size || toDelete.size) {
        log.debug(`repair request will upload ${toUpload.size} items and delete ${toDelete.size} items`);
        // whew - now add these items to the tracker "weakly" (ie, they will not
        // persist in the case of a restart, but that's OK - we'll then end up here
        // again) and also record them in the response we send back.
        for (let id of toUpload) {
          engine._modified.setWeak(id, { tombstone: false });
          this._currentState.ids.push(id);
        }
        for (let id of toDelete) {
          engine._modified.setWeak(id, { tombstone: true });
          this._currentState.ids.push(id);
        }

        // We have arranged for stuff to be uploaded, so wait until that's done.
        Svc.Obs.add("weave:engine:sync:uploaded", this.onUploaded, this);
        // and record in telemetry that we got this far - just incase we never
        // end up doing the upload for some obscure reason.
        let eventExtra = {
          flowID: request.flowID,
          numIDs: this._currentState.ids.length.toString(),
        };
        this.service.recordTelemetryEvent("repairResponse", "uploading", undefined, eventExtra);
      } else {
        // We were unable to help with the repair, so report that we are done.
        this._finishRepair();
      }
    } catch (ex) {
      if (Async.isShutdownException(ex)) {
        // this repair request will be tried next time.
        throw ex;
      }
      // On failure, we still write a response so the requestor knows to move
      // on, but we record the failure reason in telemetry.
      log.error("Failed to respond to the repair request", ex);
      this._currentState.failureReason = SyncTelemetry.transformError(ex);
      this._finishRepair();
    }
  }

  async _fetchItemsToUpload(request) {
    let toUpload = new Set(); // items we will upload.
    let toDelete = new Set(); // items we will delete.

    let requested = new Set(request.ids);

    let engine = this.service.engineManager.get("bookmarks");
    // Determine every item that may be impacted by the requested IDs - eg,
    // this may include children if a requested ID is a folder.
    // Turn an array of { syncId, syncable } into a map of syncId -> syncable.
    let repairable = await PlacesSyncUtils.bookmarks.fetchSyncIdsForRepair(request.ids);
    if (repairable.length == 0) {
      // server will get upset if we request an empty set, and we can't do
      // anything in that case, so bail now.
      return { toUpload, toDelete };
    }

    // which of these items exist on the server?
    let itemSource = engine.itemSource();
    itemSource.ids = repairable.map(item => item.syncId);
    log.trace(`checking the server for items`, itemSource.ids);
    let itemsResponse = await itemSource.get();
    // If the response failed, don't bother trying to parse the output.
    // Throwing here means we abort the repair, which isn't ideal for transient
    // errors (eg, no network, 500 service outage etc), but we don't currently
    // have a sane/safe way to try again later (we'd need to implement a kind
    // of timeout, otherwise we might end up retrying forever and never remove
    // our request command.) Bug 1347805.
    if (!itemsResponse.success) {
      throw new Error(`request for server IDs failed: ${itemsResponse.status}`);
    }
    let existRemotely = new Set(JSON.parse(itemsResponse));
    // We need to be careful about handing the requested items:
    // * If the item exists locally but isn't in the tree of items we sync
    //   (eg, it might be a left-pane item or similar, we write a tombstone.
    // * If the item exists locally as a folder, we upload the folder and any
    //   children which don't exist on the server. (Note that we assume the
    //   parents *do* exist)
    // Bug 1343101 covers additional issues we might repair in the future.
    for (let { syncId: id, syncable } of repairable) {
      if (requested.has(id)) {
        if (syncable) {
          log.debug(`repair request to upload item '${id}' which exists locally; uploading`);
          toUpload.add(id);
        } else {
          // explicitly requested and not syncable, so tombstone.
          log.debug(`repair request to upload item '${id}' but it isn't under a syncable root; writing a tombstone`);
          toDelete.add(id);
        }
      // The item wasn't explicitly requested - only upload if it is syncable
      // and doesn't exist on the server.
      } else if (syncable && !existRemotely.has(id)) {
        log.debug(`repair request found related item '${id}' which isn't on the server; uploading`);
        toUpload.add(id);
      } else if (!syncable && existRemotely.has(id)) {
        log.debug(`repair request found non-syncable related item '${id}' on the server; writing a tombstone`);
        toDelete.add(id);
      } else {
        log.debug(`repair request found related item '${id}' which we will not upload; ignoring`);
      }
    }
    return { toUpload, toDelete };
  }

  onUploaded(subject, data) {
    if (data != "bookmarks") {
      return;
    }
    Svc.Obs.remove("weave:engine:sync:uploaded", this.onUploaded, this);
    log.debug(`bookmarks engine has uploaded stuff - creating a repair response`);
    this._finishRepair();
  }

  _finishRepair() {
    let clientsEngine = this.service.clientsEngine;
    let flowID = this._currentState.request.flowID;
    let response = {
      request: this._currentState.request.request,
      collection: "bookmarks",
      clientID: clientsEngine.localID,
      flowID,
      ids: this._currentState.ids,
    }
    let clientID = this._currentState.request.requestor;
    clientsEngine.sendCommand("repairResponse", [response], clientID, { flowID });
    // and nuke the request from our client.
    clientsEngine.removeLocalCommand(this._currentState.rawCommand);
    let eventExtra = {
      flowID,
      numIDs: response.ids.length.toString(),
    }
    if (this._currentState.failureReason) {
      // *sob* - recording this in "extra" means the value must be a string of
      // max 85 chars.
      eventExtra.failureReason = JSON.stringify(this._currentState.failureReason).substring(0, 85)
      this.service.recordTelemetryEvent("repairResponse", "failed", undefined, eventExtra);
    } else {
      this.service.recordTelemetryEvent("repairResponse", "finished", undefined, eventExtra);
    }
    this._currentState = null;
  }

  _abortRepair(request, rawCommand, why) {
    log.warn(`aborting repair request: ${why}`);
    this.service.clientsEngine.removeLocalCommand(rawCommand);
    // record telemetry for this.
    let eventExtra = {
      flowID: request.flowID,
      reason: why,
    };
    this.service.recordTelemetryEvent("repairResponse", "aborted", undefined, eventExtra);
    // We could also consider writing a response here so the requestor can take
    // some immediate action rather than timing out, but we abort only in cases
    // that should be rare, so let's wait and see what telemetry tells us.
  }
}

/* Exposed in case another module needs to understand our state.
*/
BookmarkRepairRequestor.STATE = STATE;
BookmarkRepairRequestor.PREF = PREF;
