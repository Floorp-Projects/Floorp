/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { FxAccountsCommands, SendTab } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsCommands.sys.mjs"
);

const { FxAccountsClient } = ChromeUtils.importESModule(
  "resource://gre/modules/FxAccountsClient.sys.mjs"
);

const { COMMAND_SENDTAB, COMMAND_SENDTAB_TAIL } = ChromeUtils.importESModule(
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

function MockFxAccountsClient() {
  FxAccountsClient.apply(this);
}

MockFxAccountsClient.prototype = {};
Object.setPrototypeOf(
  MockFxAccountsClient.prototype,
  FxAccountsClient.prototype
);

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
  // although we are sending to 3 devices, only 1 is successful - so there's
  // only 1 streamID we care about. However, we've created IDs even for the
  // failing items - so it's "4"
  const expectedTelemetryStreamID = "4";
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
      extra: { flowID: "1", streamID: expectedTelemetryStreamID },
    },
  ]);
});

add_task(async function test_sendtab_send_rate_limit() {
  const rateLimitReject = {
    code: 429,
    retryAfter: 5,
    retryAfterLocalized: "retry after 5 seconds",
  };
  const fxAccounts = {
    fxAccountsClient: new MockFxAccountsClient(),
    getUserAccountData() {
      return {};
    },
    telemetry: new TelemetryMock(),
  };
  let rejected = false;
  let invoked = 0;
  fxAccounts.fxAccountsClient.invokeCommand = async function invokeCommand() {
    invoked++;
    Assert.ok(invoked <= 2, "only called twice and not more");
    if (rejected) {
      return {};
    }
    rejected = true;
    return Promise.reject(rateLimitReject);
  };
  const commands = new FxAccountsCommands(fxAccounts);
  const sendTab = new SendTab(commands, fxAccounts);
  sendTab._encrypt = () => "encryptedpayload";

  const tab = { title: "Foo", url: "https://foo.bar/" };
  let report = await sendTab.send([{ name: "Device 1" }], tab);
  Assert.equal(report.succeeded.length, 0);
  Assert.equal(report.failed.length, 1);
  Assert.equal(report.failed[0].error, rateLimitReject);

  report = await sendTab.send([{ name: "Device 1" }], tab);
  Assert.equal(report.succeeded.length, 0);
  Assert.equal(report.failed.length, 1);
  Assert.ok(
    report.failed[0].error.message.includes(
      "Invoke for " +
        "https://identity.mozilla.com/cmd/open-uri is rate-limited"
    )
  );

  commands._invokeRateLimitExpiry = Date.now() - 1000;
  report = await sendTab.send([{ name: "Device 1" }], tab);
  Assert.equal(report.succeeded.length, 1);
  Assert.equal(report.failed.length, 0);
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
  const reason = "push";

  await sendTab.send(to, tab);
  Assert.equal(commands._invokes.length, 1);

  for (let { cmd, device, payload } of commands._invokes) {
    Assert.equal(cmd, COMMAND_SENDTAB);
    // Older Firefoxes would send a plaintext flowID in the top-level payload.
    // Test that we sensibly ignore it.
    Assert.ok(!payload.hasOwnProperty("flowID"));
    // change it - ensure we still get what we expect in telemetry later.
    payload.flowID = "ignore-me";
    Assert.deepEqual(await sendTab.handle(device.id, payload, reason), {
      title: "tab title",
      uri: "http://example.com",
    });
  }

  Assert.deepEqual(fxai.telemetry._events, [
    {
      object: "command-sent",
      method: COMMAND_SENDTAB_TAIL,
      value: "devid-san",
      extra: { flowID: "1", streamID: "2" },
    },
    {
      object: "command-received",
      method: COMMAND_SENDTAB_TAIL,
      value: "devid-san",
      extra: { flowID: "1", streamID: "2", reason },
    },
  ]);
});

