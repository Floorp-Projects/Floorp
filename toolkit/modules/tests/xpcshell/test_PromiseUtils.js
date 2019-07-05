/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);

// Tests for PromiseUtils.jsm

// Tests for PromiseUtils.defer()

/* Tests for checking the resolve method of the Deferred object
 * returned by PromiseUtils.defer() */
add_task(async function test_resolve_string() {
  let def = PromiseUtils.defer();
  let expected = "The promise is resolved " + Math.random();
  def.resolve(expected);
  let result = await def.promise;
  Assert.equal(result, expected, "def.resolve() resolves the promise");
});

/* Test for the case when undefined is passed to the resolve method
 * of the Deferred object */
add_task(async function test_resolve_undefined() {
  let def = PromiseUtils.defer();
  def.resolve();
  let result = await def.promise;
  Assert.equal(result, undefined, "resolve works with undefined as well");
});

/* Test when a pending promise is passed to the resolve method
 * of the Deferred object */
add_task(async function test_resolve_pending_promise() {
  let def = PromiseUtils.defer();
  let expected = 100 + Math.random();
  let p = new Promise((resolve, reject) => {
    setTimeout(() => resolve(expected), 100);
  });
  def.resolve(p);
  let result = await def.promise;
  Assert.equal(
    result,
    expected,
    "def.promise assumed the state of the passed promise"
  );
});

/* Test when a resovled promise is passed
 * to the resolve method of the Deferred object */
add_task(async function test_resolve_resolved_promise() {
  let def = PromiseUtils.defer();
  let expected = "Yeah resolved" + Math.random();
  let p = new Promise((resolve, reject) => resolve(expected));
  def.resolve(p);
  let result = await def.promise;
  Assert.equal(
    result,
    expected,
    "Resolved promise is passed to the resolve method"
  );
});

/* Test for the case when a rejected promise is
 * passed to the resolve method */
add_task(async function test_resolve_rejected_promise() {
  let def = PromiseUtils.defer();
  let p = new Promise((resolve, reject) =>
    reject(new Error("There its an rejection"))
  );
  def.resolve(p);
  await Assert.rejects(
    def.promise,
    /There its an rejection/,
    "Settled rejection promise passed to the resolve method"
  );
});

/* Test for the checking the reject method of
 * the Deferred object returned by PromiseUtils.defer() */
add_task(async function test_reject_Error() {
  let def = PromiseUtils.defer();
  def.reject(new Error("This one rejects"));
  await Assert.rejects(
    def.promise,
    /This one rejects/,
    "reject method with Error for rejection"
  );
});

/* Test for the case when a pending Promise is passed to
 * the reject method of Deferred object */
add_task(async function test_reject_pending_promise() {
  let def = PromiseUtils.defer();
  let p = new Promise((resolve, reject) => {
    setTimeout(() => resolve(100), 100);
  });
  def.reject(p);
  await Assert.rejects(
    def.promise,
    Promise,
    "Rejection with a pending promise uses the passed promise itself as the reason of rejection"
  );
});

/* Test for the case when a resolved Promise
 * is passed to the reject method */
add_task(async function test_reject_resolved_promise() {
  let def = PromiseUtils.defer();
  let p = new Promise((resolve, reject) => resolve("This resolved"));
  def.reject(p);
  await Assert.rejects(
    def.promise,
    Promise,
    "Rejection with a resolved promise uses the passed promise itself as the reason of rejection"
  );
});

/* Test for the case when a rejected Promise is
 * passed to the reject method */
add_task(async function test_reject_resolved_promise() {
  PromiseTestUtils.expectUncaughtRejection(/This one rejects/);
  let def = PromiseUtils.defer();
  let p = new Promise((resolve, reject) =>
    reject(new Error("This one rejects"))
  );
  def.reject(p);
  await Assert.rejects(
    def.promise,
    Promise,
    "Rejection with a rejected promise uses the passed promise itself as the reason of rejection"
  );
});
