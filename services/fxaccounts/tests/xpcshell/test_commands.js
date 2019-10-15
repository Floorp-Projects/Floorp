/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccountsCommands, SendTab } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommands.js"
);

const { COMMAND_SENDTAB, COMMAND_SENDTAB_TAIL } = ChromeUtils.import(
  "resource://gre/modules/FxAccountsCommon.js"
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

add_task(async function test_sendtab_isDeviceCompatible() {
  const sendTab = new SendTab(null, null);
  let device = { name: "My device" };
  Assert.ok(!sendTab.isDeviceCompatible(device));
  device = { name: "My device", availableCommands: {} };
  Assert.ok(!sendTab.isDeviceCompatible(device));
  device = {
    name: "My device",
    availableCommands: {
      "https://identity.mozilla.com/cmd/open-uri": "payload",
    },
  };
  Assert.ok(sendTab.isDeviceCompatible(device));
});

add_task(async function test_sendtab_send() {
  const commands = {
    invoke: sinon.spy((cmd, device, payload) => {
      if (device.name == "Device 1") {
        throw new Error("Invoke error!");
      }
      Assert.equal(payload.encrypted, "encryptedpayload");
    }),
  };
  const fxai = FxaInternalMock();
  const sendTab = new SendTab(commands, fxai);
  sendTab._encrypt = (bytes, device) => {
    if (device.name == "Device 2") {
      throw new Error("Encrypt error!");
    }
    return "encryptedpayload";
  };
  const to = [
    { name: "Device 1" },
    { name: "Device 2" },
    { id: "dev3", name: "Device 3" },
  ];
  const tab = { title: "Foo", url: "https://foo.bar/" };
  const report = await sendTab.send(to, tab);
  Assert.equal(report.succeeded.length, 1);
  Assert.equal(report.failed.length, 2);
  Assert.equal(report.succeeded[0].name, "Device 3");
  Assert.equal(report.failed[0].device.name, "Device 1");
  Assert.equal(report.failed[0].error.message, "Invoke error!");
  Assert.equal(report.failed[1].device.name, "Device 2");
  Assert.equal(report.failed[1].error.message, "Encrypt error!");
  Assert.ok(commands.invoke.calledTwice);
  Assert.deepEqual(fxai.telemetry._events, [
    {
      object: "command-sent",
      method: COMMAND_SENDTAB_TAIL,
      value: "dev3-san",
      extra: { flowID: "1" },
    },
  ]);
});

add_task(async function test_sendtab_receive() {
  // We are testing 'receive' here, but might as well go through 'send'
  // to package the data and for additional testing...
  const commands = {
    _invokes: [],
    invoke(cmd, device, payload) {
      this._invokes.push({ cmd, device, payload });
    },
  };

  const fxai = FxaInternalMock();
  const sendTab = new SendTab(commands, fxai);
  sendTab._encrypt = (bytes, device) => {
    return bytes;
  };
  sendTab._decrypt = bytes => {
    return bytes;
  };
  const tab = { title: "tab title", url: "http://example.com" };
  const to = [{ id: "devid", name: "The Device" }];

  await sendTab.send(to, tab);
  Assert.equal(commands._invokes.length, 1);

  for (let { cmd, device, payload } of commands._invokes) {
    Assert.equal(cmd, COMMAND_SENDTAB);
    Assert.deepEqual(await sendTab.handle(device.id, payload), {
      title: "tab title",
      uri: "http://example.com",
    });
  }

  Assert.deepEqual(fxai.telemetry._events, [
    {
      object: "command-sent",
      method: COMMAND_SENDTAB_TAIL,
      value: "devid-san",
      extra: { flowID: "1" },
    },
    {
      object: "command-received",
      method: COMMAND_SENDTAB_TAIL,
      value: "devid-san",
      extra: { flowID: "1" },
    },
  ]);
});

add_task(async function test_commands_pollDeviceCommands_push() {
  // Server state.
  const remoteMessages = [
    {
      index: 11,
      data: {},
    },
    {
      index: 12,
      data: {},
    },
  ];
  const remoteIndex = 12;

  // Local state.
  const pushIndexReceived = 11;
  const accountState = {
    data: {
      device: {
        lastCommandIndex: 10,
      },
    },
    getUserAccountData() {
      return this.data;
    },
    updateUserAccountData(data) {
      this.data = data;
    },
  };

  const fxAccounts = {
    async withCurrentAccountState(cb) {
      await cb(accountState);
    },
  };
  const commands = new FxAccountsCommands(fxAccounts);
  const mockCommands = sinon.mock(commands);
  mockCommands
    .expects("_fetchDeviceCommands")
    .once()
    .withArgs(11)
    .returns({
      index: remoteIndex,
      messages: remoteMessages,
    });
  mockCommands
    .expects("_handleCommands")
    .once()
    .withArgs(remoteMessages);
  await commands.pollDeviceCommands(pushIndexReceived);

  mockCommands.verify();
  Assert.equal(accountState.data.device.lastCommandIndex, 12);
});

add_task(
  async function test_commands_pollDeviceCommands_push_already_fetched() {
    // Local state.
    const pushIndexReceived = 12;
    const accountState = {
      data: {
        device: {
          lastCommandIndex: 12,
        },
      },
      getUserAccountData() {
        return this.data;
      },
      updateUserAccountData(data) {
        this.data = data;
      },
    };

    const fxAccounts = {
      async withCurrentAccountState(cb) {
        await cb(accountState);
      },
    };
    const commands = new FxAccountsCommands(fxAccounts);
    const mockCommands = sinon.mock(commands);
    mockCommands.expects("_fetchDeviceCommands").never();
    mockCommands.expects("_handleCommands").never();
    await commands.pollDeviceCommands(pushIndexReceived);

    mockCommands.verify();
    Assert.equal(accountState.data.device.lastCommandIndex, 12);
  }
);

add_task(
  async function test_commands_pollDeviceCommands_push_local_state_empty() {
    // Server state.
    const remoteMessages = [
      {
        index: 11,
        data: {},
      },
      {
        index: 12,
        data: {},
      },
    ];
    const remoteIndex = 12;

    // Local state.
    const pushIndexReceived = 11;
    const accountState = {
      data: {
        device: {},
      },
      getUserAccountData() {
        return this.data;
      },
      updateUserAccountData(data) {
        this.data = data;
      },
    };

    const fxAccounts = {
      async withCurrentAccountState(cb) {
        await cb(accountState);
      },
    };
    const commands = new FxAccountsCommands(fxAccounts);
    const mockCommands = sinon.mock(commands);
    mockCommands
      .expects("_fetchDeviceCommands")
      .once()
      .withArgs(11)
      .returns({
        index: remoteIndex,
        messages: remoteMessages,
      });
    mockCommands
      .expects("_handleCommands")
      .once()
      .withArgs(remoteMessages);
    await commands.pollDeviceCommands(pushIndexReceived);

    mockCommands.verify();
    Assert.equal(accountState.data.device.lastCommandIndex, 12);
  }
);

add_task(async function test_commands_pollDeviceCommands_scheduled_local() {
  // Server state.
  const remoteMessages = [
    {
      index: 11,
      data: {},
    },
    {
      index: 12,
      data: {},
    },
  ];
  const remoteIndex = 12;

  // Local state.
  const accountState = {
    data: {
      device: {
        lastCommandIndex: 10,
      },
    },
    getUserAccountData() {
      return this.data;
    },
    updateUserAccountData(data) {
      this.data = data;
    },
  };

  const fxAccounts = {
    async withCurrentAccountState(cb) {
      await cb(accountState);
    },
  };
  const commands = new FxAccountsCommands(fxAccounts);
  const mockCommands = sinon.mock(commands);
  mockCommands
    .expects("_fetchDeviceCommands")
    .once()
    .withArgs(11)
    .returns({
      index: remoteIndex,
      messages: remoteMessages,
    });
  mockCommands
    .expects("_handleCommands")
    .once()
    .withArgs(remoteMessages);
  await commands.pollDeviceCommands();

  mockCommands.verify();
  Assert.equal(accountState.data.device.lastCommandIndex, 12);
});

add_task(
  async function test_commands_pollDeviceCommands_scheduled_local_state_empty() {
    // Server state.
    const remoteMessages = [
      {
        index: 11,
        data: {},
      },
      {
        index: 12,
        data: {},
      },
    ];
    const remoteIndex = 12;

    // Local state.
    const accountState = {
      data: {
        device: {},
      },
      getUserAccountData() {
        return this.data;
      },
      updateUserAccountData(data) {
        this.data = data;
      },
    };

    const fxAccounts = {
      async withCurrentAccountState(cb) {
        await cb(accountState);
      },
    };
    const commands = new FxAccountsCommands(fxAccounts);
    const mockCommands = sinon.mock(commands);
    mockCommands
      .expects("_fetchDeviceCommands")
      .once()
      .withArgs(0)
      .returns({
        index: remoteIndex,
        messages: remoteMessages,
      });
    mockCommands
      .expects("_handleCommands")
      .once()
      .withArgs(remoteMessages);
    await commands.pollDeviceCommands();

    mockCommands.verify();
    Assert.equal(accountState.data.device.lastCommandIndex, 12);
  }
);
