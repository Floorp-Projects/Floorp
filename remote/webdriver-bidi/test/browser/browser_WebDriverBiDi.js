/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { RemoteAgent: RemoteAgentModule } = ChromeUtils.importESModule(
  "chrome://remote/content/components/RemoteAgent.sys.mjs"
);
const { WebDriverBiDi } = ChromeUtils.importESModule(
  "chrome://remote/content/webdriver-bidi/WebDriverBiDi.sys.mjs"
);

async function runBiDiTest(test, options = {}) {
  const { capabilities = {}, connection, isBidi = false } = options;

  const flags = new Set();
  if (isBidi) {
    flags.add("bidi");
  } else {
    flags.add("http");
  }

  const wdBiDi = new WebDriverBiDi(RemoteAgentModule);
  await wdBiDi.createSession(capabilities, flags, connection);

  await test(wdBiDi);

  wdBiDi.deleteSession();
}

add_task(async function test_createSession() {
  // Missing WebDriver session flags
  const bidi = new WebDriverBiDi(RemoteAgent);
  await Assert.rejects(bidi.createSession({}), /TypeError/);

  // Session needs to be either HTTP or BiDi
  for (const flags of [[], ["bidi", "http"]]) {
    await Assert.rejects(
      bidi.createSession({}, new Set(flags)),
      /SessionNotCreatedError:/
    );
  }

  // Session id and path
  await runBiDiTest(wdBiDi => {
    is(typeof wdBiDi.session.id, "string");
    is(wdBiDi.session.path, `/session/${wdBiDi.session.id}`);
  });

  // Sets HTTP and BiDi flags correctly.
  await runBiDiTest(
    wdBiDi => {
      is(wdBiDi.session.bidi, false);
      is(wdBiDi.session.http, true);
    },
    { isBidi: false }
  );
  await runBiDiTest(
    wdBiDi => {
      is(wdBiDi.session.bidi, true);
      is(wdBiDi.session.http, false);
    },
    { isBidi: true }
  );

  // Sets capabilities based on session configuration flag.
  const capabilities = {
    acceptInsecureCerts: true,
    unhandledPromptBehavior: "ignore",

    // HTTP only
    pageLoadStrategy: "eager",
    strictFileInteractability: true,
    timeouts: { script: 1000 },
    // Bug 1731730: Requires matching for session.new command.
    // webSocketUrl: false,
  };

  // HTTP session (no webSocketUrl)
  await runBiDiTest(
    wdBiDi => {
      is(wdBiDi.session.bidi, false);
      is(wdBiDi.session.acceptInsecureCerts, true);
      is(wdBiDi.session.pageLoadStrategy, "eager");
      is(wdBiDi.session.strictFileInteractability, true);
      is(wdBiDi.session.timeouts.script, 1000);
      is(wdBiDi.session.userPromptHandler.toJSON(), "ignore");
      is(wdBiDi.session.webSocketUrl, undefined);
    },
    { capabilities, isBidi: false }
  );

  // HTTP session (with webSocketUrl)
  capabilities.webSocketUrl = true;
  await runBiDiTest(
    wdBiDi => {
      is(wdBiDi.session.bidi, true);
      is(wdBiDi.session.acceptInsecureCerts, true);
      is(wdBiDi.session.pageLoadStrategy, "eager");
      is(wdBiDi.session.strictFileInteractability, true);
      is(wdBiDi.session.timeouts.script, 1000);
      is(wdBiDi.session.userPromptHandler.toJSON(), "ignore");
      is(
        wdBiDi.session.webSocketUrl,
        `ws://127.0.0.1:9222/session/${wdBiDi.session.id}`
      );
    },
    { capabilities, isBidi: false }
  );

  // BiDi session
  await runBiDiTest(
    wdBiDi => {
      is(wdBiDi.session.bidi, true);
      is(wdBiDi.session.acceptInsecureCerts, true);
      is(wdBiDi.session.userPromptHandler.toJSON(), "dismiss and notify");

      is(wdBiDi.session.pageLoadStrategy, undefined);
      is(wdBiDi.session.strictFileInteractability, undefined);
      is(wdBiDi.session.timeouts, undefined);
      is(wdBiDi.session.webSocketUrl, undefined);
    },
    { capabilities, isBidi: true }
  );
});
