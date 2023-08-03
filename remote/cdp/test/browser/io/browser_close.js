/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function fileRemovedAfterClose({ client }) {
  const { IO } = client;
  const contents = "Lorem ipsum";
  const { handle, stream } = await registerFileStream(contents);

  await IO.close({ handle });
  ok(
    !(await IOUtils.exists(stream.path)),
    "Discarded the temporary backing storage"
  );
});

add_task(async function unknownHandle({ client }) {
  const { IO } = client;
  const handle = "1000000";

  await Assert.rejects(
    IO.close({ handle }),
    err => err.message.includes(`Invalid stream handle`),
    "Error contains expected message"
  );
});

add_task(async function invalidHandleTypes({ client }) {
  const { IO } = client;
  for (const handle of [null, true, 1, [], {}]) {
    await Assert.rejects(
      IO.close({ handle }),
      err => err.message.includes(`handle: string value expected`),
      "Error contains expected message"
    );
  }
});
