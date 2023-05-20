/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests navigation to external protocol from csp-sandboxed iframes.
 */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.block_external_protocol_navigation_from_sandbox", true]],
  });

  await setupMailHandler();
});

add_task(async function test_sandbox_csp() {
  for (let triggerMethod of [
    "trustedClick",
    "untrustedClick",
    "trustedLocationAPI",
    "untrustedLocationAPI",
  ]) {
    await runExtProtocolSandboxTest({
      blocked: false,
      sandbox: "allow-scripts",
      useCSPSandbox: true,
      triggerMethod,
    });
  }

  await runExtProtocolSandboxTest({
    blocked: false,
    sandbox: "allow-scripts allow-top-navigation-to-custom-protocols",
    useCSPSandbox: true,
  });
});
