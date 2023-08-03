/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function seekByOffsets({ client }) {
  const { IO } = client;
  const contents = "Lorem ipsum";
  const { handle } = await registerFileStream(contents);

  for (const offset of [0, 5, 10, 100, 0, -1]) {
    const result = await IO.read({ handle, offset });
    ok(result.base64Encoded, `Data for offset ${offset} is base64 encoded`);
    ok(result.eof, `All data has been read for offset ${offset}`);
    is(
      atob(result.data),
      contents.substring(offset >= 0 ? offset : 0),
      `Found expected data for offset ${offset}`
    );
  }
});

add_task(async function remembersOffsetAfterRead({ client }) {
  const { IO } = client;
  const contents = "Lorem ipsum";
  const { handle } = await registerFileStream(contents);

  let expectedOffset = 0;
  const size = 3;
  do {
    const result = await IO.read({ handle, size });
    is(
      atob(result.data),
      contents.substring(expectedOffset, expectedOffset + size),
      `Found expected data for expectedOffset ${expectedOffset}`
    );
    ok(
      result.base64Encoded,
      `Data up to expected offset ${expectedOffset} is base64 encoded`
    );

    is(
      result.eof,
      expectedOffset + size >= contents.length,
      `All data has been read up to expected offset ${expectedOffset}`
    );

    expectedOffset = Math.min(expectedOffset + size, contents.length);
  } while (expectedOffset < contents.length);
});

add_task(async function readBySize({ client }) {
  const { IO } = client;
  const contents = "Lorem ipsum";
  const { handle } = await registerFileStream(contents);

  for (const size of [0, 5, 10, 100, 0, -1]) {
    const result = await IO.read({ handle, offset: 0, size });
    ok(result.base64Encoded, `Data for size ${size} is base64 encoded`);
    is(
      result.eof,
      size >= contents.length,
      `All data has been read for size ${size}`
    );
    is(
      atob(result.data),
      contents.substring(0, size),
      `Found expected data for size ${size}`
    );
  }
});

add_task(async function readAfterClose({ client }) {
  const { IO } = client;
  const contents = "Lorem ipsum";

  // If we omit remove: false, then by the time the registered cleanup function
  // runs we will have deleted our temp file (in the following call to IO.close)
  // *but* another test will have created a file with the same name (due to the
  // way IOUtils.createUniqueFile works). That file's stream will not be closed
  // and so we won't be able to delete it, resulting in an exception and
  // therefore a test failure.
  const { handle, stream } = await registerFileStream(contents, {
    remove: false,
  });

  await IO.close({ handle });

  ok(!(await IOUtils.exists(stream.path)), "File should no longer exist");

  await Assert.rejects(
    IO.read({ handle }),
    err => err.message.includes(`Invalid stream handle`),
    "Error contains expected message"
  );
});

add_task(async function unknownHandle({ client }) {
  const { IO } = client;
  const handle = "1000000";

  await Assert.rejects(
    IO.read({ handle }),
    err => err.message.includes(`Invalid stream handle`),
    "Error contains expected message"
  );
});

add_task(async function invalidHandleTypes({ client }) {
  const { IO } = client;
  for (const handle of [null, true, 1, [], {}]) {
    await Assert.rejects(
      IO.read({ handle }),
      err => err.message.includes(`handle: string value expected`),
      "Error contains expected message"
    );
  }
});

add_task(async function invalidOffsetTypes({ client }) {
  const { IO } = client;
  const contents = "Lorem ipsum";
  const { handle } = await registerFileStream(contents);

  for (const offset of [null, true, "1", [], {}]) {
    await Assert.rejects(
      IO.read({ handle, offset }),
      err => err.message.includes(`offset: integer value expected`),
      "Error contains expected message"
    );
  }
});

add_task(async function invalidSizeTypes({ client }) {
  const { IO } = client;
  const contents = "Lorem ipsum";
  const { handle } = await registerFileStream(contents);

  for (const size of [null, true, "1", [], {}]) {
    await Assert.rejects(
      IO.read({ handle, size }),
      err => err.message.includes(`size: integer value expected`),
      "Error contains expected message"
    );
  }
});