// Test that a client which only sends the flowID in the envelope and not in the
// encrypted body gets recorded without the flowID.
add_task(async function test_sendtab_receive_old_client() {
  const fxai = FxaInternalMock();
  const sendTab = new SendTab(null, fxai);
  sendTab._decrypt = bytes => {
    return bytes;
  };
  const data = { entries: [{ title: "title", url: "url" }] };
  // No 'flowID' in the encrypted payload, no 'streamID' anywhere.
  const payload = {
    flowID: "flow-id",
    encrypted: new TextEncoder().encode(JSON.stringify(data)),
  };
  const reason = "push";
  await sendTab.handle("sender-id", payload, reason);
  Assert.deepEqual(fxai.telemetry._events, [
    {
      object: "command-received",
      method: COMMAND_SENDTAB_TAIL,
      value: "sender-id-san",
      // deepEqual doesn't ignore undefined, but our telemetry code and
      // JSON.stringify() do...
      extra: { flowID: undefined, streamID: undefined, reason },
    },
  ]);
});

add_task(function test_commands_getReason() {
  const fxAccounts = {
    async withCurrentAccountState(cb) {
      await cb({});
    },
  };
  const commands = new FxAccountsCommands(fxAccounts);
  const testCases = [
    {
      receivedIndex: 0,
      currentIndex: 0,
      expectedReason: "poll",
      message: "should return reason 'poll'",
    },
    {
      receivedIndex: 7,
      currentIndex: 3,
      expectedReason: "push-missed",
      message: "should return reason 'push-missed'",
    },
    {
      receivedIndex: 2,
      currentIndex: 8,
      expectedReason: "push",
      message: "should return reason 'push'",
    },
  ];
  for (const tc of testCases) {
    const reason = commands._getReason(tc.receivedIndex, tc.currentIndex);
    Assert.equal(reason, tc.expectedReason, tc.message);
  }
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
  mockCommands.expects("_fetchDeviceCommands").once().withArgs(11).returns({
    index: remoteIndex,
    messages: remoteMessages,
  });
  mockCommands
    .expects("_handleCommands")
    .once()
    .withArgs(remoteMessages, pushIndexReceived);
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

add_task(async function test_commands_handleCommands() {
  // This test ensures that `_getReason` is being called by
  // `_handleCommands` with the expected parameters.
  const pushIndexReceived = 12;
  const senderID = "6d09f6c4-89b2-41b3-a0ac-e4c2502b5485";
  const remoteMessageIndex = 8;
  const remoteMessages = [
    {
      index: remoteMessageIndex,
      data: {
        command: COMMAND_SENDTAB,
        payload: {
          encrypted: {},
        },
        sender: senderID,
      },
    },
  ];

  const fxAccounts = {
    async withCurrentAccountState(cb) {
      await cb({});
    },
  };
  const commands = new FxAccountsCommands(fxAccounts);
  commands.sendTab.handle = (sender, data, reason) => {
    return {
      title: "testTitle",
      uri: "https://testURI",
    };
  };
  commands._fxai.device = {
    refreshDeviceList: () => {},
    recentDeviceList: [
      {
        id: senderID,
      },
    ],
  };
  const mockCommands = sinon.mock(commands);
  mockCommands
    .expects("_getReason")
    .once()
    .withExactArgs(pushIndexReceived, remoteMessageIndex);
  mockCommands.expects("_notifyFxATabsReceived").once();
  await commands._handleCommands(remoteMessages, pushIndexReceived);
  mockCommands.verify();
});

add_task(async function test_commands_handleCommands_invalid_tab() {
  // This test ensures that `_getReason` is being called by
  // `_handleCommands` with the expected parameters.
  const pushIndexReceived = 12;
  const senderID = "6d09f6c4-89b2-41b3-a0ac-e4c2502b5485";
  const remoteMessageIndex = 8;
  const remoteMessages = [
    {
      index: remoteMessageIndex,
      data: {
        command: COMMAND_SENDTAB,
        payload: {
          encrypted: {},
        },
        sender: senderID,
      },
    },
  ];

  const fxAccounts = {
    async withCurrentAccountState(cb) {
      await cb({});
    },
  };
  const commands = new FxAccountsCommands(fxAccounts);
  commands.sendTab.handle = (sender, data, reason) => {
    return {
      title: "badUriTab",
      uri: "file://path/to/pdf",
    };
  };
  commands._fxai.device = {
    refreshDeviceList: () => {},
    recentDeviceList: [
      {
        id: senderID,
      },
    ],
  };
  const mockCommands = sinon.mock(commands);
  mockCommands
    .expects("_getReason")
    .once()
    .withExactArgs(pushIndexReceived, remoteMessageIndex);
  // We shouldn't have tried to open a tab with an invalid uri
  mockCommands.expects("_notifyFxATabsReceived").never();

  await commands._handleCommands(remoteMessages, pushIndexReceived);
  mockCommands.verify();
});

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
    mockCommands.expects("_fetchDeviceCommands").once().withArgs(11).returns({
      index: remoteIndex,
      messages: remoteMessages,
    });
    mockCommands
      .expects("_handleCommands")
      .once()
      .withArgs(remoteMessages, pushIndexReceived);
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
  const pushIndexReceived = 0;
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
  mockCommands.expects("_fetchDeviceCommands").once().withArgs(11).returns({
    index: remoteIndex,
    messages: remoteMessages,
  });
  mockCommands
    .expects("_handleCommands")
    .once()
    .withArgs(remoteMessages, pushIndexReceived);
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
    const pushIndexReceived = 0;
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
    mockCommands.expects("_fetchDeviceCommands").once().withArgs(0).returns({
      index: remoteIndex,
      messages: remoteMessages,
    });
    mockCommands
      .expects("_handleCommands")
      .once()
      .withArgs(remoteMessages, pushIndexReceived);
    await commands.pollDeviceCommands();

    mockCommands.verify();
    Assert.equal(accountState.data.device.lastCommandIndex, 12);
  }
);

