/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [
  "ContentTask"
];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;
Cu.import("resource://gre/modules/Promise.jsm");

const FRAME_SCRIPT = "chrome://mochikit/content/tests/BrowserTestUtils/content-task.js";

const globalMM = Cc["@mozilla.org/globalmessagemanager;1"]
                   .getService(Ci.nsIMessageListenerManager);

/**
 * Keeps track of whether the frame script was already loaded.
 */
let gFrameScriptLoaded = false;

/**
 * Mapping from message id to associated promise.
 */
let gPromises = new Map();

/**
 * Incrementing integer to generate unique message id.
 */
let gMessageID = 1;

/**
 * This object provides the public module functions.
 */
this.ContentTask = {
  /**
   * Creates and starts a new task in a browser's content.
   *
   * @param browser A xul:browser
   * @param arg A single serializable argument that will be passed to the
   *             task when executed on the content process.
   * @param task
   *        - A generator or function which will be serialized and sent to
   *          the remote browser to be executed. Unlike Task.spawn, this
   *          argument may not be an iterator as it will be serialized and
   *          sent to the remote browser.
   * @return A promise object where you can register completion callbacks to be
   *         called when the task terminates.
   * @resolves With the final returned value of the task if it executes
   *           successfully.
   * @rejects An error message if execution fails.
   */
  spawn: function ContentTask_spawn(browser, arg, task) {
    // Load the frame script if needed.
    if (!gFrameScriptLoaded) {
      globalMM.loadFrameScript(FRAME_SCRIPT, true);
      gFrameScriptLoaded = true;
    }

    let deferred = {};
    deferred.promise = new Promise((resolve, reject) => {
      deferred.resolve = resolve;
      deferred.reject = reject;
    });

    let id = gMessageID++;
    gPromises.set(id, deferred);

    browser.messageManager.sendAsyncMessage(
      "content-task:spawn",
      {
        id: id,
        runnable: task.toString(),
        arg: arg,
      });

    return deferred.promise;
  },
};

let ContentMessageListener = {
  receiveMessage(aMessage) {
    let id = aMessage.data.id
    let deferred = gPromises.get(id);
    gPromises.delete(id);

    if (aMessage.data.error) {
      deferred.reject(aMessage.data.error);
    } else {
      deferred.resolve(aMessage.data.result);
    }
  },
};
Cc["@mozilla.org/globalmessagemanager;1"].getService(Ci.nsIMessageListenerManager)
  .addMessageListener("content-task:complete", ContentMessageListener);
