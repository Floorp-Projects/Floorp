/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Capabilities, Timeouts } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/Capabilities.sys.mjs"
);
const { getWebDriverSessionById, WebDriverSession } =
  ChromeUtils.importESModule(
    "chrome://remote/content/shared/webdriver/Session.sys.mjs"
  );

add_task(function test_WebDriverSession_ctor() {
  const session = new WebDriverSession();

  equal(typeof session.id, "string");
  ok(session.capabilities instanceof Capabilities);
});

add_task(function test_WebDriverSession_destroy() {
  const session = new WebDriverSession();

  session.destroy();
});

add_task(function test_WebDriverSession_getters() {
  const session = new WebDriverSession();

  equal(
    session.a11yChecks,
    session.capabilities.get("moz:accessibilityChecks")
  );
  equal(session.pageLoadStrategy, session.capabilities.get("pageLoadStrategy"));
  equal(session.proxy, session.capabilities.get("proxy"));
  equal(
    session.strictFileInteractability,
    session.capabilities.get("strictFileInteractability")
  );
  equal(session.timeouts, session.capabilities.get("timeouts"));
  equal(
    session.unhandledPromptBehavior,
    session.capabilities.get("unhandledPromptBehavior")
  );
});

add_task(function test_WebDriverSession_setters() {
  const session = new WebDriverSession();

  const timeouts = new Timeouts();
  timeouts.pageLoad = 45;

  session.timeouts = timeouts;
  equal(session.timeouts, session.capabilities.get("timeouts"));
});

add_task(function test_getWebDriverSessionById() {
  const session1 = new WebDriverSession();
  const session2 = new WebDriverSession();

  equal(getWebDriverSessionById(session1.id), session1);
  equal(getWebDriverSessionById(session2.id), session2);

  session1.destroy();
  equal(getWebDriverSessionById(session1.id), undefined);
  equal(getWebDriverSessionById(session2.id), session2);

  session2.destroy();
  equal(getWebDriverSessionById(session1.id), undefined);
  equal(getWebDriverSessionById(session2.id), undefined);
});
