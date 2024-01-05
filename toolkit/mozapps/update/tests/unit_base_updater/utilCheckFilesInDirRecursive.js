/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

add_task(async function test_checkFilesInDirRecursive() {
  let root = do_get_tempdir().path;

  await IOUtils.makeDirectory(PathUtils.join(root, "A", "B", "C"));
  await IOUtils.makeDirectory(PathUtils.join(root, "A", "B", "EMPTY"));

  await IOUtils.writeUTF8(PathUtils.join(root, "A", "a.txt"), "a.txt");
  await IOUtils.writeUTF8(PathUtils.join(root, "A", "B", "b.txt"), "b.txt");
  await IOUtils.writeUTF8(
    PathUtils.join(root, "A", "B", "C", "c.txt"),
    "c.txt"
  );

  let l = [];

  // Turn `/abs/root/path/leaf` into `/path/leaf`.
  let relativize = p => p.split(root)[1];
  checkFilesInDirRecursive(do_get_tempdir(), f => l.push(relativize(f.path)));

  Assert.deepEqual(
    Array.from(new Set(l)).toSorted(),
    ["/A/B/C/c.txt", "/A/B/b.txt", "/A/a.txt"],
    "iterated expected file names"
  );
  Assert.equal(l.length, 3, "iterated expected number of directory entries");

  // Now verify that directories are iterated after files.  This is
  // tricky to check because there's no order of directory entries
  // (and we don't want to add one to the utility function).
  l = [];
  checkFilesInDirRecursive(do_get_tempdir(), f => l.push(relativize(f.path)), {
    includeDirectories: true,
  });

  Assert.deepEqual(
    Array.from(new Set(l)).toSorted(),
    [
      "",
      "/A",
      "/A/B",
      "/A/B/C",
      "/A/B/C/c.txt",
      "/A/B/EMPTY",
      "/A/B/b.txt",
      "/A/a.txt",
    ],
    "iterated expected entry names"
  );
  Assert.equal(l.length, 8, "iterated expected number of directory entries");

  // This is awkward but I don't see a better way to accommodate the
  // non-deterministic directory entries.
  Assert.ok(l.indexOf("") > l.indexOf("/A"));
  Assert.ok(l.indexOf("/A") > l.indexOf("/A/a.txt"));
  Assert.ok(l.indexOf("/A") > l.indexOf("/A/B"));
  Assert.ok(l.indexOf("/A/B") > l.indexOf("/A/B/b.txt"));
  Assert.ok(l.indexOf("/A/B") > l.indexOf("/A/B/C"));
  Assert.ok(l.indexOf("/A/B/C") > l.indexOf("/A/B/C/c.txt"));
  Assert.ok(l.indexOf("/A/B") > l.indexOf("/A/B/EMPTY"));
});
