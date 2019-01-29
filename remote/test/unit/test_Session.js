/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
