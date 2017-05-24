add_task(async function testCompleteAndPartialPatchesWithBadPartialSize() {
  let updateParams = "invalidPartialSize=1&promptWaitTime=0";

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
