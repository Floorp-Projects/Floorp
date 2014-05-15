/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const StreamUtils = devtools.require("devtools/toolkit/transport/stream-utils");

const StringInputStream = CC("@mozilla.org/io/string-input-stream;1",
                             "nsIStringInputStream", "setData");

function run_test() {
  add_task(function() {
    yield test_delimited_read("0123:", "0123:");
    yield test_delimited_read("0123:4567:", "0123:");
    yield test_delimited_read("012345678901:", "0123456789");
    yield test_delimited_read("0123/0123", "0123/0123");
  });

  run_next_test();
}

/*** Tests ***/

function test_delimited_read(input, expected) {
  input = new StringInputStream(input, input.length);
  let result = StreamUtils.delimitedRead(input, ":", 10);
  do_check_eq(result, expected);
}
