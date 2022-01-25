/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from ../head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/remote/cdp/test/browser/head.js",
  this
);

// To fully test the Remote Agent's capabilities an instance of the interface
// also needs to be used.
const remoteAgentInstance = Cc["@mozilla.org/remote/agent;1"].createInstance(
  Ci.nsIRemoteAgent
);

// set up test conditions and clean up
function add_agent_task(originalTask) {
  const task = async function() {
    await RemoteAgent.close();
    await originalTask();
  };
  Object.defineProperty(task, "name", {
    value: originalTask.name,
    writable: false,
  });
  add_plain_task(task);
}

function getNonAtomicFreePort() {
  const so = Cc["@mozilla.org/network/server-socket;1"].createInstance(
    Ci.nsIServerSocket
  );
  try {
    so.init(-1, true /* aLoopbackOnly */, -1 /* aBackLog */);
    return so.port;
  } finally {
    so.close();
  }
}
