/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);

addMessageListener("content-task:spawn", function (msg) {
  let id = msg.data.id;
  let source = msg.data.runnable || "()=>{}";

  function getStack(aStack) {
    let frames = [];
    for (let frame = aStack; frame; frame = frame.caller) {
      frames.push(frame.filename + ":" + frame.name + ":" + frame.lineNumber);
    }
    return frames.join("\n");
  }

  function ok(condition, name, diag) {
    let stack = getStack(Components.stack.caller);
    sendAsyncMessage("content-task:test-result", {
      id, condition: !!condition, name, diag, stack
    });
  }

  function is(a, b, name) {
    ok(Object.is(a, b), name, "Got " + a + ", expected " + b);
  }

  function isnot(a, b, name) {
    ok(!Object.is(a, b), name, "Didn't expect " + a + ", but got it");
  }

  function info(name) {
    sendAsyncMessage("content-task:test-info", {id, name});
  }

  try {
    let runnablestr = `
      (() => {
        return (${source});
      })();`

    let runnable = eval(runnablestr);
    let iterator = runnable.call(this, msg.data.arg);
    Task.spawn(iterator).then((val) => {
      sendAsyncMessage("content-task:complete", {
        id: id,
        result: val,
      });
    }, (e) => {
      sendAsyncMessage("content-task:complete", {
        id: id,
        error: e.toString(),
      });
    });
  } catch (e) {
    sendAsyncMessage("content-task:complete", {
      id: id,
      error: e.toString(),
    });
  }
});
