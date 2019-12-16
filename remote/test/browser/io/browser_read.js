/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function seekByOffsets({ IO }) {
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

add_task(async function remembersOffsetAfterRead({ IO }) {
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

add_task(async function readBySize({ IO }) {
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

add_task(async function unknownHandle({ IO }) {
  const handle = "1000000";

  try {
    await IO.read({ handle });
    ok(false, "Read shouldn't pass");
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
      await IO.read({ handle });
      ok(false, "Read shouldn't pass");
    } catch (e) {
      ok(
        e.message.startsWith(`handle: string value expected`),
        "Error contains expected message"
      );
    }
  }
});

add_task(async function invalidOffsetTypes({ IO }) {
  const contents = "Lorem ipsum";
  const { handle } = await registerFileStream(contents);

  for (const offset of [null, true, "1", [], {}]) {
    try {
      await IO.read({ handle, offset });
      ok(false, "Read shouldn't pass");
    } catch (e) {
      ok(
        e.message.startsWith(`offset: integer value expected`),
        "Error contains expected message"
      );
    }
  }
});

add_task(async function invalidSizeTypes({ IO }) {
  const contents = "Lorem ipsum";
  const { handle } = await registerFileStream(contents);

  for (const size of [null, true, "1", [], {}]) {
    try {
      await IO.read({ handle, size });
      ok(false, "Read shouldn't pass");
    } catch (e) {
      ok(
        e.message.startsWith(`size: integer value expected`),
        "Error contains expected message"
      );
    }
  }
});
