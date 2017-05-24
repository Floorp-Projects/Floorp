add_task(async function testCompleteAndPartialPatchesWithBadCompleteSize() {
  let updateParams = "invalidCompleteSize=1&promptWaitTime=0";

  await runUpdateTest(updateParams, 1, [
    {
      notificationId: "update-restart",
      button: "secondarybutton",
      cleanup() {
        AppMenuNotifications.removeNotification(/.*/);
      }
    },
  ]);
});
