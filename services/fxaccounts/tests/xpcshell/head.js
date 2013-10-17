/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

(function initFxAccountsTestingInfrastructure() {
  do_get_profile();

  let ns = {};
  Components.utils.import("resource://testing-common/services-common/logging.js",
                          ns);

  ns.initTestLogging("Trace");
}).call(this);

/**
 * Test whether specified function throws exception with expected
 * result.
 *
 * @param func
 *        Function to be tested.
 * @param message
 *        Message of expected exception. <code>null</code> for no throws.
 * @param stack
 *        Optional stack object to be printed. <code>null</code> for
 *        Components#stack#caller.
 */
function do_check_throws(func, message, stack)
{
  if (!stack)
    stack = Components.stack.caller;

  try {
    func();
  } catch (exc) {
    if (exc.message === message) {
      return;
    }
    do_throw("expecting exception '" + message
             + "', caught '" + exc.message + "'", stack);
  }

  if (message) {
    do_throw("expecting exception '" + message + "', none thrown", stack);
  }
}
