/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests navigation to external protocol from sandboxed iframes.
 */

"use strict";

requestLongerTimeout(2);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.block_external_protocol_navigation_from_sandbox", true]],
  });

  await setupMailHandler();
});

add_task(async function test_sandbox_disabled() {
  await runExtProtocolSandboxTest({ blocked: false, sandbox: null });
});

add_task(async function test_sandbox_allowed() {
  let flags = [
    "allow-popups",
    "allow-top-navigation",
    "allow-top-navigation-by-user-activation",
    "allow-top-navigation-to-custom-protocols",
  ];

  for (let flag of flags) {
    await runExtProtocolSandboxTest({
      blocked: false,
      sandbox: `allow-scripts ${flag}`,
    });
  }
});

add_task(async function test_sandbox_blocked() {
  let flags = [
    "",
    "allow-same-origin",
    "allow-forms",
    "allow-scripts",
    "allow-pointer-lock",
    "allow-orientation-lock",
    "allow-modals",
    "allow-popups-to-escape-sandbox",
    "allow-presentation",
    "allow-storage-access-by-user-activation",
    "allow-downloads",
  ];

  for (let flag of flags) {
    await runExtProtocolSandboxTest({
      blocked: true,
      sandbox: `allow-scripts ${flag}`,
    });
  }
});

add_task(async function test_sandbox_blocked_triggers() {
  info(
    "For sandboxed frames external protocol navigation is blocked, no matter how it is triggered."
  );
  for (let triggerMethod of [
    "trustedClick",
    "untrustedClick",
    "trustedLocationAPI",
    "untrustedLocationAPI",
    "frameSrc",
  ]) {
    await runExtProtocolSandboxTest({
      blocked: true,
      sandbox: "allow-scripts",
      triggerMethod,
    });
  }

  info(
    "When allow-top-navigation-by-user-activation navigation to external protocols with transient user activations is allowed."
  );
  await runExtProtocolSandboxTest({
    blocked: false,
    sandbox: "allow-scripts allow-top-navigation-by-user-activation",
    triggerMethod: "trustedClick",
  });

  await runExtProtocolSandboxTest({
    blocked: true,
    sandbox: "allow-scripts allow-top-navigation-by-user-activation",
    triggerMethod: "untrustedClick",
  });

  await runExtProtocolSandboxTest({
    blocked: true,
    sandbox: "allow-scripts allow-top-navigation-by-user-activation",
    triggerMethod: "untrustedLocationAPI",
  });

  await runExtProtocolSandboxTest({
    blocked: true,
    sandbox: "allow-scripts allow-top-navigation-by-user-activation",
    triggerMethod: "frameSrc",
  });
});

add_task(async function test_sandbox_combination() {
  await runExtProtocolSandboxTest({
    blocked: false,
    sandbox:
      "allow-scripts allow-downloads allow-top-navigation-to-custom-protocols",
  });

  await runExtProtocolSandboxTest({
    blocked: false,
    sandbox:
      "allow-scripts allow-top-navigation allow-top-navigation-to-custom-protocols",
  });

  await runExtProtocolSandboxTest({
    blocked: true,
    sandbox: "allow-scripts allow-modals",
  });
});

add_task(async function test_sandbox_iframe_redirect() {
  await runExtProtocolSandboxTest({
    blocked: true,
    sandbox: "allow-scripts",
    triggerMethod: "frameSrcRedirect",
  });

  await runExtProtocolSandboxTest({
    blocked: false,
    sandbox: "allow-scripts allow-top-navigation-to-custom-protocols",
    triggerMethod: "frameSrcRedirect",
  });
});
