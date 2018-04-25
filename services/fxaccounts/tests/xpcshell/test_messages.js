/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://testing-common/Assert.jsm");
ChromeUtils.import("resource://gre/modules/FxAccountsMessages.js");

add_task(async function test_sendTab() {
  const fxAccounts = {
    async getDeviceId() {
      return "my-device-id";
    }
  };
  const sender = {
    send: sinon.spy()
  };
  const fxAccountsMessages = new FxAccountsMessages(fxAccounts, {sender});

  const to = [{
    id: "deviceid-1",
    pushPublicKey: "pubkey-1",
    pushAuthKey: "authkey-1"
  }];
  const tab = {url: "https://foo.com", title: "Foo"};
  await fxAccountsMessages.sendTab(to, tab);
  Assert.ok(sender.send.calledOnce);
  Assert.equal(sender.send.args[0][0], "sendtab");
  Assert.deepEqual(sender.send.args[0][1], to);
  Assert.deepEqual(sender.send.args[0][2], {
    topic: "sendtab",
    data: {
      from: "my-device-id",
      entries: [{title: "Foo", url: "https://foo.com"}]
    }
  });
});

add_task(async function test_consumeRemoteMessages() {
  const fxAccounts = {};
  const receiver = {
    consumeRemoteMessages: sinon.spy()
  };
  const fxAccountsMessages = new FxAccountsMessages(fxAccounts, {receiver});
  fxAccountsMessages.consumeRemoteMessages();
  Assert.ok(receiver.consumeRemoteMessages.calledOnce);
});

add_task(async function test_canReceiveSendTabMessages() {
  const fxAccounts = {};
  const messages = new FxAccountsMessages(fxAccounts);
  Assert.ok(!messages.canReceiveSendTabMessages({id: "device-id-1"}));
  Assert.ok(!messages.canReceiveSendTabMessages({id: "device-id-1", capabilities: []}));
  Assert.ok(!messages.canReceiveSendTabMessages({id: "device-id-1", capabilities: ["messages"]}));
  Assert.ok(messages.canReceiveSendTabMessages({id: "device-id-1", capabilities: ["messages", "messages.sendtab"]}));
});

add_task(async function test_sender_send() {
  const sandbox = sinon.sandbox.create();
  const fxaClient = {
    sendMessage: sinon.spy()
  };
  const sessionToken = "toktok";
  const fxAccounts = {
    async getSignedInUser() {
      return {sessionToken};
    },
    getAccountsClient() {
      return fxaClient;
    }
  };
  const sender = new FxAccountsMessagesSender(fxAccounts);
  sandbox.stub(sender, "_encrypt").callsFake((_, device) => {
    if (device.pushPublicKey == "pubkey-1") {
      return "encrypted-text-1";
    }
    return "encrypted-text-2";
  });

  const topic = "mytopic";
  const to = [{
    id: "deviceid-1",
    pushPublicKey: "pubkey-1",
    pushAuthKey: "authkey-1"
  }, {
    id: "deviceid-2",
    pushPublicKey: "pubkey-2",
    pushAuthKey: "authkey-2"
  }];
  const payload = {foo: "bar"};

  await sender.send(topic, to, payload);

  Assert.ok(fxaClient.sendMessage.calledTwice);
  const checkCallArgs = (callNum, deviceId, encrypted) => {
    Assert.equal(fxaClient.sendMessage.args[callNum][0], sessionToken);
    Assert.equal(fxaClient.sendMessage.args[callNum][1], topic);
    Assert.equal(fxaClient.sendMessage.args[callNum][2], deviceId);
    Assert.equal(fxaClient.sendMessage.args[callNum][3], encrypted);
  };
  checkCallArgs(0, "deviceid-1", "encrypted-text-1");
  checkCallArgs(1, "deviceid-2", "encrypted-text-2");
  sandbox.restore();
});

add_task(async function test_receiver_consumeRemoteMessages() {
  const fxaClient = {
    getMessages: sinon.spy(async () => {
      return {
        index: "idx-2",
        messages: [{
          index: "idx-1",
          data: "#giberish#"
        }, {
          index: "idx-2",
          data: "#encrypted#"
        }]
      };
    })
  };
  const fxAccounts = {
    accountState: {sessionToken: "toktok", device: {}},
    _withCurrentAccountState(fun) {
      const get = () => this.accountState;
      const update = (obj) => { this.accountState = {...this.accountState, ...obj}; };
      return fun(get, update);
    },
    getAccountsClient() {
      return fxaClient;
    }
  };
  const sandbox = sinon.sandbox.create();
  const messagesHandler = {
    handle: sinon.spy()
  };
  const receiver = new FxAccountsMessagesReceiver(fxAccounts, {
    handler: messagesHandler
  });
  sandbox.stub(receiver, "_getOwnKeys").callsFake(async () => {});
  sandbox.stub(receiver, "_decrypt").callsFake((ciphertext) => {
    if (ciphertext == "#encrypted#") {
      return new TextEncoder("utf-8").encode(JSON.stringify({"foo": "bar"}));
    }
    throw new Error("Boom!");
  });

  await receiver.consumeRemoteMessages();

  Assert.ok(fxaClient.getMessages.calledOnce);
  Assert.equal(fxaClient.getMessages.args[0][0], "toktok");
  Assert.deepEqual(fxaClient.getMessages.args[0][1], {});
  Assert.ok(messagesHandler.handle.calledOnce);
  Assert.deepEqual(messagesHandler.handle.args[0][0], [{"foo": "bar"}]);
  fxaClient.getMessages.reset();

  await receiver.consumeRemoteMessages();

  Assert.ok(fxaClient.getMessages.calledOnce);
  Assert.equal(fxaClient.getMessages.args[0][0], "toktok");
  Assert.deepEqual(fxaClient.getMessages.args[0][1], {"index": "idx-2"});

  sandbox.restore();
});

add_task(async function test_handler_handle_sendtab() {
  const fxAccounts = {
    async getDeviceList() {
      return [{id: "1234a", name: "My Computer"}];
    }
  };
  const handler = new FxAccountsMessagesHandler(fxAccounts);
  const payloads = [{
    topic: "sendtab",
    data: {
      from: "1234a",
      current: 0,
      entries: [{title: "Foo", url: "https://foo.com"},
                {title: "Bar", url: "https://bar.com"}]
    }
  }, {
    topic: "sendtab",
    data: {
      from: "unknown_device",
      entries: [{title: "Foo2", url: "https://foo2.com"},
                {title: "Bar2", url: "https://bar2.com"}]
    }
  }, {
    topic: "unknowntopic",
    data: {foo: "bar"}
  }];
  const notificationPromise = promiseObserver("fxaccounts:messages:display-tabs");
  await handler.handle(payloads);
  const {subject} = await notificationPromise;
  const toDisplay = subject.wrappedJSObject.object;
  const expected = [
    {uri: "https://foo.com", title: "Foo", sender: {id: "1234a", name: "My Computer"}},
    {uri: "https://bar2.com", title: "Bar2", sender: {id: "", name: ""}}
  ];
  Assert.deepEqual(toDisplay, expected);
});

function promiseObserver(aTopic) {
  return new Promise(resolve => {
    Services.obs.addObserver(function onNotification(subject, topic, data) {
      Services.obs.removeObserver(onNotification, topic);
        resolve({subject, data});
      }, aTopic);
  });
}

