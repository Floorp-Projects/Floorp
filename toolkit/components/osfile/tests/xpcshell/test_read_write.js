/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var SHARED_PATH;

var EXISTING_FILE = do_get_file("xpcshell.ini").path;

add_task(async function init() {
  do_get_profile();
  SHARED_PATH = OS.Path.join(
    OS.Constants.Path.profileDir,
    "test_osfile_read.tmp"
  );
});

// Check that OS.File.read() is executed after the previous operation
add_test_pair(async function ordering() {
  let string1 = "Initial state " + Math.random();
  let string2 = "After writing " + Math.random();
  await OS.File.writeAtomic(SHARED_PATH, string1);
  OS.File.writeAtomic(SHARED_PATH, string2);
  let string3 = await OS.File.read(SHARED_PATH, { encoding: "utf-8" });
  Assert.equal(string3, string2);
});

add_test_pair(async function read_write_all() {
  let DEST_PATH = SHARED_PATH + Math.random();
  let TMP_PATH = DEST_PATH + ".tmp";

  let test_with_options = function(options, suffix) {
    return (async function() {
      info(
        "Running test read_write_all with options " + JSON.stringify(options)
      );
      let TEST = "read_write_all " + suffix;

      let optionsBackup = JSON.parse(JSON.stringify(options));

      // Check that read + writeAtomic performs a correct copy
      let currentDir = await OS.File.getCurrentDirectory();
      let pathSource = OS.Path.join(currentDir, EXISTING_FILE);
      let contents = await OS.File.read(pathSource);
      Assert.ok(!!contents); // Content is not empty
      let bytesRead = contents.byteLength;

      let bytesWritten = await OS.File.writeAtomic(
        DEST_PATH,
        contents,
        options
      );
      Assert.equal(bytesRead, bytesWritten); // Correct number of bytes written

      // Check that options are not altered
      Assert.equal(JSON.stringify(options), JSON.stringify(optionsBackup));
      await reference_compare_files(pathSource, DEST_PATH, TEST);

      // Check that temporary file was removed or never created exist
      Assert.ok(!new FileUtils.File(TMP_PATH).exists());

      // Check that writeAtomic fails if noOverwrite is true and the destination
      // file already exists!
      contents = new Uint8Array(300);
      let view = new Uint8Array(contents.buffer, 10, 200);
      try {
        let opt = JSON.parse(JSON.stringify(options));
        opt.noOverwrite = true;
        await OS.File.writeAtomic(DEST_PATH, view, opt);
        do_throw(
          "With noOverwrite, writeAtomic should have refused to overwrite file (" +
            suffix +
            ")"
        );
      } catch (err) {
        if (err instanceof OS.File.Error && err.becauseExists) {
          info(
            "With noOverwrite, writeAtomic correctly failed (" + suffix + ")"
          );
        } else {
          throw err;
        }
      }
      await reference_compare_files(pathSource, DEST_PATH, TEST);

      // Check that temporary file was removed or never created
      Assert.ok(!new FileUtils.File(TMP_PATH).exists());

      // Now write a subset
      let START = 10;
      let LENGTH = 100;
      contents = new Uint8Array(300);
      for (let i = 0; i < contents.byteLength; i++) {
        contents[i] = i % 256;
      }
      view = new Uint8Array(contents.buffer, START, LENGTH);
      bytesWritten = await OS.File.writeAtomic(DEST_PATH, view, options);
      Assert.equal(bytesWritten, LENGTH);

      let array2 = await OS.File.read(DEST_PATH);
      Assert.equal(LENGTH, array2.length);
      for (let j = 0; j < LENGTH; j++) {
        Assert.equal(array2[j], (j + START) % 256);
      }

      // Cleanup.
      await OS.File.remove(DEST_PATH);
      await OS.File.remove(TMP_PATH);
    })();
  };

  await test_with_options({ tmpPath: TMP_PATH }, "Renaming, not flushing");
  await test_with_options(
    { tmpPath: TMP_PATH, flush: true },
    "Renaming, flushing"
  );
  await test_with_options({}, "Not renaming, not flushing");
  await test_with_options({ flush: true }, "Not renaming, flushing");
});
