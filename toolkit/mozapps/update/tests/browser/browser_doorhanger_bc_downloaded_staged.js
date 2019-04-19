add_task(async function testCompleteAndPartialPatchesWithBadCompleteSize() {
  SpecialPowers.pushPrefEnv({set: [
    [PREF_APP_UPDATE_STAGING_ENABLED, true],
  ]});

  let updateParams = "invalidCompleteSize=1&promptWaitTime=0";

  await runUpdateTest(updateParams, 1, [
    {
      notificationId: "update-restart",
      button: "secondaryButton",
      cleanup() {
        AppMenuNotifications.removeNotification(/.*/);
      },
    },
  ]);
});
