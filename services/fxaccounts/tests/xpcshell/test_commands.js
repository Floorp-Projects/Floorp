/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://testing-common/Assert.jsm");
ChromeUtils.import("resource://gre/modules/FxAccountsCommands.js");

add_task(async function test_sendtab_isDeviceCompatible() {
  const fxAccounts = {
    getKeys() {
      return {
        kXCS: "abcd"
      };
    }
  };
  const sendTab = new SendTab(null, fxAccounts);
  let device = {name: "My device"};
  Assert.ok(!(await sendTab.isDeviceCompatible(device)));
  device = {name: "My device", availableCommands: {}};
  Assert.ok(!(await sendTab.isDeviceCompatible(device)));
  device = {name: "My device", availableCommands: {
    "https://identity.mozilla.com/cmd/open-uri": JSON.stringify({
      kid: "dcba"
    })
  }};
  Assert.ok(!(await sendTab.isDeviceCompatible(device)));
  device = {name: "My device", availableCommands: {
    "https://identity.mozilla.com/cmd/open-uri": JSON.stringify({
      kid: "abcd"
    })
  }};
  Assert.ok((await sendTab.isDeviceCompatible(device)));
});

add_task(async function test_sendtab_send() {
  const commands = {
    invoke: sinon.spy((cmd, device, payload) => {
      if (device.name == "Device 1") {
        throw new Error("Invoke error!");
      }
      Assert.equal(payload.encrypted, "encryptedpayload");
    })
  };
  const sendTab = new SendTab(commands, null);
  sendTab._encrypt = (bytes, device) => {
    if (device.name == "Device 2") {
      throw new Error("Encrypt error!");
    }
    return "encryptedpayload";
  };
  const to = [
    {name: "Device 1"},
    {name: "Device 2"},
    {name: "Device 3"},
  ];
  const tab = {title: "Foo", url: "https://foo.bar/"};
  const report = await sendTab.send(to, tab);
  Assert.equal(report.succeeded.length, 1);
  Assert.equal(report.failed.length, 2);
  Assert.equal(report.succeeded[0].name, "Device 3");
  Assert.equal(report.failed[0].device.name, "Device 1");
  Assert.equal(report.failed[0].error.message, "Invoke error!");
  Assert.equal(report.failed[1].device.name, "Device 2");
  Assert.equal(report.failed[1].error.message, "Encrypt error!");
  Assert.ok(commands.invoke.calledTwice);
});

add_task(async function test_commands_fetchMissedRemoteCommands() {
  const accountState = {
    data: {
      device: {
        handledCommands: [8, 9, 10, 11],
        lastCommandIndex: 11,
      }
    }
  };
  const fxAccounts = {
    async _withCurrentAccountState(cb) {
      const get = () => accountState.data;
      const set = (val) => { accountState.data = val; };
      await cb(get, set);
    }
  };
  const commands = new FxAccountsCommands(fxAccounts);
  commands._fetchRemoteCommands = () => {
    return {
      index: 12,
      messages: [
        {
          index: 11,
          data: {}
        },
        {
          index: 12,
          data: {}
        }
      ]
    };
  };
  commands._handleCommands = sinon.spy();
  await commands.fetchMissedRemoteCommands();

  Assert.equal(accountState.data.device.handledCommands.length, 0);
  Assert.equal(accountState.data.device.lastCommandIndex, 12);
  const callArgs = commands._handleCommands.args[0][0];
  Assert.equal(callArgs[0].index, 12);
});
