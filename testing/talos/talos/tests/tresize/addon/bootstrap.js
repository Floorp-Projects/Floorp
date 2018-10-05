"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const PREFIX = "tresize@mozilla.org";

function listener({target, data}) {
  let win = target.ownerGlobal;
  Services.scriptloader.loadSubScript("chrome://tresize/content/Profiler.js", win);
  Services.scriptloader.loadSubScript("chrome://tresize/content/tresize.js", win);

  function sendResult(result) {
    target.messageManager.sendAsyncMessage(`${PREFIX}:chrome-run-reply`, {id: data.id, result});
  }
  win.runTest(sendResult, data.locationSearch);
}

function startup(data, reason) {
  Services.mm.addMessageListener(`${PREFIX}:chrome-run-message`, listener);
  Services.mm.loadFrameScript("chrome://tresize/content/framescript.js", true);
}

function shutdown(data, reason) {}
function install(data, reason) {}
function uninstall(data, reason) {}
