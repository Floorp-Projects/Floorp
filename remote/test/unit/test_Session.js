/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {Session} = ChromeUtils.import("chrome://remote/content/sessions/Session.jsm");

const connection = {
  registerSession: () => {},
  transport: {
    on: () => {},
  },
};

class MockTarget {
  constructor() {
  }

  get browsingContext() {
    return {id: 42};
  }

  get mm() {
    return {
      addMessageListener() {},
      removeMessageListener() {},
      loadFrameScript() {},
      sendAsyncMessage() {},
    };
  }
}

add_test(function test_Session_destructor() {
  const session = new Session(connection, new MockTarget());
  session.domains.get("Browser");
  equal(session.domains.size, 1);
  session.destructor();
  equal(session.domains.size, 0);

  run_next_test();
});
