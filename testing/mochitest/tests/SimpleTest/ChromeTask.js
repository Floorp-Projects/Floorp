/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function ChromeTask_ChromeScript() {
  /* eslint-env mozilla/chrome-script */

  "use strict";

  const { Assert: AssertCls } = ChromeUtils.import(
    "resource://testing-common/Assert.jsm"
  );

  addMessageListener("chrome-task:spawn", async function(aData) {
    let id = aData.id;
    let source = aData.runnable || "()=>{}";

    function getStack(aStack) {
      let frames = [];
      for (let frame = aStack; frame; frame = frame.caller) {
        frames.push(frame.filename + ":" + frame.name + ":" + frame.lineNumber);
      }
      return frames.join("\n");
    }

    /* eslint-disable no-unused-vars */
    var Assert = new AssertCls((err, message, stack) => {
      sendAsyncMessage("chrome-task:test-result", {
        id,
        condition: !err,
        name: err ? err.message : message,
        stack: getStack(err ? err.stack : stack),
      });
    });

    var ok = Assert.ok.bind(Assert);
    var is = Assert.equal.bind(Assert);
    var isnot = Assert.notEqual.bind(Assert);

    function todo(expr, name) {
      sendAsyncMessage("chrome-task:test-todo", { id, expr, name });
    }

    function todo_is(a, b, name) {
      sendAsyncMessage("chrome-task:test-todo_is", { id, a, b, name });
    }

    function info(name) {
      sendAsyncMessage("chrome-task:test-info", { id, name });
    }
    /* eslint-enable no-unused-vars */

    try {
      let runnablestr = `
        (() => {
          return (${source});
        })();`;

      // eslint-disable-next-line no-eval
      let runnable = eval(runnablestr);
      let result = await runnable.call(this, aData.arg);
      sendAsyncMessage("chrome-task:complete", {
        id,
        result,
      });
    } catch (ex) {
      sendAsyncMessage("chrome-task:complete", {
        id,
        error: ex.toString(),
      });
    }
  });
}

/**
 * This object provides the public module functions.
 */
var ChromeTask = {
  /**
   * the ChromeScript if it has already been loaded.
   */
  _chromeScript: null,

  /**
   * Mapping from message id to associated promise.
   */
  _promises: new Map(),

  /**
   * Incrementing integer to generate unique message id.
   */
  _messageID: 1,

  /**
   * Creates and starts a new task in the chrome process.
   *
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
  spawn: function ChromeTask_spawn(arg, task) {
    // Load the frame script if needed.
    let handle = ChromeTask._chromeScript;
    if (!handle) {
      handle = SpecialPowers.loadChromeScript(ChromeTask_ChromeScript);
      handle.addMessageListener("chrome-task:complete", ChromeTask.onComplete);
      handle.addMessageListener("chrome-task:test-result", ChromeTask.onResult);
      handle.addMessageListener("chrome-task:test-info", ChromeTask.onInfo);
      handle.addMessageListener("chrome-task:test-todo", ChromeTask.onTodo);
      handle.addMessageListener(
        "chrome-task:test-todo_is",
        ChromeTask.onTodoIs
      );
      ChromeTask._chromeScript = handle;
    }

    let deferred = {};
    deferred.promise = new Promise((resolve, reject) => {
      deferred.resolve = resolve;
      deferred.reject = reject;
    });

    let id = ChromeTask._messageID++;
    ChromeTask._promises.set(id, deferred);

    handle.sendAsyncMessage("chrome-task:spawn", {
      id,
      runnable: task.toString(),
      arg,
    });

    return deferred.promise;
  },

  onComplete(aData) {
    let deferred = ChromeTask._promises.get(aData.id);
    ChromeTask._promises.delete(aData.id);

    if (aData.error) {
      deferred.reject(aData.error);
    } else {
      deferred.resolve(aData.result);
    }
  },

  onResult(aData) {
    SimpleTest.record(aData.condition, aData.name);
  },

  onInfo(aData) {
    SimpleTest.info(aData.name);
  },

  onTodo(aData) {
    SimpleTest.todo(aData.expr, aData.name);
  },

  onTodoIs(aData) {
    SimpleTest.todo_is(aData.a, aData.b, aData.name);
  },
};
