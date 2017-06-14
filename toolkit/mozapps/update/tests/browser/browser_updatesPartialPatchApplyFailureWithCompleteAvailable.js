add_task(async function testPartialPatchApplyFailureWithCompleteAvailable() {
  let patchProps = {type: "partial",
                    state: STATE_PENDING};
  let patches = getLocalPatchString(patchProps);
  patchProps = {selected: "false"};
  patches += getLocalPatchString(patchProps);
  let updateProps = {isCompleteUpdate: "false",
                     promptWaitTime: "0"};
  let updates = getLocalUpdateString(updateProps, patches);

  await runUpdateProcessingTest(updates, [
    {
      notificationId: "update-restart",
      button: "secondarybutton",
      cleanup() {
        AppMenuNotifications.removeNotification(/.*/);
      }
    },
  ]);
});
