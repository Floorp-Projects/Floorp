/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const { StreamRegistry } = ChromeUtils.import(
  "chrome://remote/content/cdp/StreamRegistry.jsm"
);

add_test(function test_constructor() {
  const registry = new StreamRegistry();
  equal(registry.streams.size, 0);

  run_next_test();
});

add_test(async function test_destructor() {
  const registry = new StreamRegistry();
  const { file: file1, path: path1 } = await createFile("foo bar");
  const { file: file2, path: path2 } = await createFile("foo bar");

  registry.add(file1);
  registry.add(file2);

  equal(registry.streams.size, 2);

  await registry.destructor();
  equal(registry.streams.size, 0);
  ok(!(await OS.File.exists(path1)), "temporary file has been removed");
  ok(!(await OS.File.exists(path2)), "temporary file has been removed");

  run_next_test();
});

add_test(async function test_addValidStreamType() {
  const registry = new StreamRegistry();
  const { file } = await createFile("foo bar");

  const handle = registry.add(file);
  equal(registry.streams.size, 1, "A single stream has been added");
  equal(typeof handle, "string", "Handle is of type string");
  ok(registry.streams.has(handle), "Handle has been found");
  equal(registry.streams.get(handle), file, "Expected OS.File stream found");

  run_next_test();
});

add_test(async function test_addCreatesDifferentHandles() {
  const registry = new StreamRegistry();
  const { file } = await createFile("foo bar");

  const handle1 = registry.add(file);
  equal(registry.streams.size, 1, "A single stream has been added");
  equal(typeof handle1, "string", "Handle is of type string");
  ok(registry.streams.has(handle1), "Handle has been found");
  equal(registry.streams.get(handle1), file, "Expected OS.File stream found");

  const handle2 = registry.add(file);
  equal(registry.streams.size, 2, "A single stream has been added");
  equal(typeof handle2, "string", "Handle is of type string");
  ok(registry.streams.has(handle2), "Handle has been found");
  equal(registry.streams.get(handle2), file, "Expected OS.File stream found");

  notEqual(handle1, handle2, "Different handles have been generated");

  run_next_test();
});

add_test(async function test_addInvalidStreamType() {
  const registry = new StreamRegistry();
  Assert.throws(() => registry.add(new Blob([])), /UnsupportedError/);

  run_next_test();
});

add_test(async function test_getForValidHandle() {
  const registry = new StreamRegistry();
  const { file } = await createFile("foo bar");
  const handle = registry.add(file);

  equal(registry.streams.size, 1, "A single stream has been added");
  equal(registry.get(handle), file, "Expected OS.File stream found");

  run_next_test();
});

add_test(async function test_getForInvalidHandle() {
  const registry = new StreamRegistry();
  const { file } = await createFile("foo bar");
  registry.add(file);

  equal(registry.streams.size, 1, "A single stream has been added");
  Assert.throws(() => registry.get("foo"), /TypeError/);

  run_next_test();
});

add_test(async function test_removeForValidHandle() {
  const registry = new StreamRegistry();
  const { file: file1, path: path1 } = await createFile("foo bar");
  const { file: file2, path: path2 } = await createFile("foo bar");

  const handle1 = registry.add(file1);
  const handle2 = registry.add(file2);

  equal(registry.streams.size, 2);

  await registry.remove(handle1);
  equal(registry.streams.size, 1);
  ok(
    !(await OS.File.exists(path1)),
    "temporary file for first stream has been removed"
  );
  equal(registry.get(handle2), file2, "Second stream has not been closed");
  ok(
    await OS.File.exists(path2),
    "temporary file for second stream hasn't been removed"
  );

  run_next_test();
});

add_test(async function test_removeForInvalidHandle() {
  const registry = new StreamRegistry();
  const { file } = await createFile("foo bar");
  registry.add(file);

  equal(registry.streams.size, 1, "A single stream has been added");
  await Assert.rejects(registry.remove("foo"), /TypeError/);

  run_next_test();
});

async function createFile(contents, options = {}) {
  let { path = null, remove = true } = options;

  if (!path) {
    const basePath = OS.Path.join(OS.Constants.Path.tmpDir, "remote-agent.txt");
    const { file, path: tmpPath } = await OS.File.openUnique(basePath, {
      humanReadable: true,
    });
    await file.close();
    path = tmpPath;
  }

  let encoder = new TextEncoder();
  let array = encoder.encode(contents);

  const count = await OS.File.writeAtomic(path, array, {
    encoding: "utf-8",
    tmpPath: path + ".tmp",
  });
  equal(count, contents.length, "All data has been written to file");

  const file = await OS.File.open(path);

  // Automatically remove the file once the test has finished
  if (remove) {
    registerCleanupFunction(async () => {
      await file.close();
      await OS.File.remove(path, { ignoreAbsent: true });
    });
  }

  return { file, path };
}
