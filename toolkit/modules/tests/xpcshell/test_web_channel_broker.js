/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/WebChannel.jsm");

const VALID_WEB_CHANNEL_ID = "id";
const URL_STRING = "http://example.com";
const VALID_WEB_CHANNEL_ORIGIN = Services.io.newURI(URL_STRING, null, null);

function run_test() {
  run_next_test();
}

/**
 * Test WebChannelBroker channel map
 */
add_test(function test_web_channel_broker_channel_map() {
  let channel = new Object();
  let channel2 = new Object();

  do_check_eq(WebChannelBroker._channelMap.size, 0);
  do_check_false(WebChannelBroker._messageListenerAttached);

  // make sure _channelMap works correctly
  WebChannelBroker.registerChannel(channel);
  do_check_eq(WebChannelBroker._channelMap.size, 1);
  do_check_true(WebChannelBroker._messageListenerAttached);

  WebChannelBroker.registerChannel(channel2);
  do_check_eq(WebChannelBroker._channelMap.size, 2);

  WebChannelBroker.unregisterChannel(channel);
  do_check_eq(WebChannelBroker._channelMap.size, 1);

  // make sure the correct channel is unregistered
  do_check_false(WebChannelBroker._channelMap.has(channel));
  do_check_true(WebChannelBroker._channelMap.has(channel2));

  WebChannelBroker.unregisterChannel(channel2);
  do_check_eq(WebChannelBroker._channelMap.size, 0);

  run_next_test();
});


/**
 * Test WebChannelBroker _listener test
 */
add_task(function test_web_channel_broker_listener() {
  return new Promise((resolve, reject) => {
    var channel = new Object({
      id: VALID_WEB_CHANNEL_ID,
      origin: VALID_WEB_CHANNEL_ORIGIN,
      deliver: function(data, sender) {
        do_check_eq(data.id, VALID_WEB_CHANNEL_ID);
        do_check_eq(data.message.command, "hello");
        WebChannelBroker.unregisterChannel(channel);
        resolve();
      }
    });

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
      }
    };

    WebChannelBroker._listener(mockEvent);
  });
});
