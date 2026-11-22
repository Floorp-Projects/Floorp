// SPDX-License-Identifier: MPL-2.0

import {
  assert,
  assertEquals,
  assertNotEquals,
  assertRejects,
} from "@std/assert";
import * as path from "@std/path";
import {
  hashDirectoryContents,
  isHubDocumentUrl,
  withExactSourceRestoration,
} from "./hub_menu_acceptance.ts";

Deno.test("HUB URL matching allows only an exact about:hub base", () => {
  assert(isHubDocumentUrl("about:hub"));
  assert(isHubDocumentUrl("about:hub#/overview/home"));
  assert(!isHubDocumentUrl("about:hub-extra"));
  assert(!isHubDocumentUrl("about:hub/path"));
  assert(!isHubDocumentUrl("about:hub?query"));
});

Deno.test("directory content hash covers empty, file, and missing paths", async () => {
  const root = await Deno.makeTempDir();
  try {
    const empty = await hashDirectoryContents(root);
    assertEquals(empty, await hashDirectoryContents(root));
    await Deno.writeTextFile(path.join(root, "state.json"), "first");
    const first = await hashDirectoryContents(root);
    await Deno.writeTextFile(path.join(root, "state.json"), "second");
    assertNotEquals(first, await hashDirectoryContents(root));
  } finally {
    await Deno.remove(root, { recursive: true });
  }
  await assertRejects(() => hashDirectoryContents(`${root}-missing`));
});

Deno.test("source mutation restores exact bytes after success", async () => {
  const sourcePath = await Deno.makeTempFile();
  const original = new TextEncoder().encode("before\r\n");
  await Deno.writeFile(sourcePath, original);
  try {
    let afterRestoreCalled = false;
    const result = await withExactSourceRestoration(
      sourcePath,
      (source) => source.replace("before", "during"),
      async () => {
        assertEquals(await Deno.readTextFile(sourcePath), "during\r\n");
        return "done";
      },
      () => {
        afterRestoreCalled = true;
        return Promise.resolve();
      },
    );
    assertEquals(result, "done");
    assert(afterRestoreCalled);
    assertEquals(await Deno.readFile(sourcePath), original);
  } finally {
    await Deno.remove(sourcePath);
  }
});

Deno.test("source mutation preserves operation and restoration errors", async () => {
  const sourcePath = await Deno.makeTempFile();
  const original = new TextEncoder().encode("original");
  const operationError = new Error("mutation failed");
  const restorationError = new Error("restoration observer failed");
  try {
    await Deno.writeFile(sourcePath, original);
    const thrown = await assertRejects(
      () =>
        withExactSourceRestoration(
          sourcePath,
          (source) => `${source}-changed`,
          () => Promise.reject(operationError),
          () => Promise.reject(restorationError),
        ),
      AggregateError,
    );
    assert(thrown.errors[0] === operationError);
    assert(thrown.errors[1] === restorationError);
    assertEquals(await Deno.readFile(sourcePath), original);
  } finally {
    await Deno.remove(sourcePath);
  }
});

Deno.test("source mutation restores exact bytes after operation failure", async () => {
  const sourcePath = await Deno.makeTempFile();
  const original = new TextEncoder().encode("original");
  await Deno.writeFile(sourcePath, original);
  const operationError = new Error("mutation probe failed");
  try {
    const thrown = await assertRejects(
      () =>
        withExactSourceRestoration(
          sourcePath,
          (source) => `${source}-changed`,
          () => Promise.reject(operationError),
          async () => {},
        ),
      Error,
      "mutation probe failed",
    );
    assert(thrown === operationError);
    assertEquals(await Deno.readFile(sourcePath), original);
  } finally {
    await Deno.remove(sourcePath);
  }
});
