/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

"use strict";

let certDialogShown = false;
function onCertDialogLoaded(subject) {
  certDialogShown = true;
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  setTimeout(() => {
    subject.acceptDialog();
  }, 0);
}

Services.obs.addObserver(onCertDialogLoaded, "cert-dialog-loaded");

function clearClientCertsDecision() {
  let cars = Cc["@mozilla.org/security/clientAuthRememberService;1"].getService(
    Ci.nsIClientAuthRememberService
  );
  cars.clearRememberedDecisions();
}

registerCleanupFunction(() => {
  Services.obs.removeObserver(onCertDialogLoaded, "cert-dialog-loaded");
  // Make sure we don't affect other tests.
  clearClientCertsDecision();
});

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["security.default_personal_cert", "Ask Every Time"]],
  });

  clearClientCertsDecision();

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["<all_urls>"],
    },

    async background() {
      try {
        await fetch("https://requireclientcert.example.com/");
        browser.test.notifyPass("cert_dialog_shown");
      } catch (error) {
        browser.test.fail(`${error} :: ${error.stack}`);
        browser.test.notifyFail("cert_dialog_shown");
      }
    },
  });

  await extension.startup();
  await extension.awaitFinish("cert_dialog_shown");
  await extension.unload();
  ok(certDialogShown, "Cert dialog was shown");
});
