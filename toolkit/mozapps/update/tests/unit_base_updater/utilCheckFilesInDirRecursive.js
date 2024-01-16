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

  // Turn `/abs/root/path/leaf` into `/path/leaf`; normalize '\' to '/'.
  let relativize = p => p.split(root)[1].replace(/\\/g, "/");
  checkFilesInDirRecursive(do_get_tempdir(), f => l.push(relativize(f.path)));

  Assert.deepEqual(
    l,
    ["/A/a.txt", "/A/B/b.txt", "/A/B/C/c.txt"],
    "iterated expected file names in expected order"
  );

  // Verify that directories are iterated after files.
  l = [];
  checkFilesInDirRecursive(do_get_tempdir(), f => l.push(relativize(f.path)), {
    includeDirectories: true,
  });

  Assert.deepEqual(
    l,
    [
      "/A/a.txt",
      "/A/B/b.txt",
      "/A/B/EMPTY",
      "/A/B/C/c.txt",
      "/A/B/C",
      "/A/B",
      "/A",
      "",
    ],
    "iterated expected entry names"
  );
});
