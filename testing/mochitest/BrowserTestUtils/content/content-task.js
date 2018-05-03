/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

ChromeUtils.import("resource://gre/modules/Task.jsm", this);
ChromeUtils.import("resource://testing-common/ContentTaskUtils.jsm", this);
const AssertCls = ChromeUtils.import("resource://testing-common/Assert.jsm", null).Assert;

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
      stack: getStack(err ? err.stack : stack)
    });
  });

  /* eslint-disable no-unused-vars */
  var ok = Assert.ok.bind(Assert);
  var is = Assert.equal.bind(Assert);
  var isnot = Assert.notEqual.bind(Assert);

  function todo(expr, name) {
    sendAsyncMessage("content-task:test-todo", {id, expr, name});
  }

  function info(name) {
    sendAsyncMessage("content-task:test-info", {id, name});
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
    Task.spawn(iterator).then((val) => {
      sendAsyncMessage("content-task:complete", {
        id,
        result: val,
      });
    }, (e) => {
      sendAsyncMessage("content-task:complete", {
        id,
        error: e.toString(),
      });
    });
  } catch (e) {
    sendAsyncMessage("content-task:complete", {
      id,
      error: e.toString(),
    });
  }
});
