/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Detects and reports unhandled rejections during test runs. Test harnesses
 * will fail tests in this case, unless the test explicitly allows rejections.
 */

"use strict";

var EXPORTED_SYMBOLS = ["PromiseTestUtils"];

ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://testing-common/Assert.jsm", this);

// Keep "JSMPromise" separate so "Promise" still refers to DOM Promises.
let JSMPromise = ChromeUtils.import("resource://gre/modules/Promise.jsm", {})
  .Promise;

var PromiseTestUtils = {
  /**
   * Array of objects containing the details of the Promise rejections that are
   * currently left uncaught. This includes DOM Promise and Promise.jsm. When
   * rejections in DOM Promises are consumed, they are removed from this list.
   *
   * The objects contain at least the following properties:
   * {
   *   message: The error message associated with the rejection, if any.
   *   date: Date object indicating when the rejection was observed.
   *   id: For DOM Promise only, the Promise ID from PromiseDebugging. This is
   *       only used for tracking and should not be checked by the callers.
   *   stack: nsIStackFrame, SavedFrame, or string indicating the stack at the
   *          time the rejection was triggered. May also be null if the
   *          rejection was triggered while a script was on the stack.
   * }
   */
  _rejections: [],

  /**
   * When an uncaught rejection is detected, it is ignored if one of the
   * functions in this array returns true when called with the rejection details
   * as its only argument. When a function matches an expected rejection, it is
   * then removed from the array.
   */
  _rejectionIgnoreFns: [],

  /**
   * If any of the functions in this array returns true when called with the
   * rejection details as its only argument, the rejection is ignored. This
   * happens after the "_rejectionIgnoreFns" array is processed.
   */
  _globalRejectionIgnoreFns: [],

  /**
   * Called only by the test infrastructure, registers the rejection observers.
   *
   * This should be called only once, and a matching "uninit" call must be made
   * or the tests will crash on shutdown.
   */
  init() {
    if (this._initialized) {
      Cu.reportError("This object was already initialized.");
      return;
    }

    PromiseDebugging.addUncaughtRejectionObserver(this);

    // Promise.jsm rejections are only reported to this observer when requested,
    // so we don't have to store a key to remove them when consumed.
    JSMPromise.Debugging.addUncaughtErrorObserver(rejection =>
      this._rejections.push(rejection)
    );

    this._initialized = true;
  },
  _initialized: false,

  /**
   * Called only by the test infrastructure, unregisters the observers.
   */
  uninit() {
    if (!this._initialized) {
      return;
    }

    PromiseDebugging.removeUncaughtRejectionObserver(this);
    JSMPromise.Debugging.clearUncaughtErrorObservers();

    this._initialized = false;
  },

  /**
   * Called only by the test infrastructure, spins the event loop until the
   * messages for pending DOM Promise rejections have been processed.
   */
  ensureDOMPromiseRejectionsProcessed() {
    let observed = false;
    let observer = {
      onLeftUncaught: promise => {
        if (
          PromiseDebugging.getState(promise).reason ===
          this._ensureDOMPromiseRejectionsProcessedReason
        ) {
          observed = true;
          return true;
        }
        return false;
      },
      onConsumed() {},
    };

    PromiseDebugging.addUncaughtRejectionObserver(observer);
    Promise.reject(this._ensureDOMPromiseRejectionsProcessedReason);
    Services.tm.spinEventLoopUntil(() => observed);
    PromiseDebugging.removeUncaughtRejectionObserver(observer);
  },
  _ensureDOMPromiseRejectionsProcessedReason: {},

  /**
   * Called only by the tests for PromiseDebugging.addUncaughtRejectionObserver
   * and for JSMPromise.Debugging, disables the observers in this module.
   */
  disableUncaughtRejectionObserverForSelfTest() {
    this.uninit();
  },

  /**
   * Called by tests with uncaught rejections to disable the observers in this
   * module. For new tests where uncaught rejections are expected, you should
   * use the more granular expectUncaughtRejection function instead.
   */
  thisTestLeaksUncaughtRejectionsAndShouldBeFixed() {
    this.uninit();
  },

  // UncaughtRejectionObserver
  onLeftUncaught(promise) {
    let message = "(Unable to convert rejection reason to string.)";
    let reason = null;
    try {
      reason = PromiseDebugging.getState(promise).reason;
      if (reason === this._ensureDOMPromiseRejectionsProcessedReason) {
        // Ignore the special promise for ensureDOMPromiseRejectionsProcessed.
        return;
      }
      message = reason.message || "" + reason;
    } catch (ex) {}

    // We should convert the rejection stack to a string immediately. This is
    // because the object might not be available when we report the rejection
    // later, if the error occurred in a context that has been unloaded.
    let stack = "(Unable to convert rejection stack to string.)";
    try {
      // In some cases, the rejection stack from `PromiseDebugging` may be null.
      // If the rejection reason was an Error object, use its `stack` to recover
      // a meaningful value.
      stack =
        "" +
        ((reason && reason.stack) ||
          PromiseDebugging.getRejectionStack(promise) ||
          "(No stack available.)");
    } catch (ex) {}

    // Always add a newline at the end of the stack for consistent reporting.
    // This is already present when the stack is provided by PromiseDebugging.
    if (!stack.endsWith("\n")) {
      stack += "\n";
    }

    // It's important that we don't store any reference to the provided Promise
    // object or its value after this function returns in order to avoid leaks.
    this._rejections.push({
      id: PromiseDebugging.getPromiseID(promise),
      message,
      date: new Date(),
      stack,
    });
  },

  // UncaughtRejectionObserver
  onConsumed(promise) {
    // We don't expect that many unhandled rejections will appear at the same
    // time, so the algorithm doesn't need to be optimized for that case.
    let id = PromiseDebugging.getPromiseID(promise);
    let index = this._rejections.findIndex(rejection => rejection.id == id);
    // If we get a consumption notification for a rejection that was left
    // uncaught before this module was initialized, we can safely ignore it.
    if (index != -1) {
      this._rejections.splice(index, 1);
    }
  },

  /**
   * Informs the test suite that the test code will generate a Promise rejection
   * that will still be unhandled when the test file terminates.
   *
   * This method must be called once for each instance of Promise that is
   * expected to be uncaught, even if the rejection reason is the same for each
   * instance.
   *
   * If the expected rejection does not occur, the test will fail.
   *
   * @param regExpOrCheckFn
   *        This can either be a regular expression that should match the error
   *        message of the rejection, or a check function that is invoked with
   *        the rejection details object as its first argument.
   */
  expectUncaughtRejection(regExpOrCheckFn) {
    let checkFn = !("test" in regExpOrCheckFn)
      ? regExpOrCheckFn
      : rejection => regExpOrCheckFn.test(rejection.message);
    this._rejectionIgnoreFns.push(checkFn);
  },

  /**
   * Allows an entire class of Promise rejections. Usage of this function
   * should be kept to a minimum because it has a broad scope and doesn't
   * prevent new unhandled rejections of this class from being added.
   *
   * @param regExp
   *        This should match the error message of the rejection.
   */
  allowMatchingRejectionsGlobally(regExp) {
    this._globalRejectionIgnoreFns.push(rejection =>
      regExp.test(rejection.message)
    );
  },

  /**
   * Fails the test if there are any uncaught rejections at this time that have
   * not been explicitly allowed using expectUncaughtRejection.
   *
   * Depending on the configuration of the test suite, this function might only
   * report the details of the first uncaught rejection that was generated.
   *
   * This is called by the test suite at the end of each test function.
   */
  assertNoUncaughtRejections() {
    // Ask Promise.jsm to report all uncaught rejections to the observer now.
    JSMPromise.Debugging.flushUncaughtErrors();

    // If there is any uncaught rejection left at this point, the test fails.
    while (this._rejections.length) {
      let rejection = this._rejections.shift();

      // If one of the ignore functions matches, ignore the rejection, then
      // remove the function so that each function only matches one rejection.
      let index = this._rejectionIgnoreFns.findIndex(f => f(rejection));
      if (index != -1) {
        this._rejectionIgnoreFns.splice(index, 1);
        continue;
      }

      // Check the global ignore functions.
      if (this._globalRejectionIgnoreFns.some(fn => fn(rejection))) {
        continue;
      }

      // Report the error. This operation can throw an exception, depending on
      // the configuration of the test suite that handles the assertion. The
      // first line of the message, including the latest call on the stack, is
      // used to identify related test failures. To keep the first line similar
      // between executions, we place the time-dependent rejection date on its
      // own line, after all the other stack lines.
      Assert.ok(
        false,
        `A promise chain failed to handle a rejection:` +
          ` ${rejection.message} - stack: ${rejection.stack}` +
          `Rejection date: ${rejection.date}`
      );
    }
  },

  /**
   * Fails the test if any rejection indicated by expectUncaughtRejection has
   * not yet been reported at this time.
   *
   * This is called by the test suite at the end of each test file.
   */
  assertNoMoreExpectedRejections() {
    // Only log this condition is there is a failure.
    if (this._rejectionIgnoreFns.length) {
      Assert.equal(
        this._rejectionIgnoreFns.length,
        0,
        "Unable to find a rejection expected by expectUncaughtRejection."
      );
    }
    // Reset the list of expected rejections in case the test suite continues.
    this._rejectionIgnoreFns = [];
  },
};
