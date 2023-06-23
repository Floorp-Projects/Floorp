"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

/**
 * A trivial test ensuring that we can call osfile from xpcshell.
 * (see bug 808161)
 */

function run_test() {
  do_test_pending();
  OS.File.getCurrentDirectory().then(do_test_finished, do_test_finished);
}
