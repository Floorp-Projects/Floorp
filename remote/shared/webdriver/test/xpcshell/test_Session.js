/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Timeouts } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs"
);
const { getWebDriverSessionById, WebDriverSession } =
  ChromeUtils.importESModule(
    "chrome://remote/content/shared/webdriver/Session.sys.mjs"
  );

function createSession(options = {}) {
  const { capabilities = {}, connection, isBidi = false } = options;

  const flags = new Set();
  if (isBidi) {
    flags.add("bidi");
  } else {
    flags.add("http");
  }

  return new WebDriverSession(capabilities, flags, connection);
}

add_task(function test_WebDriverSession_ctor() {
  // Missing WebDriver session flags
  Assert.throws(() => new WebDriverSession({}), /TypeError/);

  // Session needs to be either HTTP or BiDi
  for (const flags of [[], ["bidi", "http"]]) {
    Assert.throws(
      () => new WebDriverSession({}, new Set(flags)),
      /SessionNotCreatedError:/
    );
  }

  // Session id and path
  let session = createSession();
  equal(typeof session.id, "string");
  equal(session.path, `/session/${session.id}`);

  // Sets HTTP and BiDi flags correctly.
  session = createSession({ isBidi: false });
  equal(session.bidi, false);
  equal(session.http, true);
  session = createSession({ isBidi: true });
  equal(session.bidi, true);
  equal(session.http, false);

  // Sets capabilities based on session configuration flag.
  const capabilities = {
    acceptInsecureCerts: true,
    unhandledPromptBehavior: "ignore",

    // HTTP only
    pageLoadStrategy: "eager",
    strictFileInteractability: true,
    timeouts: { script: 1000 },
  };

  // HTTP session
  session = createSession({ capabilities, isBidi: false });
  equal(session.acceptInsecureCerts, true);
  equal(session.pageLoadStrategy, "eager");
  equal(session.strictFileInteractability, true);
  equal(session.timeouts.script, 1000);
  equal(session.userPromptHandler.toJSON(), "ignore");

  // BiDi session
  session = createSession({ capabilities, isBidi: true });
  equal(session.acceptInsecureCerts, true);
  equal(session.userPromptHandler.toJSON(), "dismiss and notify");

  equal(session.pageLoadStrategy, undefined);
  equal(session.strictFileInteractability, undefined);
  equal(session.timeouts, undefined);
});

add_task(function test_WebDriverSession_destroy() {
  const session = createSession();

  session.destroy();

  // Calling twice doesn't raise error.
  session.destroy();
});

add_task(function test_WebDriverSession_getters() {
  const session = createSession();

  equal(
    session.a11yChecks,
    session.capabilities.get("moz:accessibilityChecks")
  );
  equal(
    session.acceptInsecureCerts,
    session.capabilities.get("acceptInsecureCerts")
  );
  equal(session.pageLoadStrategy, session.capabilities.get("pageLoadStrategy"));
  equal(session.proxy, session.capabilities.get("proxy"));
  equal(
    session.strictFileInteractability,
    session.capabilities.get("strictFileInteractability")
  );
  equal(session.timeouts, session.capabilities.get("timeouts"));
  equal(
    session.userPromptHandler,
    session.capabilities.get("unhandledPromptBehavior")
  );
});

add_task(function test_WebDriverSession_setters() {
  const session = createSession();

  const timeouts = new Timeouts();
  timeouts.pageLoad = 45;

  session.timeouts = timeouts;
  equal(session.timeouts, session.capabilities.get("timeouts"));
});

add_task(function test_getWebDriverSessionById() {
  const session1 = createSession();
  const session2 = createSession();

  equal(getWebDriverSessionById(session1.id), session1);
  equal(getWebDriverSessionById(session2.id), session2);

  session1.destroy();
  equal(getWebDriverSessionById(session1.id), undefined);
  equal(getWebDriverSessionById(session2.id), session2);

  session2.destroy();
  equal(getWebDriverSessionById(session1.id), undefined);
  equal(getWebDriverSessionById(session2.id), undefined);
});
