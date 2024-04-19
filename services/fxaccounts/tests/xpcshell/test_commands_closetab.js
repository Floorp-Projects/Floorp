/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CloseRemoteTab } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsCommands.sys.mjs"
);

const { COMMAND_CLOSETAB, COMMAND_CLOSETAB_TAIL } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsCommon.sys.mjs"
);

class TelemetryMock {
  constructor() {
    this._events = [];
    this._uuid_counter = 0;
  }

  recordEvent(object, method, value, extra = undefined) {
    this._events.push({ object, method, value, extra });
  }

  generateFlowID() {
    this._uuid_counter += 1;
    return this._uuid_counter.toString();
  }

  sanitizeDeviceId(id) {
    return id + "-san";
  }
}

function FxaInternalMock() {
  return {
    telemetry: new TelemetryMock(),
  };
}

function promiseObserver(topic) {
  return new Promise(resolve => {
    let obs = (aSubject, aTopic) => {
      Services.obs.removeObserver(obs, aTopic);
      resolve(aSubject);
    };
    Services.obs.addObserver(obs, topic);
  });
}

add_task(async function test_closetab_isDeviceCompatible() {
  const closeTab = new CloseRemoteTab(null, null);
  let device = { name: "My device" };
  Assert.ok(!closeTab.isDeviceCompatible(device));
  device = { name: "My device", availableCommands: {} };
  Assert.ok(!closeTab.isDeviceCompatible(device));
  device = {
    name: "My device",
    availableCommands: {
      "https://identity.mozilla.com/cmd/close-uri/v1": "payload",
    },
  };
  // Even though the command is available, we're keeping this feature behind a feature
  // flag for now, so it should still show up as "not available"
  Assert.ok(!closeTab.isDeviceCompatible(device));

  // Enable the feature
  Services.prefs.setBoolPref(
    "identity.fxaccounts.commands.remoteTabManagement.enabled",
    true
  );
  Assert.ok(closeTab.isDeviceCompatible(device));

  // clear it for the next test
  Services.prefs.clearUserPref(
    "identity.fxaccounts.commands.remoteTabManagement.enabled"
  );
});

add_task(async function test_closetab_send() {
  const commands = {
    invoke: sinon.spy((cmd, device, payload) => {
      Assert.equal(payload.encrypted, "encryptedpayload");
    }),
  };
  const fxai = FxaInternalMock();
  const closeTab = new CloseRemoteTab(commands, fxai);
  closeTab._encrypt = async () => {
    return "encryptedpayload";
  };
  const targetDevice = { id: "dev1", name: "Device 1" };
  const tab = { url: "https://foo.bar/" };

  // We add a 0 delay so we can "send" the push immediately
  closeTab.enqueueTabToClose(targetDevice, tab, 0);

  // We have a tab queued
  Assert.equal(closeTab.pendingClosedTabs.get(targetDevice.id).tabs.length, 1);

  // Wait on the notification to ensure the push sent
  await promiseObserver("test:fxaccounts:commands:close-uri:sent");

  // The push has been sent, we should not have the tabs anymore
  Assert.equal(
    closeTab.pendingClosedTabs.has(targetDevice.id),
    false,
    "The device should be removed from the queue after sending."
  );

  // Telemetry shows we sent one successfully
  Assert.deepEqual(fxai.telemetry._events, [
    {
      object: "command-sent",
      method: COMMAND_CLOSETAB_TAIL,
      value: "dev1-san",
      // streamID uses the same generator as flowId, so it will be 2
      extra: { flowID: "1", streamID: "2" },
    },
  ]);
});

add_task(async function test_multiple_tabs_one_device() {
  const commands = sinon.stub({
    invoke: async () => {},
  });
  const fxai = FxaInternalMock();
  const closeTab = new CloseRemoteTab(commands, fxai);
  closeTab._encrypt = async () => "encryptedpayload";

  const targetDevice = {
    id: "dev1",
    name: "Device 1",
    availableCommands: { [COMMAND_CLOSETAB]: "payload" },
  };
  const tab1 = { url: "https://foo.bar/" };
  const tab2 = { url: "https://example.com/" };

  closeTab.enqueueTabToClose(targetDevice, tab1, 1000);
  closeTab.enqueueTabToClose(targetDevice, tab2, 0);

  // We have two tabs queued
  Assert.equal(closeTab.pendingClosedTabs.get("dev1").tabs.length, 2);

  // Wait on the notification to ensure the push sent
  await promiseObserver("test:fxaccounts:commands:close-uri:sent");

  Assert.equal(
    closeTab.pendingClosedTabs.has(targetDevice.id),
    false,
    "The device should be removed from the queue after sending."
  );

  // Telemetry shows we sent one successfully
  Assert.deepEqual(fxai.telemetry._events, [
    {
      object: "command-sent",
      method: COMMAND_CLOSETAB_TAIL,
      value: "dev1-san",
      extra: { flowID: "1", streamID: "2" },
    },
  ]);
});

