/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

let { ContentTaskUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ContentTaskUtils.sys.mjs"
);
const { Assert: AssertCls } = ChromeUtils.importESModule(
  "resource://testing-common/Assert.sys.mjs"
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

// Injects EventUtils into ContentTask scope. To avoid leaks, this does not hold on
// to the window global. This means you **need** to pass the window as an argument to
// the individual EventUtils functions.
// See SimpleTest/EventUtils.js for documentation.
var EventUtils = {
  _EU_Ci: Ci,
  _EU_Cc: Cc,
  KeyboardEvent: content.KeyboardEvent,
  navigator: content.navigator,
  setTimeout,
  window: {},
};

EventUtils.synthesizeClick = element =>
  new Promise(resolve => {
    element.addEventListener(
      "click",
      function () {
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

addMessageListener("content-task:spawn", async function (msg) {
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
    let result = await runnable.call(this, msg.data.arg);
    sendAsyncMessage("content-task:complete", {
      id,
      result,
    });
  } catch (ex) {
    sendAsyncMessage("content-task:complete", {
      id,
      error: ex.toString(),
    });
  }
});
