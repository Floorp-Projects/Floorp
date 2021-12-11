/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * ***** END LICENSE BLOCK ***** */

var complete = false;

function run_test() {
  dump("Starting test\n");
  registerCleanupFunction(function() {
    dump("Checking test completed\n");
    Assert.ok(complete);
  });

  executeSoon(function execute_soon_callback() {
    dump("do_execute_soon callback\n");
    complete = true;
  });
}
