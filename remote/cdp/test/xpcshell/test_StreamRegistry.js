/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { Stream, StreamRegistry } = ChromeUtils.importESModule(
  "chrome://remote/content/cdp/StreamRegistry.sys.mjs"
);

add_task(function test_constructor() {
  const registry = new StreamRegistry();
  equal(registry.streams.size, 0);
});

add_task(async function test_destructor() {
  const registry = new StreamRegistry();

  const stream1 = await createFileStream("foo bar");
  const stream2 = await createFileStream("foo bar");

  registry.add(stream1);
  registry.add(stream2);

  equal(registry.streams.size, 2);

  await registry.destructor();
  equal(registry.streams.size, 0);

  ok(!(await IOUtils.exists(stream1.path)), "temporary file has been removed");
  ok(!(await IOUtils.exists(stream2.path)), "temporary file has been removed");
});

add_task(async function test_addValidStreamType() {
  const registry = new StreamRegistry();

  const stream = await createFileStream("foo bar");
  const handle = registry.add(stream);

  equal(registry.streams.size, 1, "A single stream has been added");
  equal(typeof handle, "string", "Handle is of type string");
  ok(registry.streams.has(handle), "Handle has been found");

  const rv = registry.streams.get(handle);
  equal(rv, stream, "Expected stream found");
});

add_task(async function test_addCreatesDifferentHandles() {
  const registry = new StreamRegistry();
  const stream = await createFileStream("foo bar");

  const handle1 = registry.add(stream);
  equal(registry.streams.size, 1, "A single stream has been added");
  equal(typeof handle1, "string", "Handle is of type string");
  ok(registry.streams.has(handle1), "Handle has been found");
  equal(registry.streams.get(handle1), stream, "Expected stream found");

  const handle2 = registry.add(stream);
  equal(registry.streams.size, 2, "A single stream has been added");
  equal(typeof handle2, "string", "Handle is of type string");
  ok(registry.streams.has(handle2), "Handle has been found");
  equal(registry.streams.get(handle2), stream, "Expected stream found");

  notEqual(handle1, handle2, "Different handles have been generated");
});

add_task(async function test_addInvalidStreamType() {
  const registry = new StreamRegistry();
  Assert.throws(() => registry.add(new Blob([])), /UnsupportedError/);
});

add_task(async function test_getForValidHandle() {
  const registry = new StreamRegistry();
  const stream = await createFileStream("foo bar");
  const handle = registry.add(stream);

  equal(registry.streams.size, 1, "A single stream has been added");
  equal(registry.get(handle), stream, "Expected stream found");
});

add_task(async function test_getForInvalidHandle() {
  const registry = new StreamRegistry();
  const stream = await createFileStream("foo bar");
  registry.add(stream);

  equal(registry.streams.size, 1, "A single stream has been added");
  Assert.throws(() => registry.get("foo"), /TypeError/);
});

add_task(async function test_removeForValidHandle() {
  const registry = new StreamRegistry();
  const stream1 = await createFileStream("foo bar");
  const stream2 = await createFileStream("foo bar");

  const handle1 = registry.add(stream1);
  const handle2 = registry.add(stream2);

  equal(registry.streams.size, 2);

  await registry.remove(handle1);
  equal(registry.streams.size, 1);
  equal(registry.get(handle2), stream2, "Second stream has not been closed");

  ok(
    !(await IOUtils.exists(stream1.path)),
    "temporary file for first stream is removed"
  );
  ok(
    await IOUtils.exists(stream2.path),
    "temporary file for second stream is not removed"
  );
});

add_task(async function test_removeForInvalidHandle() {
  const registry = new StreamRegistry();
  const stream = await createFileStream("foo bar");
  registry.add(stream);

  equal(registry.streams.size, 1, "A single stream has been added");
  await Assert.rejects(registry.remove("foo"), /TypeError/);
});

/**
 * Create a stream with the specified contents.
 *
 * @param {string} contents
 *     Contents of the file.
 * @param {object} options
 * @param {string=} options.path
 *     Path of the file. Defaults to the temporary directory.
 * @param {boolean=} options.remove
 *     If true, automatically remove the file after the test. Defaults to true.
 *
 * @returns {Promise<Stream>}
 */
async function createFileStream(contents, options = {}) {
  let { path = null, remove = true } = options;

  if (!path) {
    path = await IOUtils.createUniqueFile(
      PathUtils.tempDir,
      "remote-agent.txt"
    );
  }

  await IOUtils.writeUTF8(path, contents);

  const stream = new Stream(path);
  if (remove) {
    registerCleanupFunction(() => stream.destroy());
  }

  return stream;
}
