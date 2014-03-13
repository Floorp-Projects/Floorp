/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["ClusterManager"];

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/policies.js");
Cu.import("resource://services-sync/util.js");

/**
 * Contains code for managing the Sync cluster we are in.
 */
this.ClusterManager = function ClusterManager(service) {
  this._log = Log.repository.getLogger("Sync.Service");
  this._log.level = Log.Level[Svc.Prefs.get("log.logger.service.main")];

  this.service = service;
}
ClusterManager.prototype = {
  get identity() {
    return this.service.identity;
  },

  /**
   * Obtain the cluster for the current user.
   *
   * Returns the string URL of the cluster or null on error.
   */
  _findCluster: function _findCluster() {
    this._log.debug("Finding cluster for user " + this.identity.username);

    // This should ideally use UserAPI10Client but the legacy hackiness is
    // strong with this code.
    let fail;
    let url = this.service.userAPIURI + this.identity.username + "/node/weave";
    let res = this.service.resource(url);
    try {
      let node = res.get();
      switch (node.status) {
        case 400:
          this.service.status.login = LOGIN_FAILED_LOGIN_REJECTED;
          fail = "Find cluster denied: " + this.service.errorHandler.errorStr(node);
          break;
        case 404:
          this._log.debug("Using serverURL as data cluster (multi-cluster support disabled)");
          return this.service.serverURL;
        case 0:
        case 200:
          if (node == "null") {
            node = null;
          }
          this._log.trace("_findCluster successfully returning " + node);
          return node;
        default:
          this.service.errorHandler.checkServerError(node);
          fail = "Unexpected response code: " + node.status;
          break;
      }
    } catch (e) {
      this._log.debug("Network error on findCluster");
      this.service.status.login = LOGIN_FAILED_NETWORK_ERROR;
      this.service.errorHandler.checkServerError(e);
      fail = e;
    }
    throw fail;
  },

  /**
   * Determine the cluster for the current user and update state.
   */
  setCluster: function setCluster() {
    // Make sure we didn't get some unexpected response for the cluster.
    let cluster = this._findCluster();
    this._log.debug("Cluster value = " + cluster);
    if (cluster == null) {
      return false;
    }

    // Don't update stuff if we already have the right cluster
    if (cluster == this.service.clusterURL) {
      return false;
    }

    this._log.debug("Setting cluster to " + cluster);
    this.service.clusterURL = cluster;
    Svc.Prefs.set("lastClusterUpdate", Date.now().toString());

    return true;
  },

  getUserBaseURL: function getUserBaseURL() {
    // Legacy Sync and FxA Sync construct the userBaseURL differently. Legacy
    // Sync appends path components onto an empty path, and in FxA Sync, the
    // token server constructs this for us in an opaque manner. Since the
    // cluster manager already sets the clusterURL on Service and also has
    // access to the current identity, we added this functionality here.

    // If the clusterURL hasn't been set, the userBaseURL shouldn't be set
    // either. Some tests expect "undefined" to be returned here.
    if (!this.service.clusterURL) {
      return undefined;
    }
    let storageAPI = this.service.clusterURL + SYNC_API_VERSION + "/";
    return storageAPI + this.identity.username + "/";
  }
};
Object.freeze(ClusterManager.prototype);
