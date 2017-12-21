/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {OS: {File, Path, Constants}} = Components.utils.import("resource://gre/modules/osfile.jsm", {});

/**
 * Remove all temporary files and back up files, including
 * test_backupTo_option_with_tmpPath.tmp
 * test_backupTo_option_with_tmpPath.tmp.backup
 * test_backupTo_option_without_tmpPath.tmp
 * test_backupTo_option_without_tmpPath.tmp.backup
 * test_non_backupTo_option.tmp
 * test_non_backupTo_option.tmp.backup
 * test_backupTo_option_without_destination_file.tmp
 * test_backupTo_option_without_destination_file.tmp.backup
 * test_backupTo_option_with_backup_file.tmp
 * test_backupTo_option_with_backup_file.tmp.backup
 */
async function clearFiles() {
  let files = ["test_backupTo_option_with_tmpPath.tmp",
               "test_backupTo_option_without_tmpPath.tmp",
               "test_non_backupTo_option.tmp",
               "test_backupTo_option_without_destination_file.tmp",
               "test_backupTo_option_with_backup_file.tmp"];
  for (let file of files) {
    let path = Path.join(Constants.Path.tmpDir, file);
    await File.remove(path);
    await File.remove(path + ".backup");
  }
}

add_task(async function init() {
  await clearFiles();
});

/**
 * test when
 * |backupTo| specified
 * |tmpPath| specified
 * destination file exists
 * @result destination file will be backed up
 */
add_task(async function test_backupTo_option_with_tmpPath() {
  let DEFAULT_CONTENTS = "default contents" + Math.random();
  let WRITE_CONTENTS = "abc" + Math.random();
  let path = Path.join(Constants.Path.tmpDir,
                       "test_backupTo_option_with_tmpPath.tmp");
  await File.writeAtomic(path, DEFAULT_CONTENTS);
  await File.writeAtomic(path, WRITE_CONTENTS, { tmpPath: path + ".tmp",
                                        backupTo: path + ".backup" });
  Assert.ok((await File.exists(path + ".backup")));
  let contents = await File.read(path + ".backup");
  Assert.equal(DEFAULT_CONTENTS, (new TextDecoder()).decode(contents));
});

/**
 * test when
 * |backupTo| specified
 * |tmpPath| not specified
 * destination file exists
 * @result destination file will be backed up
 */
add_task(async function test_backupTo_option_without_tmpPath() {
  let DEFAULT_CONTENTS = "default contents" + Math.random();
  let WRITE_CONTENTS = "abc" + Math.random();
  let path = Path.join(Constants.Path.tmpDir,
                       "test_backupTo_option_without_tmpPath.tmp");
  await File.writeAtomic(path, DEFAULT_CONTENTS);
  await File.writeAtomic(path, WRITE_CONTENTS, { backupTo: path + ".backup" });
  Assert.ok((await File.exists(path + ".backup")));
  let contents = await File.read(path + ".backup");
  Assert.equal(DEFAULT_CONTENTS, (new TextDecoder()).decode(contents));
});

/**
 * test when
 * |backupTo| not specified
 * |tmpPath| not specified
 * destination file exists
 * @result destination file will not be backed up
 */
add_task(async function test_non_backupTo_option() {
  let DEFAULT_CONTENTS = "default contents" + Math.random();
  let WRITE_CONTENTS = "abc" + Math.random();
  let path = Path.join(Constants.Path.tmpDir,
                       "test_non_backupTo_option.tmp");
  await File.writeAtomic(path, DEFAULT_CONTENTS);
  await File.writeAtomic(path, WRITE_CONTENTS);
  Assert.equal(false, (await File.exists(path + ".backup")));
});

/**
 * test when
 * |backupTo| specified
 * |tmpPath| not specified
 * destination file not exists
 * @result no back up file exists
 */
add_task(async function test_backupTo_option_without_destination_file() {
  let WRITE_CONTENTS = "abc" + Math.random();
  let path = Path.join(Constants.Path.tmpDir,
                       "test_backupTo_option_without_destination_file.tmp");
  await File.remove(path);
  await File.writeAtomic(path, WRITE_CONTENTS, { backupTo: path + ".backup" });
  Assert.equal(false, (await File.exists(path + ".backup")));
});

/**
 * test when
 * |backupTo| specified
 * |tmpPath| not specified
 * backup file exists
 * destination file exists
 * @result destination file will be backed up
 */
add_task(async function test_backupTo_option_with_backup_file() {
  let DEFAULT_CONTENTS = "default contents" + Math.random();
  let WRITE_CONTENTS = "abc" + Math.random();
  let path = Path.join(Constants.Path.tmpDir,
                       "test_backupTo_option_with_backup_file.tmp");
  await File.writeAtomic(path, DEFAULT_CONTENTS);

  await File.writeAtomic(path + ".backup", new Uint8Array(1000));

  await File.writeAtomic(path, WRITE_CONTENTS, { backupTo: path + ".backup" });
  Assert.ok((await File.exists(path + ".backup")));
  let contents = await File.read(path + ".backup");
  Assert.equal(DEFAULT_CONTENTS, (new TextDecoder()).decode(contents));
});

add_task(async function cleanup() {
  await clearFiles();
});
