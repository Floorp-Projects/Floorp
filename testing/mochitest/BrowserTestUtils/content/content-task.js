/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://testing-common/ContentTaskUtils.jsm", this);
const AssertCls = Cu.import("resource://testing-common/Assert.jsm", null).Assert;

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

  var Assert = new AssertCls((err, message, stack) => {
    sendAsyncMessage("content-task:test-result", {
      id: id,
      condition: !err,
      name: err ? err.message : message,
      stack: getStack(err ? err.stack : stack)
    });
  });

  var ok = Assert.ok.bind(Assert);
  var is = Assert.equal.bind(Assert);
  var isnot = Assert.notEqual.bind(Assert);

  function todo(expr, name) {
    sendAsyncMessage("content-task:test-todo", {id, expr, name});
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
