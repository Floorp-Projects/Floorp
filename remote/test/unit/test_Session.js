/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {Session} = ChromeUtils.import("chrome://remote/content/Session.jsm");

const connection = {onmessage: () => {}};

add_test(function test_Session_destructor() {
  const session = new Session(connection);
  session.domains.get("Log");
  equal(session.domains.size, 1);
  session.destructor();
  equal(session.domains.size, 0);

  run_next_test();
});
