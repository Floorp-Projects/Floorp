/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyGetter(this, "isXpcshell", function() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  return env.exists("XPCSHELL_TEST_PROFILE_DIR");
});

/**
 * Checks whether the given error matches the given expectations.
 *
 * @param {*} error
 *        The error to check.
 * @param {string|RegExp|function|null} expectedError
 *        The expectation to check against. If this parameter is:
 *
 *        - a string, the error message must exactly equal the string.
 *        - a regular expression, it must match the error message.
 *        - a function, it is called with the error object and its
 *          return value is returned.
 * @param {BaseContext} context
 *
 * @returns {boolean}
 *        True if the error matches the expected error.
 */
const errorMatches = (error, expectedError, context) => {
  if (
    typeof error === "object" &&
    error !== null &&
    !context.principal.subsumes(Cu.getObjectPrincipal(error))
  ) {
    Cu.reportError("Error object belongs to the wrong scope.");
    return false;
  }

  if (typeof expectedError === "function") {
    return context.runSafeWithoutClone(expectedError, error);
  }

  if (
    typeof error !== "object" ||
    error == null ||
    typeof error.message !== "string"
  ) {
    return false;
  }

  if (typeof expectedError === "string") {
    return error.message === expectedError;
  }

  try {
    return expectedError.test(error.message);
  } catch (e) {
    Cu.reportError(e);
  }

  return false;
};

/**
 * Calls .toSource() on the given value, but handles null, undefined,
 * and errors.
 *
 * @param {*} value
 * @returns {string}
 */
const toSource = value => {
  if (value === null) {
    return "null";
  }
  if (value === undefined) {
    return "undefined";
  }
  if (typeof value === "string") {
    return JSON.stringify(value);
  }

  try {
    return String(value);
  } catch (e) {
    return "<unknown>";
  }
};

this.test = class extends ExtensionAPI {
  getAPI(context) {
    const { extension } = context;

    function getStack(savedFrame = null) {
      if (savedFrame) {
        return ChromeUtils.createError("", savedFrame).stack.replace(
          /^/gm,
          "    "
        );
      }
      return new context.Error().stack.replace(/^/gm, "    ");
    }

    function assertTrue(value, msg) {
      extension.emit(
        "test-result",
        Boolean(value),
        String(msg),
        getStack(context.getCaller())
      );
    }

    class TestEventManager extends EventManager {
      constructor(...args) {
        super(...args);

        // A map to keep track of the listeners wrappers being added in
        // addListener (the wrapper will be needed to be able to remove
        // the listener from this EventManager instance if the extension
        // does call test.onMessage.removeListener).
        this._listenerWrappers = new Map();
        context.callOnClose({
          close: () => this._listenerWrappers.clear(),
        });
      }

      addListener(callback, ...args) {
        const listenerWrapper = function(...args) {
          try {
            callback.call(this, ...args);
          } catch (e) {
            assertTrue(false, `${e}\n${e.stack}`);
          }
        };
        super.addListener(listenerWrapper, ...args);
        this._listenerWrappers.set(callback, listenerWrapper);
      }

      removeListener(callback) {
        if (!this._listenerWrappers.has(callback)) {
          return;
        }

        super.removeListener(this._listenerWrappers.get(callback));
        this._listenerWrappers.delete(callback);
      }
    }

    if (!Cu.isInAutomation && !isXpcshell) {
      return { test: {} };
    }

    return {
      test: {
        withHandlingUserInput(callback) {
          // TODO(Bug 1598804): remove this once we don't expose anymore the
          // entire test API namespace based on an environment variable.
          if (!Cu.isInAutomation) {
            // This dangerous method should only be available if the
            // automation pref is set, which is the case in browser tests.
            throw new ExtensionUtils.ExtensionError(
              "withHandlingUserInput can only be called in automation"
            );
          }
          ExtensionCommon.withHandlingUserInput(
            context.contentWindow,
            callback
          );
        },

        sendMessage(...args) {
          extension.emit("test-message", ...args);
        },

        notifyPass(msg) {
          extension.emit("test-done", true, msg, getStack(context.getCaller()));
        },

        notifyFail(msg) {
          extension.emit(
            "test-done",
            false,
            msg,
            getStack(context.getCaller())
          );
        },

        log(msg) {
          extension.emit("test-log", true, msg, getStack(context.getCaller()));
        },

        fail(msg) {
          assertTrue(false, msg);
        },

        succeed(msg) {
          assertTrue(true, msg);
        },

        assertTrue(value, msg) {
          assertTrue(value, msg);
        },

        assertFalse(value, msg) {
          assertTrue(!value, msg);
        },

        assertEq(expected, actual, msg) {
          let equal = expected === actual;

          expected = String(expected);
          actual = String(actual);

          if (!equal && expected === actual) {
            actual += " (different)";
          }
          extension.emit(
            "test-eq",
            equal,
            String(msg),
            expected,
            actual,
            getStack(context.getCaller())
          );
        },

        assertRejects(promise, expectedError, msg) {
          // Wrap in a native promise for consistency.
          promise = Promise.resolve(promise);

          return promise.then(
            result => {
              let message = `Promise resolved, expected rejection '${toSource(
                expectedError
              )}'`;
              if (msg) {
                message += `: ${msg}`;
              }
              assertTrue(false, message);
            },
            error => {
              let expected = toSource(expectedError);
              let message = `got '${toSource(error)}'`;
              if (msg) {
                message += `: ${msg}`;
              }

              assertTrue(
                errorMatches(error, expectedError, context),
                `Promise rejected, expecting rejection to match '${expected}', ${message}`
              );
            }
          );
        },

        assertThrows(func, expectedError, msg) {
          try {
            func();

            let message = `Function did not throw, expected error '${toSource(
              expectedError
            )}'`;
            if (msg) {
              message += `: ${msg}`;
            }
            assertTrue(false, message);
          } catch (error) {
            let expected = toSource(expectedError);
            let message = `got '${toSource(error)}'`;
            if (msg) {
              message += `: ${msg}`;
            }

            assertTrue(
              errorMatches(error, expectedError, context),
              `Function threw, expecting error to match '${expected}', ${message}`
            );
          }
        },

        onMessage: new TestEventManager({
          context,
          name: "test.onMessage",
          register: fire => {
            let handler = (event, ...args) => {
              fire.async(...args);
            };

            extension.on("test-harness-message", handler);
            return () => {
              extension.off("test-harness-message", handler);
            };
          },
        }).api(),
      },
    };
  }
};
