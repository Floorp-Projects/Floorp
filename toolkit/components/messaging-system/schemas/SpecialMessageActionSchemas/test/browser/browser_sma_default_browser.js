/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_set_default_browser() {
  const sandbox = sinon.createSandbox();
  const stub = sandbox.stub();

  await SMATestUtils.executeAndValidateAction(
    { type: "SET_DEFAULT_BROWSER" },
    {
      ownerGlobal: {
        getShellService: () => ({
          setAsDefault: stub,
        }),
      },
    }
  );

  Assert.equal(stub.callCount, 1, "setAsDefault was called by the action");
});
