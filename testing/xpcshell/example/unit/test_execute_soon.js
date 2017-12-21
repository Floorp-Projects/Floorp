/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 * ***** END LICENSE BLOCK ***** */

var complete = false;

function run_test() {
  dump("Starting test\n");
  do_register_cleanup(function() {
    dump("Checking test completed\n");
    Assert.ok(complete);
  });

  do_execute_soon(function execute_soon_callback() {
    dump("do_execute_soon callback\n");
    complete = true;
  });
}
