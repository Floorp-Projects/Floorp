/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

let { Task } = ChromeUtils.import("resource://testing-common/Task.jsm");
let { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { Assert: AssertCls } = ChromeUtils.import(
  "resource://testing-common/Assert.jsm"
);

// Injects EventUtils into ContentTask scope. To avoid leaks, this does not hold on
// to the window global. This means you **need** to pass the window as an argument to
// the individual EventUtils functions.
// See SimpleTest/EventUtils.js for documentation.
var EventUtils = {};

EventUtils.window = {};
EventUtils.parent = EventUtils.window;
EventUtils._EU_Ci = Ci;
EventUtils._EU_Cc = Cc;
EventUtils.KeyboardEvent = content.KeyboardEvent;
EventUtils.navigator = content.navigator;

EventUtils.synthesizeClick = element =>
  new Promise(resolve => {
    element.addEventListener(
      "click",
      function() {
        resolve();
      },
      { once: true }
    );

    EventUtils.synthesizeMouseAtCenter(
      element,
      { type: "mousedown", isSynthesized: false },
      content
    );
    EventUtils.synthesizeMouseAtCenter(
      element,
      { type: "mouseup", isSynthesized: false },
      content
    );
  });

try {
  Services.scriptloader.loadSubScript(
    "chrome://mochikit/content/tests/SimpleTest/EventUtils.js",
    EventUtils
  );
} catch (e) {
  // There are some xpcshell tests which may use ContentTask.
  // Just ignore if loading EventUtils fails. Tests that need it
  // will fail anyway.
  EventUtils = null;
}

addMessageListener("content-task:spawn", function(msg) {
  let id = msg.data.id;
  let source = msg.data.runnable || "()=>{}";

  function getStack(aStack) {
    let frames = [];
    for (let frame = aStack; frame; frame = frame.caller) {
      frames.push(frame.filename + ":" + frame.name + ":" + frame.lineNumber);
    }
    return frames.join("\n");
  }

  var Assert = new AssertCls((err, message, stack) => {
    sendAsyncMessage("content-task:test-result", {
      id,
      condition: !err,
      name: err ? err.message : message,
      stack: getStack(err ? err.stack : stack),
    });
  });

  /* eslint-disable no-unused-vars */
  var ok = Assert.ok.bind(Assert);
  var is = Assert.equal.bind(Assert);
  var isnot = Assert.notEqual.bind(Assert);

  function todo(expr, name) {
    sendAsyncMessage("content-task:test-todo", { id, expr, name });
  }

  function todo_is(a, b, name) {
    sendAsyncMessage("content-task:test-todo_is", { id, a, b, name });
  }

  function info(name) {
    sendAsyncMessage("content-task:test-info", { id, name });
  }
  /* eslint-enable no-unused-vars */

  try {
    let runnablestr = `
      (() => {
        return (${source});
      })();`;

    // eslint-disable-next-line no-eval
    let runnable = eval(runnablestr);
    let iterator = runnable.call(this, msg.data.arg);
    Task.spawn(iterator).then(
      val => {
        sendAsyncMessage("content-task:complete", {
          id,
          result: val,
        });
      },
      e => {
        sendAsyncMessage("content-task:complete", {
          id,
          error: e.toString(),
        });
      }
    );
  } catch (e) {
    sendAsyncMessage("content-task:complete", {
      id,
      error: e.toString(),
    });
  }
});
