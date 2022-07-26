/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

var idleCallbackHandle;

function _idleCallbackHandler() {
  content.window.cancelIdleCallback(idleCallbackHandle);
  sendAsyncMessage("PageLoader:IdleCallbackReceived", {});
}

function setIdleCallback() {
  idleCallbackHandle = content.window.requestIdleCallback(_idleCallbackHandler);
  sendAsyncMessage("PageLoader:IdleCallbackSet", {});
}

function contentLoadHandlerCallback(cb) {
  function _handler(e) {
    if (e.originalTarget.defaultView == content) {
      content.wrappedJSObject.tpRecordTime = Cu.exportFunction((t, s, n) => {
        sendAsyncMessage("PageLoader:RecordTime", {
          time: t,
          startTime: s,
          testName: n,
        });
      }, content);
      content.setTimeout(cb, 0);
      content.setTimeout(setIdleCallback, 0);
    }
  }
  return _handler;
}
