/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that nsIPrivateBrowsingChannel.isChannelPrivate yields the correct
 * result for various combinations of .setPrivate() and nsILoadContexts
 */

"use strict";

var URIs = ["http://example.org", "https://example.org"];

function* getChannels() {
  for (let u of URIs) {
    yield NetUtil.newChannel({
      uri: u,
      loadUsingSystemPrincipal: true,
    });
  }
}

function checkPrivate(channel, shouldBePrivate) {
  Assert.equal(
    channel.QueryInterface(Ci.nsIPrivateBrowsingChannel).isChannelPrivate,
    shouldBePrivate
  );
}

/**
 * Default configuration
 * Default is non-private
 */
add_test(function test_plain() {
  for (let c of getChannels()) {
    checkPrivate(c, false);
  }
  run_next_test();
});

/**
 * Explicitly setPrivate(true), no load context
 */
add_test(function test_setPrivate_private() {
  for (let c of getChannels()) {
    c.QueryInterface(Ci.nsIPrivateBrowsingChannel).setPrivate(true);
    checkPrivate(c, true);
  }
  run_next_test();
});

/**
 * Explicitly setPrivate(false), no load context
 */
add_test(function test_setPrivate_regular() {
  for (let c of getChannels()) {
    c.QueryInterface(Ci.nsIPrivateBrowsingChannel).setPrivate(false);
    checkPrivate(c, false);
  }
  run_next_test();
});

/**
 * Load context mandates private mode
 */
add_test(function test_LoadContextPrivate() {
  let ctx = Cu.createPrivateLoadContext();
  for (let c of getChannels()) {
    c.notificationCallbacks = ctx;
    checkPrivate(c, true);
  }
  run_next_test();
});

/**
 * Load context mandates regular mode
 */
add_test(function test_LoadContextRegular() {
  let ctx = Cu.createLoadContext();
  for (let c of getChannels()) {
    c.notificationCallbacks = ctx;
    checkPrivate(c, false);
  }
  run_next_test();
});

// Do not test simultanous uses of .setPrivate and load context.
// There is little merit in doing so, and combining both will assert in
// Debug builds anyway.
