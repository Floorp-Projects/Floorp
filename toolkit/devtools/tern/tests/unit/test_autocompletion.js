/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that tern autocompletions work.
 */

const tern = require("tern/tern");
const ecma5 = require("tern/ecma5");

function run_test() {
  do_test_pending();

  const server = new tern.Server({ defs: [ecma5] });
  const code = "[].";
  const query = { type: "completions", file: "test", end: code.length };
  const files = [{ type: "full", name: "test", text: code }];

  server.request({ query: query, files: files }, (error, response) => {
    do_check_eq(error, null);
    do_check_true(!!response);
    do_check_true(Array.isArray(response.completions));
    do_check_true(response.completions.indexOf("concat") != -1);
    do_test_finished();
  });
}
