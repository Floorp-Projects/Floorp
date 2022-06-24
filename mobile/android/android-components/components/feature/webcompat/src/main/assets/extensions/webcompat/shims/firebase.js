/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1767407 - Shim Firebase
 *
 * Sites relying on firebase-messaging.js will break in Private
 * browsing mode because it assumes that they require service
 * workers and indexedDB, when they generally do not.
 */

/* globals cloneInto */

if (!window.wrappedJSObject.serviceWorker) {
  const win = window.wrappedJSObject;
  const emptyArr = new win.Array();
  const emptyMsg = cloneInto({ message: "" }, window);

  const idb = {
    open: () => win.Promise.reject(emptyMsg),
  };

  const sw = {
    addEventListener() {},
    getRegistrations: () => win.Promise.resolve(emptyArr),
    register: () => win.Promise.reject(emptyMsg),
  };

  Object.defineProperty(win, "indexedDB", {
    value: cloneInto(idb, window, { cloneFunctions: true }),
  });

  Object.defineProperty(navigator.wrappedJSObject, "serviceWorker", {
    value: cloneInto(sw, window, { cloneFunctions: true }),
  });
}
