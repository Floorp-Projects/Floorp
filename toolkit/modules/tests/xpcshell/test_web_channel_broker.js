/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/WebChannel.jsm");

const VALID_WEB_CHANNEL_ID = "id";
const URL_STRING = "http://example.com";
const VALID_WEB_CHANNEL_ORIGIN = Services.io.newURI(URL_STRING);

/**
 * Test WebChannelBroker channel map
 */
add_test(function test_web_channel_broker_channel_map() {
  let channel = {};
  let channel2 = {};

  Assert.equal(WebChannelBroker._channelMap.size, 0);
  Assert.ok(!WebChannelBroker._messageListenerAttached);

  // make sure _channelMap works correctly
  WebChannelBroker.registerChannel(channel);
  Assert.equal(WebChannelBroker._channelMap.size, 1);
  Assert.ok(WebChannelBroker._messageListenerAttached);

  WebChannelBroker.registerChannel(channel2);
  Assert.equal(WebChannelBroker._channelMap.size, 2);

  WebChannelBroker.unregisterChannel(channel);
  Assert.equal(WebChannelBroker._channelMap.size, 1);

  // make sure the correct channel is unregistered
  Assert.ok(!WebChannelBroker._channelMap.has(channel));
  Assert.ok(WebChannelBroker._channelMap.has(channel2));

  WebChannelBroker.unregisterChannel(channel2);
  Assert.equal(WebChannelBroker._channelMap.size, 0);

  run_next_test();
});


/**
 * Test WebChannelBroker _listener test
 */
add_task(function test_web_channel_broker_listener() {
  return new Promise((resolve, reject) => {
    var channel = {
      id: VALID_WEB_CHANNEL_ID,
      _originCheckCallback: requestPrincipal => {
        return VALID_WEB_CHANNEL_ORIGIN.prePath === requestPrincipal.origin;
      },
      deliver(data, sender) {
        Assert.equal(data.id, VALID_WEB_CHANNEL_ID);
        Assert.equal(data.message.command, "hello");
        Assert.notEqual(sender, undefined);
        WebChannelBroker.unregisterChannel(channel);
        resolve();
      }
    };

    WebChannelBroker.registerChannel(channel);

    var mockEvent = {
      data: {
        id: VALID_WEB_CHANNEL_ID,
        message: {
          command: "hello"
        }
      },
      principal: {
        origin: URL_STRING
      },
      objects: {
      },
    };

    WebChannelBroker._listener(mockEvent);
  });
});
