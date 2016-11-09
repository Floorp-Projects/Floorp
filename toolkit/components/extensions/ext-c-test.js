"use strict";

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
 *        - null, the function always returns true.
 * @param {BaseContext} context
 *
 * @returns {boolean}
 *        True if the error matches the expected error.
 */
function errorMatches(error, expectedError, context) {
  if (expectedError === null) {
    return true;
  }

  if (typeof expectedError === "function") {
    return context.runSafeWithoutClone(expectedError, error);
  }

  if (typeof error !== "object" || error == null ||
      typeof error.message !== "string") {
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
}

/**
 * Calls .toSource() on the given value, but handles null, undefined,
 * and errors.
 *
 * @param {*} value
 * @returns {string}
 */
function toSource(value) {
  if (value === null) {
    return null;
  }
  if (value === undefined) {
    return null;
  }
  if (typeof value === "string") {
    return JSON.stringify(value);
  }

  try {
    return String(value.toSource());
  } catch (e) {
    return "<unknown>";
  }
}

function makeTestAPI(context) {
  function assertTrue(...args) {
    context.childManager.callParentFunctionNoReturn("test.assertTrue", args);
  }

  return {
    test: {
      // These functions accept arbitrary values. Convert the parameters to
      // make sure that the values can be cloned structurally for IPC.

      sendMessage(...args) {
        args = Cu.cloneInto(args, context.cloneScope);
        context.childManager.callParentFunctionNoReturn("test.sendMessage", args);
      },

      assertTrue(value, msg) {
        context.childManager.callParentFunctionNoReturn("test.assertTrue", [
          Boolean(value),
          String(msg),
        ]);
      },

      assertFalse(value, msg) {
        context.childManager.callParentFunctionNoReturn("test.assertFalse", [
          Boolean(value),
          String(msg),
        ]);
      },

      assertEq(expected, actual, msg) {
        let equal = expected === actual;
        expected += "";
        actual += "";
        if (!equal && expected === actual) {
          // Add an extra tag so that "expected === actual" in the parent is
          // also false, despite the fact that the serialization is equal.
          actual += " (different)";
        }
        context.childManager.callParentFunctionNoReturn("test.assertEq", [
          expected,
          actual,
          String(msg),
        ]);
      },

      assertRejects(promise, expectedError, msg) {
        // Wrap in a native promise for consistency.
        promise = Promise.resolve(promise);

        if (msg) {
          msg = `: ${msg}`;
        }

        return promise.then(result => {
          assertTrue(false, `Promise resolved, expected rejection${msg}`);
        }, error => {
          let errorMessage = toSource(error && error.message);

          assertTrue(errorMatches(error, expectedError, context),
                     `Promise rejected, expecting rejection to match ${toSource(expectedError)}, ` +
                     `got ${errorMessage}${msg}`);
        });
      },

      assertThrows(func, expectedError, msg) {
        if (msg) {
          msg = `: ${msg}`;
        }

        try {
          func();

          assertTrue(false, `Function did not throw, expected error${msg}`);
        } catch (error) {
          let errorMessage = toSource(error && error.message);

          assertTrue(errorMatches(error, expectedError, context),
                     `Promise rejected, expecting rejection to match ${toSource(expectedError)}` +
                     `got ${errorMessage}${msg}`);
        }
      },
    },
  };
}
extensions.registerSchemaAPI("test", "addon_child", makeTestAPI);
extensions.registerSchemaAPI("test", "content_child", makeTestAPI);

