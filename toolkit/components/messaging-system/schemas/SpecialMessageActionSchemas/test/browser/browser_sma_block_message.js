/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_block_message() {
  let blockStub = sinon.stub(SpecialMessageActions, "blockMessageById");

  await SMATestUtils.executeAndValidateAction({
    type: "BLOCK_MESSAGE",
    data: {
      id: "TEST_MESSAGE_ID",
    },
  });

  Assert.equal(blockStub.callCount, 1, "blockMessageById called by the action");
  Assert.equal(
    blockStub.firstCall.args[0],
    "TEST_MESSAGE_ID",
    "Argument is message id"
  );
  blockStub.restore();
});
