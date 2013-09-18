/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Provides infrastructure for automated download components tests.
 */

"use strict";

////////////////////////////////////////////////////////////////////////////////
//// Termination functions common to all tests

add_task(function test_common_terminate()
{
  // Ensure all the pending HTTP requests have a chance to finish.
  continueResponses();

  // Stop the HTTP server.  We must do this inside a task in "tail.js" until the
  // xpcshell testing framework supports asynchronous termination functions.
  let deferred = Promise.defer();
  gHttpServer.stop(deferred.resolve);
  yield deferred.promise;
});
