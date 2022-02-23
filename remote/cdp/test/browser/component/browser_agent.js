/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Preferences } = ChromeUtils.import(
  "resource://gre/modules/Preferences.jsm"
);

const URL = "http://localhost:0";

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

add_agent_task(async function remoteListeningNotification() {
  let active;
  const port = getNonAtomicFreePort();

  function observer(subject, topic, data) {
    Services.obs.removeObserver(observer, topic);

    active = data;
  }

  Services.obs.addObserver(observer, "remote-listening");
  await RemoteAgent.listen("http://localhost:" + port);
  is(active, "true", "remote-listening observer notified enabled state");

  Services.obs.addObserver(observer, "remote-listening");
  await RemoteAgent.close();
  is(active, null, "remote-listening observer notified disabled state");
});

add_agent_task(async function remoteDevToolsActivePortFile() {
  const profileDir = await PathUtils.getProfileDir();
  const portFile = PathUtils.join(profileDir, "DevToolsActivePort");

  const port = getNonAtomicFreePort();

  await RemoteAgent.listen("http://localhost:" + port);

  if (await IOUtils.exists(portFile)) {
    const buffer = await IOUtils.read(portFile);
    const lines = new TextDecoder().decode(buffer).split("\n");
    is(lines.length, 2, "DevToolsActivePort file contains two lines");
    is(parseInt(lines[0]), port, "DevToolsActivePort file contains port");
    ok(
      RemoteAgent.cdp.mainTargetPath.includes(lines[1]),
      "DevToolsActivePort file contains main target path"
    );
  } else {
    ok(false, "DevToolsActivePort file written");
  }

  await RemoteAgent.close();
  ok(!(await IOUtils.exists(portFile)), "DevToolsActivePort file removed");
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
  try {
    Preferences.set("remote.force-local", false);
    await RemoteAgent.listen("http://0.0.0.0:0");
    await RemoteAgent.close();
  } finally {
    Preferences.reset("remote.force-local");
  }
});

add_agent_task(async function test_close() {
  await RemoteAgent.listen(URL);
  await RemoteAgent.close();
  // no-op when not listening
  await RemoteAgent.close();
});
