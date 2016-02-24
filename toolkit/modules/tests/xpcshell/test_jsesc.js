/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/third_party/jsesc/jsesc.js");

function run_test() {
  do_check_eq(jsesc("teééést", {lowercaseHex: true}), "te\\xe9\\xe9\\xe9st");
  do_check_eq(jsesc("teééést", {lowercaseHex: false}), "te\\xE9\\xE9\\xE9st");
}
