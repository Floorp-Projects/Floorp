/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Ensure httpd.js can be imported as a module and that a server starts.
 */
function run_test() {
  const { HttpServer } = ChromeUtils.importESModule(
    "resource://testing-common/httpd.sys.mjs"
  );

  let server = new HttpServer();
  server.start(-1);

  do_test_pending();

  server.stop(do_test_finished);
}
