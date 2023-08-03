/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function ({ CDP }) {
  const { webSocketDebuggerUrl } = await CDP.Version();
  const client = await CDP({ target: webSocketDebuggerUrl });

  await Assert.rejects(
    client.send("Hoobaflooba"),
    /Invalid method format/,
    `Fails with invalid method format`
  );

  await Assert.rejects(
    client.send("Hooba.flooba"),
    /UnknownMethodError/,
    `Fails with UnknownMethodError`
  );

  await client.close();
});
