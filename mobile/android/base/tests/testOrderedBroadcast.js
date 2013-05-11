// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Components.utils.import("resource://gre/modules/OrderedBroadcast.jsm");
Components.utils.import("resource://gre/modules/commonjs/sdk/core/promise.js");

let _observerId = 0;

function makeObserver() {
  let deferred = Promise.defer();

  let ret = {
    id: _observerId++,
    count: 0,
    promise: deferred.promise,
    callback: function(data, token, action) {
      ret.count += 1;
      deferred.resolve({ data: data,
                         token: token,
                         action: action });
    },
  };

  return ret;
};

add_task(function test_send() {
  let deferred = Promise.defer();

  let observer = makeObserver();

  sendOrderedBroadcast("org.mozilla.gecko.test.receiver",
                       { a: "bcde", b: 1234 }, observer.callback);

  let value = yield observer.promise;

  do_check_eq(observer.count, 1);

  // We get back the correct action and token.
  do_check_neq(value, null);
  do_check_neq(value.token, null);
  do_check_eq(value.token.a, "bcde");
  do_check_eq(value.token.b, 1234);
  do_check_eq(value.action, "org.mozilla.gecko.test.receiver");

  // Data is provided by testOrderedBroadcast.java.in.
  do_check_neq(value.data, null);
  do_check_eq(value.data.c, "efg");
  do_check_eq(value.data.d, 456);
});

add_task(function test_null_token() {
  let deferred = Promise.defer();

  let observer = makeObserver();

  sendOrderedBroadcast("org.mozilla.gecko.test.receiver",
                       null, observer.callback);

  let value = yield observer.promise;

  do_check_eq(observer.count, 1);

  // We get back the correct action and token.
  do_check_neq(value, null);
  do_check_eq(value.token, null);
  do_check_eq(value.action, "org.mozilla.gecko.test.receiver");

  // Data is provided by testOrderedBroadcast.java.in.
  do_check_neq(value.data, null);
  do_check_eq(value.data.c, "efg");
  do_check_eq(value.data.d, 456);
});

add_task(function test_permission() {
  let deferred = Promise.defer();

  let observer = makeObserver();

  sendOrderedBroadcast("org.mozilla.gecko.test.receiver",
                       null, observer.callback,
                       "org.mozilla.gecko.fake.permission");

  let value = yield observer.promise;

  do_check_eq(observer.count, 1);

  // We get back the correct action and token.
  do_check_neq(value, null);
  do_check_eq(value.token, null);
  do_check_eq(value.action, "org.mozilla.gecko.test.receiver");

  // Data would be provided by testOrderedBroadcast.java.in, except
  // the no package has the permission, so no responder exists.
  do_check_eq(value.data, null);
});

add_task(function test_send_no_receiver() {
  let deferred = Promise.defer();

  let observer = makeObserver();

  sendOrderedBroadcast("org.mozilla.gecko.test.no.receiver",
                       { a: "bcd", b: 123 }, observer.callback);

  let value = yield observer.promise;

  do_check_eq(observer.count, 1);

  // We get back the correct action and token, but the default is to
  // return no data.
  do_check_neq(value, null);
  do_check_neq(value.token, null);
  do_check_eq(value.token.a, "bcd");
  do_check_eq(value.token.b, 123);
  do_check_eq(value.action, "org.mozilla.gecko.test.no.receiver");
  do_check_eq(value.data, null);
});

run_next_test();
