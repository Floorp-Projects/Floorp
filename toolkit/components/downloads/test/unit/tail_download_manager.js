/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Provides infrastructure for automated download components tests.
 */

"use strict";

// //////////////////////////////////////////////////////////////////////////////
// // Termination functions common to all tests

add_task(function* test_common_terminate()
{
  // Stop the HTTP server.  We must do this inside a task in "tail.js" until the
  // xpcshell testing framework supports asynchronous termination functions.
  let deferred = Promise.defer();
  gHttpServer.stop(deferred.resolve);
  yield deferred.promise;
});

