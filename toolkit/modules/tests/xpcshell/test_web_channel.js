/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { WebChannel } = ChromeUtils.import(
  "resource://gre/modules/WebChannel.jsm"
);
const { PermissionTestUtils } = ChromeUtils.import(
  "resource://testing-common/PermissionTestUtils.jsm"
);

const ERROR_ID_ORIGIN_REQUIRED =
  "WebChannel id and originOrPermission are required.";
const VALID_WEB_CHANNEL_ID = "id";
const URL_STRING = "http://example.com";
const VALID_WEB_CHANNEL_ORIGIN = Services.io.newURI(URL_STRING);
const TEST_PERMISSION_NAME = "test-webchannel-permissions";

var MockWebChannelBroker = {
  _channelMap: new Map(),
  registerChannel(channel) {
    if (!this._channelMap.has(channel)) {
      this._channelMap.set(channel);
    }
  },
  unregisterChannel(channelToRemove) {
    this._channelMap.delete(channelToRemove);
  },
};

/**
 * Web channel tests
 */

/**
 * Test channel listening with originOrPermission being an nsIURI.
 */
add_task(function test_web_channel_listen() {
  return new Promise((resolve, reject) => {
    let channel = new WebChannel(
      VALID_WEB_CHANNEL_ID,
      VALID_WEB_CHANNEL_ORIGIN,
      {
        broker: MockWebChannelBroker,
      }
    );
    let delivered = 0;
    Assert.equal(channel.id, VALID_WEB_CHANNEL_ID);
    Assert.equal(
      channel._originOrPermission.spec,
      VALID_WEB_CHANNEL_ORIGIN.spec
    );
    Assert.equal(channel._deliverCallback, null);

    channel.listen(function(id, message, target) {
      Assert.equal(id, VALID_WEB_CHANNEL_ID);
      Assert.ok(message);
      Assert.ok(message.command);
      Assert.ok(target.sender);
      delivered++;
      // 2 messages should be delivered
      if (delivered === 2) {
        channel.stopListening();
        Assert.equal(channel._deliverCallback, null);
        resolve();
      }
    });

    // send two messages
    channel.deliver(
      {
        id: VALID_WEB_CHANNEL_ID,
        message: {
          command: "one",
        },
      },
      { sender: true }
    );

    channel.deliver(
      {
        id: VALID_WEB_CHANNEL_ID,
        message: {
          command: "two",
        },
      },
      { sender: true }
    );
  });
});

/**
 * Test channel listening with originOrPermission being a permission string.
 */
add_task(function test_web_channel_listen_permission() {
  return new Promise((resolve, reject) => {
    // add a new permission
    PermissionTestUtils.add(
      VALID_WEB_CHANNEL_ORIGIN,
      TEST_PERMISSION_NAME,
      Services.perms.ALLOW_ACTION
    );
    registerCleanupFunction(() =>
      PermissionTestUtils.remove(VALID_WEB_CHANNEL_ORIGIN, TEST_PERMISSION_NAME)
    );
    let channel = new WebChannel(VALID_WEB_CHANNEL_ID, TEST_PERMISSION_NAME, {
      broker: MockWebChannelBroker,
    });
    let delivered = 0;
    Assert.equal(channel.id, VALID_WEB_CHANNEL_ID);
    Assert.equal(channel._originOrPermission, TEST_PERMISSION_NAME);
    Assert.equal(channel._deliverCallback, null);

    channel.listen(function(id, message, target) {
      Assert.equal(id, VALID_WEB_CHANNEL_ID);
      Assert.ok(message);
      Assert.ok(message.command);
      Assert.ok(target.sender);
      delivered++;
      // 2 messages should be delivered
      if (delivered === 2) {
        channel.stopListening();
        Assert.equal(channel._deliverCallback, null);
        resolve();
      }
    });

    // send two messages
    channel.deliver(
      {
        id: VALID_WEB_CHANNEL_ID,
        message: {
          command: "one",
        },
      },
      { sender: true }
    );

    channel.deliver(
      {
        id: VALID_WEB_CHANNEL_ID,
        message: {
          command: "two",
        },
      },
      { sender: true }
    );
  });
});

/**
 * Test constructor
 */
add_test(function test_web_channel_constructor() {
  Assert.equal(constructorTester(), ERROR_ID_ORIGIN_REQUIRED);
  Assert.equal(constructorTester(undefined), ERROR_ID_ORIGIN_REQUIRED);
  Assert.equal(
    constructorTester(undefined, VALID_WEB_CHANNEL_ORIGIN),
    ERROR_ID_ORIGIN_REQUIRED
  );
  Assert.equal(
    constructorTester(VALID_WEB_CHANNEL_ID, undefined),
    ERROR_ID_ORIGIN_REQUIRED
  );
  Assert.ok(!constructorTester(VALID_WEB_CHANNEL_ID, VALID_WEB_CHANNEL_ORIGIN));

  run_next_test();
});

function constructorTester(id, origin) {
  try {
    new WebChannel(id, origin);
  } catch (e) {
    return e.message;
  }
  return false;
}
