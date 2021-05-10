/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

// To fully test the Remote Agent's capabilities an instance of the interface
// needs to be used. This refers to the Rust specific implementation.
const remoteAgentInstance = Cc["@mozilla.org/remote/agent;1"].createInstance(
  Ci.nsIRemoteAgent
);

const URL = "http://localhost:0";

// set up test conditions and clean up
function add_agent_task(originalTask) {
  const task = async function() {
    try {
      await RemoteAgent.close();
      await originalTask();
    } finally {
      Preferences.reset("remote.enabled");
      Preferences.reset("remote.force-local");
    }
  };
  Object.defineProperty(task, "name", {
    value: originalTask.name,
    writable: false,
  });
  add_plain_task(task);
}

add_agent_task(async function debuggerAddress() {
  const port = getNonAtomicFreePort();

  await RemoteAgent.listen(`http://localhost:${port}`);
  is(
    remoteAgentInstance.debuggerAddress,
    `localhost:${port}`,
    "debuggerAddress set"
  );
});

add_agent_task(async function listening() {
  is(remoteAgentInstance.listening, false, "Agent is not listening");
  await RemoteAgent.listen(URL);
  is(remoteAgentInstance.listening, true, "Agent is listening");
});

add_agent_task(async function listen() {
  const port = getNonAtomicFreePort();

  let boundURL;
  function observer(subject, topic, data) {
    const prefix = "DevTools listening on ";
    if (!data.startsWith(prefix)) {
      return;
    }

    Services.obs.removeObserver(observer, topic);
    boundURL = Services.io.newURI(data.split(prefix)[1]);
  }
  Services.obs.addObserver(observer, "remote-listening");

  await RemoteAgent.listen("http://localhost:" + port);
  isnot(boundURL, undefined, "remote-listening observer notified");
  is(
    boundURL.port,
    port,
    `expected default port ${port}, got ${boundURL.port}`
  );
});

add_agent_task(async function listenWhenDisabled() {
  Preferences.set("remote.enabled", false);
  try {
    await RemoteAgent.listen(URL);
    fail("listen() did not return exception");
  } catch (e) {
    is(e.result, Cr.NS_ERROR_NOT_AVAILABLE);
    is(e.message, "Disabled by preference");
  }
});

// TODO(ato): https://bugzil.la/1590829
add_agent_task(async function listenTakesString() {
  await RemoteAgent.listen("http://localhost:0");
  await RemoteAgent.close();
});

// TODO(ato): https://bugzil.la/1590829
add_agent_task(async function listenNonURL() {
  try {
    await RemoteAgent.listen("foobar");
    fail("listen() did not reject non-URL");
  } catch (e) {
    is(e.result, Cr.NS_ERROR_MALFORMED_URI);
  }
});

add_agent_task(async function listenRestrictedToLoopbackDevice() {
  try {
    await RemoteAgent.listen("http://0.0.0.0:0");
    fail("listen() did not reject non-loopback device");
  } catch (e) {
    is(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
    is(e.message, "Restricted to loopback devices");
  }
});

add_agent_task(async function listenNonLoopbackDevice() {
  Preferences.set("remote.force-local", false);
  await RemoteAgent.listen("http://0.0.0.0:0");
});

add_agent_task(async function test_close() {
  await RemoteAgent.listen(URL);
  await RemoteAgent.close();
  // no-op when not listening
  await RemoteAgent.close();
});

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
