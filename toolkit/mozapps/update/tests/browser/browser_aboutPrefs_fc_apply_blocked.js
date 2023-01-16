/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PromptTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromptTestUtils.sys.mjs"
);

const BUILDER_URL = "https://example.com/document-builder.sjs?html=";
const PAGE_MARKUP = `
<html>
<head>
  <script>
    window.onbeforeunload = function() {
      return true;
    };
  </script>
</head>
<body>TEST PAGE</body>
</html>
`;
const TEST_URL = BUILDER_URL + encodeURI(PAGE_MARKUP);

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.require_user_interaction_for_beforeunload", false]],
  });
});

// Test for About Dialog foreground check for updates
// and apply but restart is blocked by a page.
add_task(async function aboutDialog_foregroundCheck_apply_blocked() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);

  let downloadInfo = [];
  if (Services.prefs.getBoolPref(PREF_APP_UPDATE_BITS_ENABLED)) {
    downloadInfo[0] = { patchType: "partial", bitsResult: "0" };
  } else {
    downloadInfo[0] = { patchType: "partial", internalResult: "0" };
  }

  let prefsTab;
  let handlePromise = (async () => {
    let dialog = await PromptTestUtils.waitForPrompt(window, {
      modalType: Ci.nsIPrompt.MODAL_TYPE_CONTENT,
      promptType: "confirmEx",
    });
    await SpecialPowers.spawn(prefsTab.linkedBrowser, [], async () => {
      Assert.equal(
        content.gAppUpdater.selectedPanel.id,
        "restarting",
        "The restarting panel should be displayed"
      );
    });

    await PromptTestUtils.handlePrompt(dialog, { buttonNumClick: 1 });
  })();

  // Since the partial should be successful specify an invalid size for the
  // complete update.
  let params = { queryString: "&invalidCompleteSize=1&promptWaitTime=0" };
  await runAboutPrefsUpdateTest(params, [
    {
      panelId: "checkingForUpdates",
      checkActiveUpdate: null,
      continueFile: CONTINUE_CHECK,
    },
    {
      panelId: "downloading",
      checkActiveUpdate: { state: STATE_DOWNLOADING },
      continueFile: CONTINUE_DOWNLOAD,
      downloadInfo,
    },
    async function getPrefsTab(tab) {
      prefsTab = tab;
    },
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: null,
      forceApply: true,
    },
    async function ensureDialogHasBeenCanceled() {
      await handlePromise;
    },
    // A final check to ensure that we are back in the apply state.
    {
      panelId: "apply",
      checkActiveUpdate: { state: STATE_PENDING },
      continueFile: null,
    },
  ]);

  BrowserTestUtils.removeTab(tab, { skipPermitUnload: true });
});
