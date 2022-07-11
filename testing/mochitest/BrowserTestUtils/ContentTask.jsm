/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContentTask"];

const FRAME_SCRIPT = "resource://testing-common/content-task.js";

/**
 * Keeps track of whether the frame script was already loaded.
 */
var gFrameScriptLoaded = false;

/**
 * Mapping from message id to associated promise.
 */
var gPromises = new Map();

/**
 * Incrementing integer to generate unique message id.
 */
var gMessageID = 1;

/**
 * This object provides the public module functions.
 */
var ContentTask = {
  /**
   * _testScope saves the current testScope from
   * browser-test.js. This is used to implement SimpleTest functions
   * like ok() and is() in the content process. The scope is only
   * valid for tasks spawned in the current test, so we keep track of
   * the ID of the first task spawned in this test (_scopeValidId).
   */
  _testScope: null,
  _scopeValidId: 0,

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
      Services.mm.loadFrameScript(FRAME_SCRIPT, true);
      gFrameScriptLoaded = true;
    }

    let deferred = {};
    deferred.promise = new Promise((resolve, reject) => {
      deferred.resolve = resolve;
      deferred.reject = reject;
    });

    let id = gMessageID++;
    gPromises.set(id, deferred);

    browser.messageManager.sendAsyncMessage("content-task:spawn", {
      id,
      runnable: task.toString(),
      arg,
    });

    return deferred.promise;
  },

  setTestScope(scope) {
    this._testScope = scope;
    this._scopeValidId = gMessageID;
  },
};

var ContentMessageListener = {
  receiveMessage(aMessage) {
    let id = aMessage.data.id;

    if (id < ContentTask._scopeValidId) {
      throw new Error("test result returned after test finished");
    }

    if (aMessage.name == "content-task:complete") {
      let deferred = gPromises.get(id);
      gPromises.delete(id);

      if (aMessage.data.error) {
        deferred.reject(aMessage.data.error);
      } else {
        deferred.resolve(aMessage.data.result);
      }
    } else if (aMessage.name == "content-task:test-result") {
      let data = aMessage.data;
      ContentTask._testScope.record(
        data.condition,
        data.name,
        null,
        data.stack
      );
    } else if (aMessage.name == "content-task:test-info") {
      ContentTask._testScope.info(aMessage.data.name);
    } else if (aMessage.name == "content-task:test-todo") {
      ContentTask._testScope.todo(aMessage.data.expr, aMessage.data.name);
    } else if (aMessage.name == "content-task:test-todo_is") {
      ContentTask._testScope.todo_is(
        aMessage.data.a,
        aMessage.data.b,
        aMessage.data.name
      );
    }
  },
};

Services.mm.addMessageListener("content-task:complete", ContentMessageListener);
Services.mm.addMessageListener(
  "content-task:test-result",
  ContentMessageListener
);
Services.mm.addMessageListener(
  "content-task:test-info",
  ContentMessageListener
);
Services.mm.addMessageListener(
  "content-task:test-todo",
  ContentMessageListener
);
Services.mm.addMessageListener(
  "content-task:test-todo_is",
  ContentMessageListener
);
