/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* jshint esnext:true, globalstrict:true, moz:true, undef:true, unused:true */
/* globals Cc, Cu, Ci, Assert, run_next_test, add_test, do_register_cleanup */
'use strict';

/* globals Services */
Cu.import('resource://gre/modules/Services.jsm');

add_test(function test_ipv4_any() {
  let socket = Cc['@mozilla.org/network/udp-socket;1'].createInstance(Ci.nsIUDPSocket);

  Assert.throws(() => {
    socket.init(-1, false, Services.scriptSecurityManager.getSystemPrincipal());
  }, /NS_ERROR_OFFLINE/);

  run_next_test();
});

add_test(function test_ipv6_any() {
  let socket = Cc['@mozilla.org/network/udp-socket;1'].createInstance(Ci.nsIUDPSocket);

  Assert.throws(() => {
    socket.init2('::', -1, Services.scriptSecurityManager.getSystemPrincipal());
  }, /NS_ERROR_OFFLINE/);

  run_next_test();
});

add_test(function test_ipv4() {
  let socket = Cc['@mozilla.org/network/udp-socket;1'].createInstance(Ci.nsIUDPSocket);

  Assert.throws(() => {
    socket.init2('240.0.0.1', -1, Services.scriptSecurityManager.getSystemPrincipal());
  }, /NS_ERROR_OFFLINE/);

  run_next_test();
});

add_test(function test_ipv6() {
  let socket = Cc['@mozilla.org/network/udp-socket;1'].createInstance(Ci.nsIUDPSocket);

  Assert.throws(() => {
    socket.init2('2001:db8::1', -1, Services.scriptSecurityManager.getSystemPrincipal());
  }, /NS_ERROR_OFFLINE/);

  run_next_test();
});

add_test(function test_ipv4_loopback() {
  let socket = Cc['@mozilla.org/network/udp-socket;1'].createInstance(Ci.nsIUDPSocket);

  try {
    socket.init2('127.0.0.1', -1, Services.scriptSecurityManager.getSystemPrincipal(), true);
  } catch (e) {
    Assert.ok(false, 'unexpected exception: ' + e);
  }

  run_next_test();
});

add_test(function test_ipv6_loopback() {
  let socket = Cc['@mozilla.org/network/udp-socket;1'].createInstance(Ci.nsIUDPSocket);

  try {
    socket.init2('::1', -1, Services.scriptSecurityManager.getSystemPrincipal(), true);
  } catch (e) {
    Assert.ok(false, 'unexpected exception: ' + e);
  }

  run_next_test();
});

function run_test(){ // jshint ignore:line
  Services.io.offline = true;
  registerCleanupFunction(() => {
    Services.io.offline = false;
  });
  run_next_test();
}
