/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function fileRemovedAfterClose({ IO }) {
  const contents = "Lorem ipsum";
  const { handle, path } = await registerFileStream(contents);

  await IO.close({ handle });
  ok(!(await OS.File.exists(path)), "Discarded the temporary backing storage");
});

add_task(async function unknownHandle({ IO }) {
  const handle = "1000000";

  try {
    await IO.close({ handle });
    ok(false, "Close shouldn't pass");
  } catch (e) {
    ok(
      e.message.startsWith(`Invalid stream handle`),
      "Error contains expected message"
    );
  }
});

add_task(async function invalidHandleTypes({ IO }) {
  for (const handle of [null, true, 1, [], {}]) {
    try {
      await IO.close({ handle });
      ok(false, "Close shouldn't pass");
    } catch (e) {
      ok(
        e.message.startsWith(`handle: string value expected`),
        "Error contains expected message"
      );
    }
  }
});