add_task(async function test_send_tab_keys_regenerated_if_lost() {
  const commands = {
    _invokes: [],
    invoke(cmd, device, payload) {
      this._invokes.push({ cmd, device, payload });
    },
  };

  // Local state.
  const accountState = {
    data: {
      // Since the device object has no
      // sendTabKeys, it will recover
      // when we attempt to get the
      // encryptedSendTabKeys
      device: {
        lastCommandIndex: 10,
      },
      encryptedSendTabKeys: "keys",
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
    async getUserAccountData(data) {
      return accountState.getUserAccountData(data);
    },
    telemetry: new TelemetryMock(),
  };
  const sendTab = new SendTab(commands, fxAccounts);
  let generateEncryptedKeysCalled = false;
  sendTab._generateAndPersistEncryptedSendTabKey = async () => {
    generateEncryptedKeysCalled = true;
  };
  await sendTab.getEncryptedSendTabKeys();
  Assert.ok(generateEncryptedKeysCalled);
});

add_task(async function test_send_tab_keys_are_not_regenerated_if_not_lost() {
  const commands = {
    _invokes: [],
    invoke(cmd, device, payload) {
      this._invokes.push({ cmd, device, payload });
    },
  };

  // Local state.
  const accountState = {
    data: {
      // Since the device object has
      // sendTabKeys, it will not try
      // to regenerate them
      // when we attempt to get the
      // encryptedSendTabKeys
      device: {
        lastCommandIndex: 10,
        sendTabKeys: "keys",
      },
      encryptedSendTabKeys: "encrypted-keys",
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
    async getUserAccountData(data) {
      return accountState.getUserAccountData(data);
    },
    telemetry: new TelemetryMock(),
  };
  const sendTab = new SendTab(commands, fxAccounts);
  let generateEncryptedKeysCalled = false;
  sendTab._generateAndPersistEncryptedSendTabKey = async () => {
    generateEncryptedKeysCalled = true;
  };
  await sendTab.getEncryptedSendTabKeys();
  Assert.ok(!generateEncryptedKeysCalled);
});