add_task(async function test_timer_reset_on_new_tab() {
  const commands = sinon.stub({
    invoke: async () => {},
  });
  const fxai = FxaInternalMock();
  const closeTab = new CloseRemoteTab(commands, fxai);
  closeTab._encrypt = async () => "encryptedpayload";

  const targetDevice = {
    id: "dev1",
    name: "Device 1",
    availableCommands: { [COMMAND_CLOSETAB]: "payload" },
  };
  const tab1 = { url: "https://foo.bar/" };
  const tab2 = { url: "https://example.com/" };

  // default wait is 6s
  closeTab.enqueueTabToClose(targetDevice, tab1);

  Assert.equal(closeTab.pendingClosedTabs.get(targetDevice.id).tabs.length, 1);

  // Adds a new tab and should reset timer
  closeTab.enqueueTabToClose(targetDevice, tab2, 100);

  // We have two tabs queued
  Assert.equal(closeTab.pendingClosedTabs.get(targetDevice.id).tabs.length, 2);

  // Wait on the notification to ensure the push sent
  await promiseObserver("test:fxaccounts:commands:close-uri:sent");

  // We only sent one push
  sinon.assert.calledOnce(commands.invoke);
  Assert.equal(closeTab.pendingClosedTabs.has(targetDevice.id), false);

  // Telemetry shows we sent only one
  Assert.deepEqual(fxai.telemetry._events, [
    {
      object: "command-sent",
      method: COMMAND_CLOSETAB_TAIL,
      value: "dev1-san",
      extra: { flowID: "1", streamID: "2" },
    },
  ]);
});

add_task(async function test_multiple_devices() {
  const commands = sinon.stub({
    invoke: async () => {},
  });
  const fxai = FxaInternalMock();
  const closeTab = new CloseRemoteTab(commands, fxai);
  closeTab._encrypt = async () => "encryptedpayload";

  const device1 = {
    id: "dev1",
    name: "Device 1",
    availableCommands: { [COMMAND_CLOSETAB]: "payload" },
  };
  const device2 = {
    id: "dev2",
    name: "Device 2",
    availableCommands: { [COMMAND_CLOSETAB]: "payload" },
  };
  const tab1 = { url: "https://foo.bar/" };
  const tab2 = { url: "https://example.com/" };

  closeTab.enqueueTabToClose(device1, tab1, 100);
  closeTab.enqueueTabToClose(device2, tab2, 200);

  Assert.equal(closeTab.pendingClosedTabs.get(device1.id).tabs.length, 1);
  Assert.equal(closeTab.pendingClosedTabs.get(device2.id).tabs.length, 1);

  // observe the notification to ensure the push sent
  await promiseObserver("test:fxaccounts:commands:close-uri:sent");

  // We should have only sent the first device
  sinon.assert.calledOnce(commands.invoke);
  Assert.equal(closeTab.pendingClosedTabs.has(device1.id), false);

  // Wait on the notification to ensure the push sent
  await promiseObserver("test:fxaccounts:commands:close-uri:sent");

  // Now we've sent both pushes
  sinon.assert.calledTwice(commands.invoke);

  // Two telemetry events to two different devices
  Assert.deepEqual(fxai.telemetry._events, [
    {
      object: "command-sent",
      method: COMMAND_CLOSETAB_TAIL,
      value: "dev1-san",
      extra: { flowID: "1", streamID: "2" },
    },
    {
      object: "command-sent",
      method: COMMAND_CLOSETAB_TAIL,
      value: "dev2-san",
      extra: { flowID: "3", streamID: "4" },
    },
  ]);
});
