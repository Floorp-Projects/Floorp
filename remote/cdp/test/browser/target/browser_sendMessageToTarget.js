/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function sendToAttachedTarget({ client }) {
  const { Target } = client;
  const { targetInfo } = await openTab(Target);

  const attachedToTarget = Target.attachedToTarget();
  const { sessionId } = await Target.attachToTarget({
    targetId: targetInfo.targetId,
  });
  await attachedToTarget;
  info("Target attached");

  const id = 1;
  const message = JSON.stringify({
    id,
    method: "Page.navigate",
    params: {
      url: toDataURL("new-page"),
    },
  });

  info("Calling Target.sendMessageToTarget");
  const onResponse = Target.receivedMessageFromTarget();
  await Target.sendMessageToTarget({ sessionId, message });
  const response = await onResponse;
  info("Message from target received");

  ok(!!response, "The response is not empty");
  is(response.sessionId, sessionId, "The response is from the same session");

  const responseMessage = JSON.parse(response.message);
  is(responseMessage.id, id, "The response is from the same session");
  ok(
    !!responseMessage.result.frameId,
    "received the `frameId` out of `Page.navigate` request"
  );
});
