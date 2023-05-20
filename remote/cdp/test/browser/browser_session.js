/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function ({ CDP }) {
  const { webSocketDebuggerUrl } = await CDP.Version();
  const client = await CDP({ target: webSocketDebuggerUrl });

  try {
    await client.send("Hoobaflooba");
  } catch (e) {
    ok(e.message.match(/Invalid method format/));
  }
  try {
    await client.send("Hooba.flooba");
  } catch (e) {
    ok(e.message.match(/UnknownMethodError/));
  }

  await client.close();
});
