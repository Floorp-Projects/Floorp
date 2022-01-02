/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

function sendBackResponse(evalResult, e) {
  const output = { result: evalResult, error: "", errorStack: "" };
  if (e) {
    output.error = e.toString();
    output.errorStack = e.stack;
  }
  process.send(output);
}

process.on("message", msg => {
  const code = msg.code;
  let evalResult = null;
  try {
    // eslint-disable-next-line no-eval
    evalResult = eval(code);
    if (evalResult instanceof Promise) {
      evalResult
        .then(x => sendBackResponse(x))
        .catch(e => sendBackResponse(undefined, e));
      return;
    }
  } catch (e) {
    sendBackResponse(undefined, e);
    return;
  }
  sendBackResponse(evalResult);
});
