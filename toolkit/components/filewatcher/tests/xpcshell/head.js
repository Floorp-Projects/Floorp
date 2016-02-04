/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);

function makeWatcher() {
  let watcher =
    Cc['@mozilla.org/toolkit/filewatcher/native-file-watcher;1']
      .getService(Ci.nsINativeFileWatcherService);
  return watcher;
}

function promiseAddPath(watcher, resource, onChange=null, onError=null) {
  return new Promise(resolve =>
    watcher.addPath(resource, onChange, onError, resolve)
  );
}

function promiseRemovePath(watcher, resource, onChange=null, onError=null) {
  return new Promise(resolve =>
    watcher.removePath(resource, onChange, onError, resolve)
  );
}
