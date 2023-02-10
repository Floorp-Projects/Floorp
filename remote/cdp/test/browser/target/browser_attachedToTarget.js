/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const PAGE_TEST =
  "https://example.com/browser/remote/cdp/test/browser/target/doc_test.html";

add_task(
  async function attachedPageTarget({ client }) {
    const { Target } = client;
    const { targetInfo } = await openTab(Target);

    ok(
      !targetInfo.attached,
      "Got expected target attached status before attaching"
    );

    await loadURL(PAGE_TEST);

    info("Attach new page target");
    const attachedToTarget = Target.attachedToTarget();
    const { sessionId } = await Target.attachToTarget({
      targetId: targetInfo.targetId,
    });
    const {
      targetInfo: eventTargetInfo,
      sessionId: eventSessionId,
      waitingForDebugger: eventWaitingForDebugger,
    } = await attachedToTarget;

    is(eventTargetInfo.targetId, targetInfo.targetId, "Got expected target id");
    is(eventTargetInfo.type, "page", "Got expected target type");
    is(eventTargetInfo.title, "Test Page", "Got expected target title");
    is(eventTargetInfo.url, PAGE_TEST, "Got expected target URL");
    ok(eventTargetInfo.attached, "Got expected target attached status");

    is(
      eventSessionId,
      sessionId,
      "attachedToTarget and attachToTarget refer to the same session id"
    );
    is(
      typeof eventWaitingForDebugger,
      "boolean",
      "Got expected type for waitingForDebugger"
    );
  },
  { createTab: false }
);
