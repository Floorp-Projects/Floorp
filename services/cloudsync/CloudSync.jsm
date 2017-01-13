/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["CloudSync"];

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Adapters",
                                  "resource://gre/modules/CloudSyncAdapters.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Local",
                                  "resource://gre/modules/CloudSyncLocal.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Bookmarks",
                                  "resource://gre/modules/CloudSyncBookmarks.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Tabs",
                                  "resource://gre/modules/CloudSyncTabs.jsm");

var API_VERSION = 1;

var _CloudSync = function () {
};

_CloudSync.prototype = {
  _adapters: null,

  get adapters () {
    if (!this._adapters) {
      this._adapters = new Adapters();
    }
    return this._adapters;
  },

  _bookmarks: null,

  get bookmarks () {
    if (!this._bookmarks) {
      this._bookmarks = new Bookmarks();
    }
    return this._bookmarks;
  },

  _local: null,

  get local () {
    if (!this._local) {
      this._local = new Local();
    }
    return this._local;
  },

  _tabs: null,

  get tabs () {
    if (!this._tabs) {
      this._tabs = new Tabs();
    }
    return this._tabs;
  },

  get tabsReady () {
    return this._tabs ? true: false;
  },

  get version () {
    return API_VERSION;
  },
};

this.CloudSync = function CloudSync () {
  return _cloudSyncInternal.instance;
};

Object.defineProperty(CloudSync, "ready", {
  get: function () {
    return _cloudSyncInternal.ready;
  }
});

var _cloudSyncInternal = {
  instance: null,
  ready: false,
};

XPCOMUtils.defineLazyGetter(_cloudSyncInternal, "instance", function () {
  _cloudSyncInternal.ready = true;
  return new _CloudSync();
}.bind(this));
