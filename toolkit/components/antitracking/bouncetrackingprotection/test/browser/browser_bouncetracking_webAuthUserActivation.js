/* Any copyright is dedicated to the Public Domain.
   https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Import helpers from webauthn tests.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/dom/webauthn/tests/browser/head.js",
  this
);

// Variant of promiseWebAuthnGetAssertionDiscoverable from head.js which can
// target a BrowsingContext. This is needed for running the test code in an
// iframe.
function promiseWebAuthnGetAssertionDiscoverableBC(
  target,
  mediation = "optional",
  extensions = {}
) {
  return SpecialPowers.spawn(
    target,
    [extensions, mediation],
    async (extensions, mediation) => {
      let challenge = content.crypto.getRandomValues(new Uint8Array(16));

      let publicKey = {
        challenge,
        extensions,
        rpId: content.document.domain,
        allowCredentials: [],
      };

      // Can't return the promise directly, that results in a structured clone
      // error. That's fine because this test doesn't care about the result. It
      // only cares whether the request was successful or whether any error was
      // thrown.
      await content.navigator.credentials.get({ publicKey, mediation });
    }
  );
}

/**
 * This file tests that webauthn request triggers BTP user activation.
 */

/**
 * Test that a webauthn request triggers BTP user activation.
 * @param {Function} triggerFn - Function that runs the webauthn request.
 * @param {String} userActivationSiteHost - The expected user activation site host in BTP.
 */
async function runWebAuthTest(triggerFn, userActivationSiteHost) {
  is(
    bounceTrackingProtection.testGetUserActivationHosts({}).length,
    0,
    "No user activation hosts initially."
  );

  await triggerFn();

  let userActivationHosts = bounceTrackingProtection.testGetUserActivationHosts(
    {}
  );
  is(
    userActivationHosts.length,
    1,
    "One user activation host after webauth request."
  );
  is(
    userActivationHosts[0].siteHost,
    userActivationSiteHost,
    `User activation host is ${userActivationSiteHost}.`
  );

  bounceTrackingProtection.clearAll();
}

/**
 * Wrapper around runWebAuthTest for the case where the webauthn request is made
 * in an iframe.
 * @param {string} topLevelSite - The top level site where the iframe is inserted.
 * @param {string} iframeSite - The site of the iframe.
 */
async function runWebAuthTestIframe(topLevelSite, iframeSite) {
  await runWebAuthTest(() => {
    return BrowserTestUtils.withNewTab(
      `https://${topLevelSite}`,
      async browser => {
        info(
          `Inserting an iframe with target ${iframeSite} under top level ${topLevelSite}.`
        );
        let iframeBC = await SpecialPowers.spawn(
          browser,
          [iframeSite],
          async iframeSite => {
            let iframe = content.document.createElement("iframe");
            let loadedPromise = ContentTaskUtils.waitForEvent(iframe, "load");
            iframe.src = `https://${iframeSite}`;
            iframe.allow = "publickey-credentials-get *";
            content.document.body.appendChild(iframe);
            await loadedPromise;

            return iframe.browsingContext;
          }
        );

        info(
          "Request assertion. Using the virtual authenticator means we do not get a prompt."
        );

        await promiseWebAuthnGetAssertionDiscoverableBC(iframeBC).catch(
          arrivingHereIsBad
        );
      }
    );
  }, topLevelSite);
}

add_setup(async function () {
  bounceTrackingProtection.clearAll();

  await SpecialPowers.pushPrefEnv({
    set: [
      ["security.webauth.webauthn_enable_softtoken", true],
      ["security.webauth.webauthn_enable_usbtoken", false],
    ],
  });
  let authenticatorId = add_virtual_authenticator();

  await addCredential(authenticatorId, "example.com");
  await addCredential(authenticatorId, "example.org");
});

add_task(async function test_web_auth_triggers_btp_user_activation() {
  await runWebAuthTest(() => {
    return BrowserTestUtils.withNewTab("https://example.com", async browser => {
      info(
        "Request assertion. Using the virtual authenticator means we do not get a prompt."
      );
      await promiseWebAuthnGetAssertionDiscoverable(
        gBrowser.getTabForBrowser(browser)
      ).catch(arrivingHereIsBad);
    });
  }, "example.com");
});

add_task(
  async function test_web_auth_triggers_btp_user_activation_iframe_cross_site() {
    await runWebAuthTestIframe("example.org", "example.com");
  }
);

add_task(
  async function test_web_auth_triggers_btp_user_activation_iframe_same_site() {
    await runWebAuthTestIframe("example.org", "example.org");
  }
);
